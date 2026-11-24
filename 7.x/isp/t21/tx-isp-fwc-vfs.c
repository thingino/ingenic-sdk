// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-vfs.c - C for the two /proc command writers of the firmware,
 * isp_vic_cmd_set (isp-w02) and video_input_cmd_set (isp-w00). Both are
 * single_open() seq files: file->private_data is the seq_file and its
 * private pointer is what the firmware's open stored. See tx-isp-fwc.c for
 * the mechanism that puts them in the firmware's fops tables.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/completion.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "tx-isp-abi.h"

#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

/* ---- firmware symbols ------------------------------------------------------ */

extern __printf(2, 3) int isp_printf(int level, const char *fmt, ...);
extern int private_copy_from_user(void *to, const void __user *from, unsigned long n);
extern void *private_kmalloc(size_t size, unsigned int gfp310);
extern void private_kfree(void *p);
extern int private_seq_printf(struct seq_file *m, const char *fmt, ...);

extern u32 isp_malloc_buffer(u32 size);
extern void isp_free_buffer(u32 addr);
extern int msca_ch_nrvbs;
extern u32 msca_ch_buf_addr;
extern void *main_channel_pad, *second_channel_pad;
extern void tisp_channel_stop_save(void);
extern void tisp_channel_start_restore(void);
extern int tisp_channel_attr_set_crop_scaler(int ch, void *attr);

extern char vic_cmd_buf[32];
extern char video_input_cmd_buf[128];

/* 3.10's GFP_KERNEL, which private_kmalloc translates */
#define FW_GFP_KERNEL	0xd0

/* ---- /proc/jz/isp/isp-w02 (vic) --------------------------------------------
 *
 * seq private +0xd4 -> the vic driver: +0xb8 its register base, +0xdc width,
 * +0xe0 height, +0x13c the snapshot buffer's physical address (0 = none),
 * +0x140 its kernel address, +0x144 the completion the dma-done interrupt
 * signals.
 */
#define VICP_VIC		0xd4
#define VIC_REGS		0xb8
#define VIC_WIDTH		0xdc
#define VIC_HEIGHT		0xe0
#define VIC_SNAP_PHYS		0x13c
#define VIC_SNAP_VIRT		0x140
#define VIC_SNAP_DONE		0x144

/* vic register offsets of the memory-dma engine */
#define VICR_MDMA_CTRL		0x200
#define VICR_MDMA_SIZE		0x204
#define VICR_MDMA_EN		0x208
#define VICR_MDMA_STRIDE	0x214
#define VICR_MDMA_ADDR		0x218
#define MDMA_START		0x80000000

#define VIC_MAX_WIDTH		2688
#define SNAP_WAIT_TRIES		100

/* the register "saveraw" parks at zero while it dumps (the vendor's address) */
#define MSCA_SAVE_REG		((u32 __iomem *)0xb3308964)

static const char vic_func[] = "isp_vic_cmd_set";

/* program the dma for one frame of stride * height bytes into phys */
static void vic_snap_start(u8 *vic, u32 phys, u32 stride)
{
	u8 __iomem *regs = FW(vic, VIC_REGS, u8 __iomem *);

	writel(1, regs + VICR_MDMA_EN);
	writel(FW(vic, VIC_WIDTH, u32) << 16 | FW(vic, VIC_HEIGHT, u32),
	       regs + VICR_MDMA_SIZE);
	writel(stride, regs + VICR_MDMA_STRIDE);
	writel(phys, regs + VICR_MDMA_ADDR);
	writel(MDMA_START, regs + VICR_MDMA_CTRL);
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

static void vic_snap_write(u8 *vic, u32 size)
{
	char name[64];
	struct file *filp;

	snprintf(name, sizeof(name), "/tmp/snap.%s", "raw");
	filp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC, 0766);
	/* the firmware wrote through the result unchecked */
	if (IS_ERR(filp))
		return;
	kernel_write(filp, (void *)(unsigned long)FW(vic, VIC_SNAP_VIRT, u32), size,
		     &filp->f_pos);
	filp_close(filp, NULL);
}

/* grab one frame into the buffer at phys, wait for the dma, write the file */
static bool vic_snap(struct seq_file *m, u8 *vic, u32 phys, u32 size)
{
	FW(vic, VIC_SNAP_VIRT, u32) = (u32)(unsigned long)phys_to_virt(phys);
	vic_snap_start(vic, phys, FW(vic, VIC_WIDTH, u32) << 1);
	if (!vic_snap_wait(vic)) {
		private_seq_printf(m, "snapraw timeout!\n");
		return false;
	}
	vic_snap_write(vic, size);
	return true;
}

/* the common tail: release the buffer, whatever the command did */
static void vic_snap_done(u8 *vic)
{
	u32 phys = FW(vic, VIC_SNAP_PHYS, u32);

	if (phys) {
		isp_free_buffer(phys);
		FW(vic, VIC_SNAP_PHYS, u32) = 0;
	}
}

