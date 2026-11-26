// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc.c - C for the firmware functions that check user pointers
 * against the task's address limit
 *
 * The 4.4.94 build inlines access_ok() as a load of addr_limit at 24($gp),
 * a slot 7.x uses for the syscall number. Five functions do it:
 * tisp_vic_ctrl_ioctl, video_input_cmd_set, isp_tunning_read,
 * tisp_code_tuning_ioctl and apical_isp_core_ops_g_ctrl; the last is called
 * only by isp_core_tunning_unlocked_ioctl, which is C here as well. They
 * are C instead, using copy_{from,to}_user(). The firmware's own copies are
 * renamed t23fw_* by the Kbuild so the slot checks can name them.
 *
 * The multi-sensor build (CONFIG_MULTI_SENSOR) has the same five, compiled
 * from its own code: the vic ioctl and video-input-cmd differences are
 * here, its tuning functions are tx-isp-fwc-tun-ms.c.
 *
 * Like the firmware's own handlers they take the 4.4-shaped file twin
 * tx-isp-twin.c builds (private_data at 120, f_flags at 28) and, for the
 * /proc files, the seq twin behind it (private at 72). They replace the
 * entries of the firmware's file_operations tables before tx_isp_init()
 * registers them, each slot checked against the firmware's address.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "tx-isp-abi.h"
#include "tx-isp-twin.h"

/* ---- firmware symbols ------------------------------------------------------ */

extern __printf(2, 3) int isp_printf(int level, const char *fmt, ...);
extern int private_seq_printf(void *seq, const char *fmt, ...);
extern int private_wait_for_completion_interruptible(void *c);
extern void isp_free_buffer(u32 addr);
extern int vic_mdma_enable(void *vic, int unused, int dual, u32 num, u32 addr, int fmt);
extern void tisp_api_mdns_share_mem_en(int en);

extern u8 tispPollValue;
extern u8 st_vic_save_par[40];
extern void *vaddr;
extern char video_input_cmd_buf[128];

/* the firmware's own versions, for the slot checks */
extern char t23fw_tisp_vic_ctrl_ioctl[], t23fw_video_input_cmd_set[];
extern char t23fw_isp_tunning_read[], t23fw_tisp_code_tuning_ioctl[];
extern char t23fw_isp_core_tunning_unlocked_ioctl[];

/* 4.4 file_operations tables in the firmware's .data */
extern void *isp_vic_frd_fops[], *video_input_cmd_fops[];
extern void *isp_core_tunning_fops[], *tisp_fops[];

enum { F44_READ = 2, F44_WRITE = 3, F44_UNLOCKED_IOCTL = 8 };

#define TWIN_PRIV(twin)		FW(twin, FILE44_PRIVATE_DATA, void *)
/* the data pointer of a /proc file's single_open() */
#define TWIN_SEQ_PRIV(twin)	FW(TWIN_PRIV(twin), SEQ44_PRIVATE, void *)

/* ---- /dev/isp-m0 read: the tuning poll flag ---------------------------------- */

static ssize_t fwc_isp_tunning_read(void *twin, char __user *buf, size_t count,
				    loff_t *ppos)
{
	size_t n;

	if (FW(twin, FILE44_F_FLAGS, u32) & O_NONBLOCK)
		return -EAGAIN;
	if (!tispPollValue)
		return -EAGAIN;
	/* the firmware copied count bytes from this one-byte variable */
	n = min_t(size_t, count, sizeof(tispPollValue));
	if (copy_to_user(buf, &tispPollValue, n))
		return -EFAULT;
	tispPollValue = 0;
	return n;
}

/* ---- /proc/jz/isp/isp-w02: the vic parameter ioctl --------------------------- */

/*
 * The vic driver is behind the seq private pointer at +0xd8: +0xe0 height,
 * +0xe4 width, +0x150 the completion the dma-done interrupt signals. The
 * parameter block (40 bytes, at user and in the firmware's st_vic_save_par)
 * is read and written by offset: +0 the dma buffer's address, +8 the frame
 * size, +26 memory mode, +27 what "free" releases, +30 cleared by it, +36
 * the command.
 */
