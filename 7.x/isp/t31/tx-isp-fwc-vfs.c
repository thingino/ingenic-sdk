// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-vfs.c - C replacements for firmware functions that touch kernel
 * file/seq_file objects through 3.10 offsets the compiler inlined: the two
 * /proc command files (isp_vic_cmd_set, video_input_cmd_set) and the IQ bin
 * loader (tiziano_load_parameters). See tx-isp-fwc.c for the mechanism.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "tx-isp-abi.h"

#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

/* ---- firmware symbols ------------------------------------------------------ */

extern __printf(2, 3) int isp_printf(int level, const char *fmt, ...);
extern int private_copy_from_user(void *to, const void __user *from, unsigned long n);
extern void *private_kmalloc(size_t size, unsigned int gfp310);
extern void private_kfree(void *p);
extern void *private_vmalloc(unsigned long size);
extern void private_vfree(const void *p);

extern u32 isp_malloc_buffer(u32 size);
extern void isp_free_buffer(u32 addr);
extern u32 system_reg_read(u32 reg);
extern void system_reg_write(u32 reg, u32 val);
extern int vic_mdma_enable(void *vic, int unused, int dual, u32 num, u32 addr, int fmt);

extern char vic_cmd_buf[32];
extern char video_input_cmd_buf[128];
extern const u32 crc_table[8];
extern void *tparams_day, *tparams_night, *tparams_cust;
extern char sensor_info[];

/* the firmware's own versions, for the slot checks */
extern char t31fw_isp_vic_cmd_set[], t31fw_video_input_cmd_set[];

/* 3.10 file_operations tables in the firmware's .data */
extern void *isp_vic_frd_fops[], *video_input_cmd_fops[];

enum { F310_WRITE = 3 };

/* 3.10's GFP_KERNEL, which private_kmalloc translates */
#define FW_GFP_KERNEL	0xd0

/*
 * Both /proc files are single_open() seq files: file->private_data is the
 * seq_file and its private pointer is what the firmware's open stored.
 */

/* ---- /proc/jz/isp/isp-w02 (vic) -------------------------------------------- */

/*
 * seq private +0xd4 -> the vic driver: +0xdc width, +0xe0 height, +0x110 the
 * sensor subdev (+0x7c pixel format, +0x90 dual flag), +0x140 the snapshot
 * buffer's physical address (0 = none), +0x144 its kernel address, +0x148
 * the completion the dma-done interrupt signals.
 */
#define VICP_VIC		0xd4
#define VIC_WIDTH		0xdc
#define VIC_HEIGHT		0xe0
#define VIC_SENSOR		0x110
#define VIC_SNAP_PHYS		0x140
#define VIC_SNAP_VIRT		0x144
#define VIC_SNAP_DONE		0x148
#define SENSOR_FMT		0x7c
#define SENSOR_DUAL		0x90

#define FMT_NV12		7
#define VIC_MAX_WIDTH		2688
#define SNAP_WAIT_TRIES		600

/* vic registers saved around "saveraw" */
#define VICREG_A		0x7810
#define VICREG_B		0x7814
#define VICREG_C		0x7804
#define VICREG_BUF		0x7820

static u32 vic_frame_bytes(u8 *vic)
{
	u32 w = FW(vic, VIC_WIDTH, u32);

	if (FW(FW(vic, VIC_SENSOR, u8 *), SENSOR_FMT, int) == FMT_NV12)
		w = (w * 3) >> 1;
	else
		w <<= 1;
	return w * FW(vic, VIC_HEIGHT, u32);
}

static void vic_snap_start(u8 *vic, u32 num, u32 addr)
{
	u8 *sensor = FW(vic, VIC_SENSOR, u8 *);
	int fmt = FW(sensor, SENSOR_FMT, int);
	int dual = fmt != FMT_NV12 && FW(sensor, SENSOR_DUAL, int);

	vic_mdma_enable(vic, 0, dual, num, addr, fmt == FMT_NV12 ? FMT_NV12 : 0);
}