/*
 * The checks both snapshot commands start with. A non-zero return is the
 * firmware's return value for the write: the seq_printf result.
 */
static bool vic_snap_check(struct seq_file *m, u8 *vic, u32 size, int *ret)
{
	u32 width = FW(vic, VIC_WIDTH, u32);

	isp_printf(1, "width is %d, height is %d, imagesize is %d\n", width,
		   FW(vic, VIC_HEIGHT, u32), size);
	if (width > VIC_MAX_WIDTH) {
		*ret = private_seq_printf(m, "Can't output the width(%d)!\n", width);
		return false;
	}
	if (FW(vic, VIC_SNAP_PHYS, u32)) {
		*ret = private_seq_printf(m, "The node is busy!\n");
		return false;
	}
	return true;
}

static ssize_t vic_cmd_snapraw(struct seq_file *m, u8 *vic, size_t count)
{
	u32 size = (FW(vic, VIC_WIDTH, u32) << 1) * FW(vic, VIC_HEIGHT, u32);
	u32 phys;
	int ret;

	if (!vic_snap_check(m, vic, size, &ret))
		return ret;

	isp_printf(0, "%d:%s:#####\n", 588, vic_func);
	phys = isp_malloc_buffer(size);
	FW(vic, VIC_SNAP_PHYS, u32) = phys;
	isp_printf(0, "%d:%s:#####\n", 590, vic_func);
	if (phys)
		vic_snap(m, vic, phys, size);
	vic_snap_done(vic);
	return count;
}

/*
 * "saveraw": the same into the firmware's mscaler channel-0 buffer, with
 * the register at MSCA_SAVE_REG held at zero meanwhile. The returns before
 * the restore (width, busy, timeout) are the firmware's, as is releasing
 * the channel buffer at the end (isp_free_buffer ignores an address it did
 * not hand out).
 */
static ssize_t vic_cmd_saveraw(struct seq_file *m, u8 *vic, size_t count)
{
	u32 size;
	u32 saved;
	int ret;

	if (msca_ch_nrvbs < 2) {
		isp_printf(1, "nrvbs < 2, channel0 framesource memory no enough!!!!!!!!!\n");
		return -1;
	}
	saved = readl(MSCA_SAVE_REG);
	writel(0, MSCA_SAVE_REG);
	mdelay(10);

	size = (FW(vic, VIC_WIDTH, u32) << 1) * FW(vic, VIC_HEIGHT, u32);
	if (!vic_snap_check(m, vic, size, &ret))
		return ret;

	FW(vic, VIC_SNAP_PHYS, u32) = msca_ch_buf_addr;
	if (msca_ch_buf_addr && !vic_snap(m, vic, msca_ch_buf_addr, size)) {
		vic_snap_done(vic);
		return count;
	}
	writel(saved, MSCA_SAVE_REG);
	vic_snap_done(vic);
	return count;
}

static void vic_cmd_help(void)
{
	isp_printf(1, "\n\n");
	isp_printf(1, "help:\n");
	isp_printf(1, "\t cmd:\n");
	isp_printf(1, "\t  1. snapraw\n");
	isp_printf(1, "\t\t\t use cmd \" snapraw\" you should set ispmem first!!!!!\n");
	isp_printf(1, "\t\t\t please use this cmd: \n\t\"echo snapraw > /proc/jz/isp/isp-w02\"\n");
	isp_printf(1, "\n");
	isp_printf(1, "\t  2. saveraw\n");
	isp_printf(1, "\t\t\t please use this cmd: \n\t\"echo saveraw > /proc/jz/isp/isp-w02\"\n");
	isp_printf(1, "\n\n");
}