#define VICP_VIC		0xd8
#define VIC_HEIGHT		0xe0
#define VIC_WIDTH		0xe4
#define VIC_DONE		0x150
#define VPAR_ADDR		0
#define VPAR_SIZE		8
#define VPAR_MEMMODE		26
#define VPAR_FREE_WHAT		27
#define VPAR_FREE_FLAG		30
#define VPAR_CMD		36
#define VPAR_BYTES		40
#ifdef CONFIG_MULTI_SENSOR
#define VIC_DONE_MS		0x15c
#define VIC_CMD_ERR_LINE	2141
#else
#define VIC_CMD_ERR_LINE	1724
#endif

#define VIC_IOC_GET		0x20007600
#define VIC_IOC_SET		0x20007601
#define VIC_IOC_TYPE		'v'
#define VIC_IOC_NR_MAX		51

enum { VIC_CMD_IDLE, VIC_CMD_SHARE_MEM, VIC_CMD_FREE, VIC_CMD_MDMA };
enum { VIC_FREE_VADDR, VIC_FREE_ADDR, VIC_FREE_SHARE };

static long fwc_tisp_vic_ctrl_ioctl(void *twin, unsigned int cmd, unsigned long arg)
{
	void *priv = TWIN_SEQ_PRIV(twin);
	u8 *vic = NULL;
	u8 *par = st_vic_save_par;

	if (!IS_ERR_OR_NULL(priv))
		vic = FW(priv, VICP_VIC, u8 *);

	if (((cmd >> 8) & 0xff) != VIC_IOC_TYPE) {
		isp_printf(1, "_IOC_TYPE error !!! cmd is %d\n", cmd);
		return -EINVAL;
	}
	if ((cmd & 0xff) >= VIC_IOC_NR_MAX) {
		isp_printf(1, "_IOC_NR error !!! cmd is %d\n", cmd);
		return -EINVAL;
	}

	switch (cmd) {
	case VIC_IOC_GET:
		/* the firmware read the vic through a NULL pointer here */
		if (!vic)
			return -EINVAL;
		FW(par, VPAR_SIZE, u32) = FW(vic, VIC_HEIGHT, u32) * FW(vic, VIC_WIDTH, u32) * 2;
		if (copy_to_user((void __user *)arg, par, VPAR_BYTES)) {
			isp_printf(2, "get ioctl par copy_to_user error!!!\n");
			return -EFAULT;
		}
		return 0;
	case VIC_IOC_SET:
		break;
	default:
		return -EINVAL;
	}

	/* the firmware's message says copy_to_user for this copy as well */
	if (copy_from_user(par, (void __user *)arg, VPAR_BYTES)) {
		isp_printf(2, "get ioctl par copy_to_user error!!!\n");
		return -EFAULT;
	}
#ifndef CONFIG_MULTI_SENSOR
	printk("mem mode is %d, vic cmd is %d \n", par[VPAR_MEMMODE], FW(par, VPAR_CMD, u32));
#endif

	switch (FW(par, VPAR_CMD, u32)) {
	case VIC_CMD_IDLE:
		break;
	case VIC_CMD_SHARE_MEM:
		tisp_api_mdns_share_mem_en(1);
		mdelay(40);
		break;
	case VIC_CMD_FREE:
		switch (par[VPAR_FREE_WHAT]) {
		case VIC_FREE_VADDR:
			if (vaddr) {
				kfree(vaddr);
				vaddr = NULL;
			}
			break;
		case VIC_FREE_ADDR:
			if (FW(par, VPAR_ADDR, u32)) {
				isp_free_buffer(FW(par, VPAR_ADDR, u32));
				FW(par, VPAR_ADDR, u32) = 0;
			}
			break;
		case VIC_FREE_SHARE:
			tisp_api_mdns_share_mem_en(0);
			break;
		}
		par[VPAR_FREE_FLAG] = 0;
		break;
	case VIC_CMD_MDMA:
		if (!vic)
			return -EINVAL;
#ifdef CONFIG_MULTI_SENSOR
		isp_printf(1, "mem mode is %d, paddr is 0x%08x \n", par[VPAR_MEMMODE],
			   FW(par, VPAR_ADDR, u32));
		vic_mdma_enable(vic, 0, 0, 1, FW(par, VPAR_ADDR, u32), 0);
		private_wait_for_completion_interruptible(vic + VIC_DONE_MS);
#else
		printk("mdns paddr is 0x%08x \n", FW(par, VPAR_ADDR, u32));
		vic_mdma_enable(vic, 0, 0, 1, FW(par, VPAR_ADDR, u32), 0);
		private_wait_for_completion_interruptible(vic + VIC_DONE);
#endif
		break;
	default:
		isp_printf(1, "[%s, %d]: vic cmd error !!!\n", "tisp_vic_cmd_process",
			   VIC_CMD_ERR_LINE);
		return -1;
	}
	FW(par, VPAR_CMD, u32) = 0;
	return 0;
}