static bool vic_snap_wait(u8 *vic)
{
	struct completion *done = &FW(vic, VIC_SNAP_DONE, struct completion);
	int tries = SNAP_WAIT_TRIES;

	while (wait_for_completion_interruptible(done) < 0)
		if (!--tries)
			return false;
	return true;
}

static void vic_snap_write(u8 *vic, u32 num, u32 size)
{
	u8 *base = (u8 *)(unsigned long)FW(vic, VIC_SNAP_VIRT, u32);
	char name[64];
	u32 i;

	for (i = 0; i < num; i++) {
		const char *ext = FW(FW(vic, VIC_SENSOR, u8 *), SENSOR_FMT, int) == FMT_NV12 ?
				  "nv12" : "raw";
		struct file *filp;

		snprintf(name, sizeof(name), "/tmp/snap%d.%s", i, ext);
		filp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC, 0766);
		if (IS_ERR(filp))
			continue;
		kernel_write(filp, base + i * size, size, &filp->f_pos);
		filp_close(filp, NULL);
	}
}

static ssize_t vic_snap_done(u8 *vic, size_t count)
{
	u32 addr = FW(vic, VIC_SNAP_PHYS, u32);

	if (addr) {
		isp_free_buffer(addr);
		FW(vic, VIC_SNAP_PHYS, u32) = 0;
	}
	return count;
}

static void vic_snap_arm(u8 *vic, u32 addr)
{
	FW(vic, VIC_SNAP_PHYS, u32) = addr;
	FW(vic, VIC_SNAP_VIRT, u32) = (u32)(unsigned long)phys_to_virt(addr);
}

/* "snapraw [n]": grab n frames into ispmem and write /tmp/snap<i>.<ext> */
static ssize_t vic_cmd_snapraw(struct seq_file *m, u8 *vic, const char *buf, size_t count)
{
	u32 num = simple_strtoull(buf + 8, NULL, 0);
	u32 size, total, width, addr;

	if (num < 2 || count == 8)
		num = 1;
	size = vic_frame_bytes(vic);
	total = num * size;
	isp_printf(1, "width is %d, height is %d, imagesize is %d\n, snap num is %d, buf size is %d",
		   FW(vic, VIC_WIDTH, u32), FW(vic, VIC_HEIGHT, u32), size, num, total);

	width = FW(vic, VIC_WIDTH, u32);
	if (width > VIC_MAX_WIDTH)
		return t31_seq_printf_310(m, "Can't output the width(%d)!\n", width);
	if (FW(vic, VIC_SNAP_PHYS, u32))
		return t31_seq_printf_310(m, "The node is busy!\n");

	addr = isp_malloc_buffer(total);
	FW(vic, VIC_SNAP_PHYS, u32) = addr;
	if (!addr)
		return count;

	vic_snap_arm(vic, addr);
	vic_snap_start(vic, num, addr);
	if (!vic_snap_wait(vic)) {
		t31_seq_printf_310(m, "snapraw timeout!\n");
		return vic_snap_done(vic, count);
	}
	vic_snap_write(vic, num, size);
	return vic_snap_done(vic, count);
}

/*
 * "saveraw [n]": the same, but dma into the buffer the vic register at
 * 0x7820 points at, with three vic registers masked meanwhile. The error
 * returns before the restore are the firmware's.
 */