ssize_t fwc_isp_vic_cmd_set(struct file *file, const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	void *priv = m->private;
	/* the command lands in a static buffer when it fits, else a heap one */
	bool small = count < sizeof(vic_cmd_buf) + 1;
	u8 *vic = NULL;
	char *buf;
	ssize_t ret;

	if (!IS_ERR_OR_NULL(priv))
		vic = FW(priv, VICP_VIC, u8 *);
	isp_printf(0, "count is %d\n", (int)count);
	if (IS_ERR_OR_NULL(vic))
		return private_seq_printf(m, "Can't ops the node!\n");

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

	if (!strncmp(buf, "snapraw", 7)) {
		ret = vic_cmd_snapraw(m, vic, count);
		goto out;
	}
	if (!strncmp(buf, "saveraw", 7)) {
		ret = vic_cmd_saveraw(m, vic, count);
		goto out;
	}
	if (!strncmp(buf, "help", 4))
		vic_cmd_help();
	vic_snap_done(vic);
	ret = count;
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
#define CORE_REG_WRITE		0x10

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

/*
 * "mscaler <ch>+<scaler>+<crop>..." (the text after the command word): nine
 * numbers split at '+'. The channel pad is the firmware's: +0 the channel
 * device (+0xd4 its attributes, +0x124 and +0x128 the output size there),
 * +4 the channel index. attr is the request tisp_channel_attr_set_crop_scaler
 * takes.
 */
#define MS_PAD_DEV		0
#define MS_PAD_INDEX		4
#define MS_DEV_ATTR		0xd4
#define MS_ATTR_OUT_W		0x124
#define MS_ATTR_OUT_H		0x128
#define MS_FIELDS		9

struct mscaler_attr {
	u32 scaler;
	u32 out_w;
	u32 out_h;
	u32 crop;
	u32 crop_x;
	u32 crop_y;
	u32 crop_w;
	u32 crop_h;
};

static int vin_mscaler(char *s)
{
	char *tok[MS_FIELDS];
	unsigned long v[MS_FIELDS];
	struct mscaler_attr attr;
	u8 *pad, *dev, *chan = NULL;
	int i, n = 1;

	/* the firmware let the split run off its array and read unset entries */
	tok[0] = s;
	for (i = 1; i < MS_FIELDS; i++)
		tok[i] = "";
	while (n < MS_FIELDS) {
		char *plus = strchr(s, '+');

		if (!plus)
			break;
		*plus = 0;
		s = plus + 1;
		tok[n++] = s;
	}
	for (i = 0; i < MS_FIELDS; i++)
		v[i] = simple_strtoul(tok[i], NULL, 0);

	pad = v[0] ? second_channel_pad : main_channel_pad;
	if (!IS_ERR_OR_NULL(pad)) {
		dev = FW(pad, MS_PAD_DEV, u8 *);
		if (!IS_ERR_OR_NULL(dev))
			chan = FW(dev, MS_DEV_ATTR, u8 *);
	}
	/* the firmware dereferenced these unchecked */
	if (IS_ERR_OR_NULL(chan))
		return -EINVAL;

	tisp_channel_stop_save();
	attr.scaler = v[6];
	if (v[6]) {
		attr.out_w = v[7];
		attr.out_h = v[8];
	} else {
		attr.out_w = FW(chan, MS_ATTR_OUT_W, u32);
		attr.out_h = FW(chan, MS_ATTR_OUT_H, u32);
	}
	attr.crop = v[1];
	if (v[1] == 1) {
		attr.crop_x = v[3];
		attr.crop_y = v[2];
		attr.crop_w = v[4];
		attr.crop_h = v[5];
	} else {
		attr.crop_x = 0;
		attr.crop_y = 0;
		attr.crop_w = attr.out_w;
		attr.crop_h = attr.out_h;
	}
	tisp_channel_attr_set_crop_scaler(FW(pad, MS_PAD_INDEX, u8), &attr);
	tisp_channel_start_restore();
	return 0;
}

ssize_t fwc_video_input_cmd_set(struct file *file, const char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	void *vin = m->private;
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
		return private_seq_printf(m, "don't have active sensor, please set sensor firstly!\n");

	if (small) {
		buf = video_input_cmd_buf;
	} else {
		buf = private_kmalloc(count + 1, FW_GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
		memset(buf, 0, count + 1);
	}
	if (private_copy_from_user(buf, ubuf, count)) {
		ret = -EFAULT;
		goto out;
	}
	buf[count] = 0;
	arg = buf + min_t(size_t, count, 10);

	op.client = sd + SD_CLIENT;
	if (!strncmp(buf, "r sen_reg", 9)) {
		fw_core_op fn = fw_core_fn(sd, CORE_REG_READ);

		op.reg = simple_strtoull(arg, NULL, 0);
		if (!fn || fn(sd, &op))
			isp_printf(2, "##### err %s.%d\n", "video_input_cmd_set", 629);
		private_seq_printf(m, "isp: sensor reg read 0x%x(0x%x)\n", op.reg, op.val);
		sprintf(video_input_cmd_buf, "0x%x\n", op.val);
	} else if (!strncmp(buf, "w sen_reg", 9)) {
		fw_core_op fn;
		char *end = NULL;

		op.reg = simple_strtoull(arg, &end, 0);
		/* the firmware parsed one past the terminator when there was no value */
		op.val = simple_strtoull(*end ? end + 1 : end, NULL, 0);
		private_seq_printf(m, "isp: sensor reg write 0x%x(0x%x)\n", op.reg, op.val);
		fn = fw_core_fn(sd, CORE_REG_WRITE);
		sprintf(video_input_cmd_buf, "%s\n", fn && !fn(sd, &op) ? "ok" : "failed");
	} else if (!strncmp(buf, "mscaler", 7)) {
		char *s = private_kmalloc(count + 1, FW_GFP_KERNEL);
		size_t n = count > 8 ? count - 8 : 0;

		if (!s) {
			ret = -ENOMEM;
		} else {
			memset(s, 0, count + 1);
			/* the firmware copied count bytes from past the user's data */
			if (private_copy_from_user(s, ubuf + 8, n))
				ret = -EFAULT;
			else
				ret = vin_mscaler(s) ?: count;
			private_kfree(s);
		}
	} else {
		sprintf(video_input_cmd_buf, "null");
	}
out:
	if (!small)
		private_kfree(buf);
	return ret;
}