/* ---- /proc/jz/isp/video-input-cmd ------------------------------------------- */

/*
 * seq private +0xdc -> +0xe8 the active sensor subdev: +0x90 is handed to
 * its ops, +0xc8 its ops table whose first member is the core ops. The core
 * ops take (sd, request) with the request laid out as struct fw_sensor_op.
 */
#define VIN_ACTIVE		0xdc
#define VIN_ACTIVE_SD		0xe8
#define SD_CLIENT		0x90
#define SD_OPS			0xc8
#define CORE_REG_READ		0x0c
#define CORE_REG_LIST		0x10
#define CORE_REG_ALL		0x14
#define CORE_REG_WRITE		0x18


/* the firmware's struct tx_isp_dbg_register: name, then u64 reg and val */
#ifdef CONFIG_MULTI_SENSOR
struct fw_sensor_op {
	void *client;
	u32 sensor_id;
	u32 size;
	u32 rsvd0;
	u32 reg;
	u32 reg_hi;
	u32 val;
	u32 val_hi;
};
#else
struct fw_sensor_op {
	void *client;
	u32 rsvd0;
	u32 reg;
	u32 flags;
	u32 val;
	u32 rsvd1;
};
#endif

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

#ifdef CONFIG_MULTI_SENSOR
/*
 * The multi-sensor build reads and writes a given sensor's registers as
 * well: "r sen_reg1"/"w sen_reg1" go to the sensor at active +0xec,
 * "r sen_reg2"/"w sen_reg2" to the one at +0xf0, their value text at +11.
 * The plain commands keep going to the active sensor.
 *
 * Up to 128 bytes land in the firmware's static buffer unterminated, so a
 * short command's values are parsed from what is left there from earlier
 * ones, and reading the node shows the old text after the new: both kept.
 * The parse runs on a terminated copy of that buffer so it cannot run off
 * its end.
 */
#define VIN_SENSOR(n)		(0xec + 4 * ((n) - 1))
#define VIN_STATIC		sizeof(video_input_cmd_buf)

static void vin_read(void *seq, u8 *sd, const char *arg, int line)
{
	struct fw_sensor_op op = {};

	op.client = sd + SD_CLIENT;
	op.reg = simple_strtoull(arg, NULL, 0);
	if (!sd)
		isp_printf(2, "##### err %s.%d\n", "video_input_cmd_set", line);
	else
		vin_run(sd, CORE_REG_READ, &op, line);
	private_seq_printf(seq, "isp: sensor reg read 0x%x(0x%x)\n", op.reg, op.val);
	sprintf(video_input_cmd_buf, "0x%x\n", op.val);
}