static ssize_t vic_cmd_saveraw(struct seq_file *m, u8 *vic, const char *buf, size_t count)
{
	u32 reg_a = system_reg_read(VICREG_A);
	u32 reg_b = system_reg_read(VICREG_B);
	u32 reg_c = system_reg_read(VICREG_C);
	u32 addr = system_reg_read(VICREG_BUF);
	u32 num, size, total, width;

	system_reg_write(VICREG_A, reg_a & 0x11110111);
	system_reg_write(VICREG_B, 0);
	/* the firmware restores reg_c | 1, not the value it read */
	reg_c |= 1;
	system_reg_write(VICREG_C, reg_c);
	mdelay(10);

	num = simple_strtoull(buf + 8, NULL, 0);
	if (num < 2 || count == 8)
		num = 1;
	size = vic_frame_bytes(vic);
	total = num * size;
	isp_printf(1, "width is %d, height is %d, imagesize is %d\n, save num is %d, buf size is %d",
		   FW(vic, VIC_WIDTH, u32), FW(vic, VIC_HEIGHT, u32), size, num, total);

	width = FW(vic, VIC_WIDTH, u32);
	if (width > VIC_MAX_WIDTH)
		return t31_seq_printf_310(m, "Can't output the width(%d)!\n", width);
	if (FW(vic, VIC_SNAP_PHYS, u32))
		return t31_seq_printf_310(m, "The node is busy!\n");

	FW(vic, VIC_SNAP_PHYS, u32) = addr;
	if (addr) {
		vic_snap_arm(vic, addr);
		vic_snap_start(vic, num, addr);
		if (!vic_snap_wait(vic)) {
			t31_seq_printf_310(m, "snapraw timeout!\n");
			return vic_snap_done(vic, count);
		}
		vic_snap_write(vic, num, size);
	}
	system_reg_write(VICREG_A, reg_a & 0x11111111);
	system_reg_write(VICREG_B, reg_b);
	system_reg_write(VICREG_C, reg_c);
	return vic_snap_done(vic, count);
}

static void vic_cmd_help(void)
{
	isp_printf(1, "\n\n");
	isp_printf(1, "help:\n");
	isp_printf(1, "\t cmd:\n");
	isp_printf(1, "\t\t snapraw\n");
	isp_printf(1, "\t\t\t use cmd \" snapraw\" you should set ispmem first!!!!!\n");
	isp_printf(1, "\t\t\t please use this cmd: \n\t\"echo snapraw savenum > /proc/jz/isp/isp-w02\"\n");
	isp_printf(1, "\t\t\t \"snapraw\"  is cmd; \n");
	isp_printf(1, "\t\t\t \"savenum\" is the num of you save raw picture.\n ");
	isp_printf(1, "\t\t saveraw\n");
	isp_printf(1, "\t\t\t please use this cmd: \n\t\"echo saveraw savenum > /proc/jz/isp/isp-w02\"\n");
	isp_printf(1, "\t\t\t \"saveraw\"  is cmd; \n");
	isp_printf(1, "\t\t\t \"savenum\" is the num of you save raw picture.\n ");
	isp_printf(1, "\n\n");
}