static void vin_write(void *seq, u8 *sd, char *arg)
{
	struct fw_sensor_op op = {};
	fw_core_op fn = NULL;
	char *end = NULL;

	op.reg = simple_strtoull(arg, &end, 0);
	/* one past the end of the register text, as the firmware does */
	op.val = simple_strtoull(end + 1, NULL, 0);
	private_seq_printf(seq, "isp: sensor reg write 0x%x(0x%x)\n", op.reg, op.val);
	op.client = sd + SD_CLIENT;
	if (sd)
		fn = fw_core_fn(sd, CORE_REG_WRITE);
	sprintf(video_input_cmd_buf, "%s\n", fn && !fn(sd, &op) ? "ok" : "failed");
}

/*
 * The firmware's copy-in: a range outside user space clears the buffer, a
 * fault partway keeps what was copied before it.
 */
static int vin_copy_in(char *dst, const char __user *ubuf, size_t count)
{
	char *tmp;
	size_t left;

	if (!access_ok(ubuf, count)) {
		memset(dst, 0, count);
		return -EFAULT;
	}
	tmp = kmalloc(count, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;
	left = copy_from_user(tmp, ubuf, count);
	memcpy(dst, tmp, count - left);
	kfree(tmp);
	return left ? -EFAULT : 0;
}

static ssize_t fwc_video_input_cmd_set(void *twin, const char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	void *seq = TWIN_PRIV(twin);
	void *vin = FW(seq, SEQ44_PRIVATE, void *);
	bool small = count <= VIN_STATIC;
	struct fw_sensor_op op = {};
	u8 *active = NULL;
	u8 *sd = NULL;
	char *buf;
	ssize_t ret = count;

	if (!IS_ERR_OR_NULL(vin)) {
		active = FW(vin, VIN_ACTIVE, u8 *);
		if (!IS_ERR_OR_NULL(active))
			sd = FW(active, VIN_ACTIVE_SD, u8 *);
	}
	if (IS_ERR_OR_NULL(sd))
		return private_seq_printf(seq,
					  "don't have active sensor, please set sensor firstly!\n");

	/* two terminators: "w sen_reg" parses one past the end of its text */
	buf = kzalloc((small ? VIN_STATIC : count) + 2, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	if (small) {
		ret = vin_copy_in(video_input_cmd_buf, ubuf, count);
		memcpy(buf, video_input_cmd_buf, VIN_STATIC);
	} else if (copy_from_user(buf, ubuf, count)) {
		ret = -EFAULT;
	}
	if (ret < 0)
		goto out;
	ret = count;

	op.client = sd + SD_CLIENT;
	if (!strncmp(buf, "r sen_reg1", 10)) {
		vin_read(seq, FW(active, VIN_SENSOR(1), u8 *), buf + 11, 748);
	} else if (!strncmp(buf, "w sen_reg1", 10)) {
		vin_write(seq, FW(active, VIN_SENSOR(1), u8 *), buf + 11);
	} else if (!strncmp(buf, "w sen_reg2", 10)) {
		vin_write(seq, FW(active, VIN_SENSOR(2), u8 *), buf + 11);
	} else if (!strncmp(buf, "r sen_reg2", 10)) {
		vin_read(seq, FW(active, VIN_SENSOR(2), u8 *), buf + 11, 797);
	} else if (!strncmp(buf, "r sen_reg", 9)) {
		vin_read(seq, sd, buf + 10, 810);
	} else if (!strncmp(buf, "r list", 6)) {
		vin_run(sd, CORE_REG_LIST, &op, 820);
	} else if (!strncmp(buf, "r all", 5)) {
		vin_run(sd, CORE_REG_ALL, &op, 827);
	} else if (!strncmp(buf, "w sen_reg", 9)) {
		vin_write(seq, sd, buf + 10);
	} else {
		sprintf(video_input_cmd_buf, "null");
	}
out:
	kfree(buf);
	return ret;
}

#else
static ssize_t fwc_video_input_cmd_set(void *twin, const char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	void *seq = TWIN_PRIV(twin);
	void *vin = FW(seq, SEQ44_PRIVATE, void *);
	/* room for the terminator the firmware's static path left out */
	bool small = count < sizeof(video_input_cmd_buf);
	struct fw_sensor_op op = {};
	u8 *sd = NULL;
	char *buf, *arg;
	void *active;
	ssize_t ret = count;

	if (!IS_ERR_OR_NULL(vin)) {
		active = FW(vin, VIN_ACTIVE, void *);
		if (!IS_ERR_OR_NULL(active))
			sd = FW(active, VIN_ACTIVE_SD, u8 *);
	}
	if (IS_ERR_OR_NULL(sd))
		return private_seq_printf(seq, "don't have active sensor, please set sensor firstly!\n");

	if (small) {
		buf = video_input_cmd_buf;
	} else {
		buf = kzalloc(count + 1, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
	}
	if (copy_from_user(buf, ubuf, count)) {
		ret = -EFAULT;
		goto out;
	}
	buf[count] = 0;
	arg = buf + min_t(size_t, count, 10);

	op.client = sd + SD_CLIENT;
	if (!strncmp(buf, "r sen_reg", 9)) {
		op.reg = simple_strtoull(arg, NULL, 0);
		vin_run(sd, CORE_REG_READ, &op, 644);
		private_seq_printf(seq, "isp: sensor reg read 0x%x(0x%x)\n", op.reg, op.val);
		sprintf(video_input_cmd_buf, "0x%x\n", op.val);
	} else if (!strncmp(buf, "r list", 6)) {
		vin_run(sd, CORE_REG_LIST, &op, 654);
	} else if (!strncmp(buf, "r all", 5)) {
		vin_run(sd, CORE_REG_ALL, &op, 661);
	} else if (!strncmp(buf, "w sen_reg", 9)) {
		fw_core_op fn;
		char *end = NULL;

		op.reg = simple_strtoull(arg, &end, 0);
		/* the firmware parsed one past the terminator when there was no value */
		op.val = simple_strtoull(*end ? end + 1 : end, NULL, 0);
		private_seq_printf(seq, "isp: sensor reg write 0x%x(0x%x)\n", op.reg, op.val);
		fn = fw_core_fn(sd, CORE_REG_WRITE);
		sprintf(video_input_cmd_buf, "%s\n", fn && !fn(sd, &op) ? "ok" : "failed");
	} else {
		sprintf(video_input_cmd_buf, "null");
	}
out:
	if (!small)
		kfree(buf);
	return ret;
}
#endif

/* ---- install --------------------------------------------------------------- */

struct fwc_slot {
	void **table;
	int index;
	const void *fw;
	void *c;
	const char *name;
};

#define SLOT(t, i, f)	{ t, i, t23fw_##f, fwc_##f, #f }

static const struct fwc_slot fwc_slots[] = {
	SLOT(isp_vic_frd_fops, F44_UNLOCKED_IOCTL, tisp_vic_ctrl_ioctl),
	SLOT(video_input_cmd_fops, F44_WRITE, video_input_cmd_set),
	SLOT(tisp_fops, F44_READ, isp_tunning_read),
	SLOT(tisp_fops, F44_UNLOCKED_IOCTL, tisp_code_tuning_ioctl),
	SLOT(isp_core_tunning_fops, F44_UNLOCKED_IOCTL, isp_core_tunning_unlocked_ioctl),
};

int t23_fwc_install(void)
{
	int i;

	/* all or nothing: a different firmware build fails here */
	for (i = 0; i < ARRAY_SIZE(fwc_slots); i++) {
		if (fwc_slots[i].table[fwc_slots[i].index] != fwc_slots[i].fw) {
			pr_err("t23-isp: %s: unexpected firmware table entry\n",
			       fwc_slots[i].name);
			return -ENOEXEC;
		}
	}
	for (i = 0; i < ARRAY_SIZE(fwc_slots); i++)
		fwc_slots[i].table[fwc_slots[i].index] = fwc_slots[i].c;
	return 0;
}