static ssize_t fwc_isp_vic_cmd_set(struct file *file, const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	void *priv = m->private;
	/* the command lands in a static buffer when it fits, else a heap one */
	bool small = count < sizeof(vic_cmd_buf);
	u8 *vic = NULL;
	char *buf;
	ssize_t ret;

	if (!IS_ERR_OR_NULL(priv))
		vic = FW(priv, VICP_VIC, u8 *);
	isp_printf(0, "count is %d\n", (int)count);
	if (IS_ERR_OR_NULL(vic))
		return t31_seq_printf_310(m, "Can't ops the node!\n");

	if (small) {
		buf = vic_cmd_buf;
	} else {
		buf = private_kmalloc(count + 1, FW_GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
	}
	if (private_copy_from_user(buf, ubuf, count)) {
		ret = -EFAULT;
		goto out;
	}
	/* the firmware left the heap buffer unterminated for the number parse */
	if (!small)
		buf[count] = 0;

	if (!strncmp(buf, "snapraw", 7)) {
		ret = vic_cmd_snapraw(m, vic, buf, count);
	} else if (!strncmp(buf, "saveraw", 7)) {
		ret = vic_cmd_saveraw(m, vic, buf, count);
	} else {
		if (!strncmp(buf, "help", 4))
			vic_cmd_help();
		ret = vic_snap_done(vic, count);
	}
out:
	if (!small)
		private_kfree(buf);
	return ret;
}

/* ---- /proc/jz/isp/video-input-cmd ------------------------------------------ */

/*
 * seq private +0xd8 -> +0xe4 the active sensor subdev: +0x8c is handed to its
 * ops, +0xc4 its ops table whose first member is the core ops. The core ops
 * take (sd, request) with the request laid out as struct fw_sensor_op.
 */
#define VIN_ACTIVE		0xd8
#define VIN_ACTIVE_SD		0xe4
#define SD_CLIENT		0x8c
#define SD_OPS			0xc4
#define CORE_REG_READ		0x0c
#define CORE_REG_LIST		0x10
#define CORE_REG_ALL		0x14
#define CORE_REG_WRITE		0x18

struct fw_sensor_op {
	void *client;
	u32 rsvd0;
	u32 reg;
	u32 flags;
	u32 val;
	u32 rsvd1;
};

typedef int (*fw_core_op)(void *sd, struct fw_sensor_op *op);

static fw_core_op fw_core_fn(u8 *sd, int off)
{
	u8 **ops = FW(sd, SD_OPS, u8 **);
	u8 *core = ops ? ops[0] : NULL;

	return core ? FW(core, off, fw_core_op) : NULL;
}

static void vin_run(u8 *sd, int off, struct fw_sensor_op *op, int line)
{
	fw_core_op fn = fw_core_fn(sd, off);

	if (!fn || fn(sd, op))
		isp_printf(2, "##### err %s.%d\n", "video_input_cmd_set", line);
}

static ssize_t fwc_video_input_cmd_set(struct file *file, const char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	void *vin = m->private;
	bool small = count < sizeof(video_input_cmd_buf);
	struct fw_sensor_op op = {};
	u8 *sd = NULL;
	char *buf;
	void *active;
	ssize_t ret = count;

	if (!IS_ERR_OR_NULL(vin)) {
		active = FW(vin, VIN_ACTIVE, void *);
		if (!IS_ERR_OR_NULL(active))
			sd = FW(active, VIN_ACTIVE_SD, u8 *);
	}
	if (IS_ERR_OR_NULL(sd))
		return t31_seq_printf_310(m, "don't have active sensor, please set sensor firstly!\n");

	if (small) {
		buf = video_input_cmd_buf;
	} else {
		buf = private_kmalloc(count + 1, FW_GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
		memset(buf, 0, count + 1);
	}
	if (copy_from_user(buf, ubuf, count)) {
		ret = -EFAULT;
		goto out;
	}

	op.client = sd + SD_CLIENT;
	if (!strncmp(buf, "r sen_reg", 9)) {
		op.reg = simple_strtoull(buf + 10, NULL, 0);
		vin_run(sd, CORE_REG_READ, &op, 624);
		t31_seq_printf_310(m, "isp: sensor reg read 0x%x(0x%x)\n", op.reg, op.val);
		sprintf(video_input_cmd_buf, "0x%x\n", op.val);
	} else if (!strncmp(buf, "r list", 6)) {
		vin_run(sd, CORE_REG_LIST, &op, 634);
	} else if (!strncmp(buf, "r all", 5)) {
		vin_run(sd, CORE_REG_ALL, &op, 641);
	} else if (!strncmp(buf, "w sen_reg", 9)) {
		fw_core_op fn;
		char *end = NULL;

		op.reg = simple_strtoull(buf + 10, &end, 0);
		/* the firmware parsed one past the terminator when there was no value */
		op.val = simple_strtoull(*end ? end + 1 : end, NULL, 0);
		t31_seq_printf_310(m, "isp: sensor reg write 0x%x(0x%x)\n", op.reg, op.val);
		fn = fw_core_fn(sd, CORE_REG_WRITE);
		sprintf(video_input_cmd_buf, "%s\n", fn && !fn(sd, &op) ? "ok" : "failed");
	} else {
		sprintf(video_input_cmd_buf, "null");
	}
out:
	if (!small)
		private_kfree(buf);
	return ret;
}

/* ---- tiziano_load_parameters ----------------------------------------------- */

/*
 * The sensor IQ bin: +0 version "2.10", +8 "header0", +0x10 the length of
 * the CRC-covered data, +0x14 the CRC, +0x18 the data: the day parameters
 * then the night ones, 0x137f0 bytes each (the cust bin carries one set).
 */
#define TPARAM_SET		0x137f0
#define TPARAM_HDR		0x18
#define TPARAM_MIN_SIZE		(TPARAM_HDR + 2 * TPARAM_SET)
#define TPARAM_CUST_MIN_SIZE	(TPARAM_HDR + TPARAM_SET)
#define SINFO_NAME		12

struct tparam_mgr {
	char version[8];
	char header[8];
	u32 len;
	u32 crc;
	u32 rsvd[4];
	u8 *buf;
	u32 size;
	u8 *data;
};

static bool tparam_crc_ok(const u8 *bin, u32 size, struct tparam_mgr *mgr)
{
	const u32 *w = (const u32 *)(bin + TPARAM_HDR);
	const u32 *end;
	u32 crc = 0;

	mgr->len = FW(bin, 0x10, u32);
	mgr->crc = FW(bin, 0x14, u32);
	/* the length comes from the file: keep the walk inside the buffer */
	if (mgr->len > size - TPARAM_HDR)
		return false;
	for (end = w + mgr->len / 4; w != end; w++) {
		crc ^= *w;
		crc ^= crc_table[crc & 7];
	}
	return crc == mgr->crc;
}

static void tparam_free(struct tparam_mgr *mgr)
{
	private_vfree(mgr->buf);
	private_vfree(mgr);
}

int tiziano_load_parameters(char *name);

int tiziano_load_parameters(char *name)
{
	struct tparam_mgr *mgr;
	struct file *filp;
	char path[64];
	loff_t isize;
	u32 size;

	mgr = private_vmalloc(sizeof(*mgr));
	if (!mgr) {
		isp_printf(2, "Failed to private_kmalloc TISPParamManage\n");
		return -1;
	}
	memset(mgr, 0, sizeof(*mgr));
	snprintf(mgr->version, sizeof(mgr->version), "%s", "2.10");
	snprintf(mgr->header, sizeof(mgr->header), "header%d", 0);

	/* the firmware sprintf()ed the name as a format into path */
	strscpy(path, name, sizeof(path));
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		isp_printf(2, "ISP: open %s file for isp calibrate read failed\n", path);
		private_vfree(mgr);
		return -1;
	}

	isize = i_size_read(file_inode(filp));
	size = isize;
	mgr->buf = private_vmalloc(size);
	if (!mgr->buf) {
		isp_printf(2, "%s[%d]: Failed to alloc %lld KB buffer!\n",
			   "tiziano_load_parameters", 856, isize >> 10);
		goto err_close;
	}
	mgr->size = size;
	if (isize > U32_MAX || size < TPARAM_MIN_SIZE) {
		isp_printf(2, "The size of file has been modify!\n");
		goto err_close;
	}
	kernel_read(filp, mgr->buf, size, &filp->f_pos);
	filp_close(filp, NULL);

	if (strncmp(mgr->version, mgr->buf, 8)) {
		isp_printf(2, "####################################################################\n");
		isp_printf(2, "#### The version of bin doesn't match with driver! ####\n");
		isp_printf(2, "#### The version of bin is %.8s, the driver is %s ####\n",
			   mgr->buf, mgr->version);
		isp_printf(2, "####################################################################\n");
		goto err;
	}
	if (strncmp(mgr->header, mgr->buf + 8, 8)) {
		isp_printf(2, "####################################################################\n");
		isp_printf(2, "#### The first flag of bin doesn't match with driver! ####\n");
		isp_printf(2, "####################################################################\n");
		goto err;
	}
	if (!tparam_crc_ok(mgr->buf, size, mgr)) {
		isp_printf(2, "%s[%d]: Failed to CRC sensor setting!\n",
			   "tiziano_load_parameters", 896);
		goto err;
	}
	mgr->data = mgr->buf + TPARAM_HDR;
	memcpy(tparams_day, mgr->data, TPARAM_SET);
	memcpy(tparams_night, mgr->buf + TPARAM_HDR + TPARAM_SET, TPARAM_SET);

	/* the optional customer bin reuses the buffer, so it can only be smaller */
	snprintf(path, sizeof(path), "/etc/sensor/%s-cust-t31.bin", sensor_info + SINFO_NAME);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		isp_printf(0, "ISP: no %s for cust mode isp calibrate\n", path);
		goto done;
	}
	isize = i_size_read(file_inode(filp));
	if (isize > mgr->size || isize < TPARAM_CUST_MIN_SIZE) {
		isp_printf(2, "The size of cust file has been modify!\n");
		goto err_close;
	}
	size = isize;
	kernel_read(filp, mgr->buf, size, &filp->f_pos);
	filp_close(filp, NULL);

	if (strncmp(mgr->version, mgr->buf, 8)) {
		isp_printf(2, "####################################################################\n");
		isp_printf(2, "#### The version of cust bin doesn't match with driver! ####\n");
		isp_printf(2, "#### The version of cust bin is %.8s, the driver is %s ####\n",
			   mgr->buf, mgr->version);
		isp_printf(2, "####################################################################\n");
		goto err;
	}
	if (strncmp(mgr->header, mgr->buf + 8, 8)) {
		isp_printf(2, "####################################################################\n");
		isp_printf(2, "#### The first flag of cust bin doesn't match with driver! ####\n");
		isp_printf(2, "####################################################################\n");
		goto err;
	}
	if (!tparam_crc_ok(mgr->buf, size, mgr)) {
		isp_printf(2, "%s[%d]: Failed to CRC sensor cust setting!\n",
			   "tiziano_load_parameters", 963);
		goto err;
	}
	mgr->data = mgr->buf + TPARAM_HDR;
	tparams_cust = private_vmalloc(TPARAM_SET);
	if (!tparams_cust)
		goto err;
	memset(tparams_cust, 0, TPARAM_SET);
	memcpy(tparams_cust, mgr->data, TPARAM_SET);
done:
	tparam_free(mgr);
	return 0;

err_close:
	filp_close(filp, NULL);
err:
	tparam_free(mgr);
	return -1;
}

/* ---- install --------------------------------------------------------------- */

struct fwc_vfs_slot {
	void **table;
	const void *fw;
	void *c;
	const char *name;
};

#define VSLOT(t, f)	{ t, t31fw_##f, fwc_##f, #f }

static const struct fwc_vfs_slot fwc_vfs_slots[] = {
	VSLOT(isp_vic_frd_fops, isp_vic_cmd_set),
	VSLOT(video_input_cmd_fops, video_input_cmd_set),
};

int t31_fwc_vfs_install(void)
{
	int i;

	/* all or nothing: a different firmware build fails here */
	for (i = 0; i < ARRAY_SIZE(fwc_vfs_slots); i++) {
		if (fwc_vfs_slots[i].table[F310_WRITE] != fwc_vfs_slots[i].fw) {
			pr_err("t31-isp: %s: unexpected firmware table entry\n",
			       fwc_vfs_slots[i].name);
			return -ENOEXEC;
		}
	}
	for (i = 0; i < ARRAY_SIZE(fwc_vfs_slots); i++)
		fwc_vfs_slots[i].table[F310_WRITE] = fwc_vfs_slots[i].c;
	return 0;
}
