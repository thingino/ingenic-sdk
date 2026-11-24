// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-txioctl.c - C for the firmware's /dev/tx-isp ioctl
 *
 * tx_isp_unlocked_ioctl is the unlocked_ioctl slot of the firmware's 3.10
 * tx_isp_fops. The firmware version reads file->private_data at its 3.10
 * offset; this one goes through the 7.x struct file. Everything else it
 * touches is the firmware's own: private_data is the ispdev miscdevice (see
 * tx-isp-fwc.c), and every user copy goes through the firmware's
 * private_copy_{from,to}_user.
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/err.h>

#include "tx-isp-abi.h"

#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

/* ---- firmware symbols ------------------------------------------------------ */

extern int isp_printf(int level, const char *fmt, ...);
extern int private_copy_from_user(void *to, const void __user *from, unsigned long n);
extern int private_copy_to_user(void __user *to, const void *from, unsigned long n);
extern void system_reg_write(u32 reg, u32 val);

/* local functions and data of the blob */
extern int tx_isp_video_s_stream(void *ispdev, int enable);
extern int tx_isp_video_link_stream(void *ispdev, int enable);
extern void *find_subdev_link_pad(void *ispdev, const void *pad_name);
extern int subdev_video_destroy_link(void *link);
extern int tx_isp_video_link_destroy(void *ispdev) asm("tx_isp_video_link_destroy.isra.1");
extern const u8 link1[3][0x14];

/*
 * The firmware returns -ENOIOCTLCMD (-515) from subdev ops it does not
 * implement; callers treat it as success.
 */
#define FW_ENOIOCTLCMD	(-515)

/* ---- ispdev (file->private_data) and subdev layout ----------------------- */

#define ISPDEV_SUBDEV_FIRST	0x2c
#define ISPDEV_SUBDEV_END	0x6c
#define ISPDEV_LINK		0x10c
#define ISPDEV_TO_MISC		12	/* private_data = ispdev + 12 */

#define SD_OPS			0xc4	/* ops: sensor ops at +0xc, its ioctl at +8 */
#define SD_OPS_SENSOR		0xc
#define SENSOR_IOCTL		0x8
#define SD_PRIV			0xd4

/* the first subdev's private state */
#define PRIV_FRAME_W		0xec
#define PRIV_FRAME_H		0xf0

/* link pads: +6 type mask, +7 state, +8 the link {self, peer, peer link, flags} */
#define PAD_TYPE		6
#define PAD_STATE		7
#define PAD_LINK		8
#define PAD_PEER		0xc
#define PAD_PEER_LINK		0x10
#define PAD_FLAGS		0x14
#define PAD_STATE_LINKED	3
#define PAD_STATE_STREAMING	4

/* a link1[] entry: two pad names (8 bytes each) and the link type mask */
#define LINKCFG_DST		8
#define LINKCFG_TYPE		0x10

/* subdev ioctl commands the ioctls forward */
#define SD_EV_REGISTER_SENSOR	0x2000000
#define SD_EV_RELEASE_SENSOR	0x2000001
#define SD_EV_ENUM_INPUT	0x2000002
#define SD_EV_GET_INPUT		0x2000003
#define SD_EV_SET_INPUT		0x2000004
#define SD_EV_S_REGISTER	0x200000f
#define SD_EV_G_REGISTER	0x2000010

/* ioctl commands */
#define TXIO_VIDEO_S_STREAM_ON	0x80045612
#define TXIO_VIDEO_S_STREAM_OFF	0x80045613
#define TXIO_ENUM_INPUT		0xc050561a
#define TXIO_GET_INPUT		0x40045626
#define TXIO_SET_INPUT		0xc0045627
#define TXIO_S_REGISTER		0x8038564f
#define TXIO_G_REGISTER		0xc0385650
#define TXIO_REGISTER_SENSOR	0x805056c1
#define TXIO_RELEASE_SENSOR	0x805056c2
#define TXIO_LINK_SETUP		0x800456d0
#define TXIO_LINK_DESTROY	0x800456d1
#define TXIO_LINK_STREAM_ON	0x800456d2
#define TXIO_LINK_STREAM_OFF	0x800456d3
#define TXIO_SET_BUF		0x800856d4
#define TXIO_GET_BUF		0x800856d5

typedef int (*fw_sd_ioctl)(void *sd, unsigned int cmd, void *arg);

#define COPY_FROM_ERR(fn, line) \
	isp_printf(2, "[%s][%d] copy from user error\n", fn, line)
#define COPY_TO_ERR(fn, line) \
	isp_printf(2, "[%s][%d] copy to user error\n", fn, line)

/*
 * Pass cmd to the sensor ops ioctl of every subdev that has one. The first
 * real error stops the walk; -ENOIOCTLCMD does not.
 */
static int fwc_sensor_ioctl_all(u8 *dev, unsigned int cmd, void *arg)
{
	int off;

	for (off = ISPDEV_SUBDEV_FIRST; off < ISPDEV_SUBDEV_END; off += 4) {
		void *sd = FW(dev, off, void *);
		u8 *ops, *sensor_ops;
		fw_sd_ioctl fn;
		int ret;

		if (!sd)
			continue;
		/* the firmware did not check the ops table */
		ops = FW(sd, SD_OPS, u8 *);
		if (!ops)
			continue;
		sensor_ops = FW(ops, SD_OPS_SENSOR, u8 *);
		if (!sensor_ops)
			continue;
		fn = FW(sensor_ops, SENSOR_IOCTL, fw_sd_ioctl);
		if (!fn)
			continue;
		ret = fn(sd, cmd, arg);
		if (ret && ret != FW_ENOIOCTLCMD)
			return ret;
	}
	return 0;
}

/* ---- sensor input ioctls ---------------------------------------------------- */

static int fwc_enum_input(u8 *dev, unsigned long arg)
{
	u32 buf[20];
	int ret;

	if (private_copy_from_user(buf, (void __user *)arg, 80)) {
		COPY_FROM_ERR("tx_isp_sensor_enum_input", 314);
		return -EFAULT;
	}
	ret = fwc_sensor_ioctl_all(dev, SD_EV_ENUM_INPUT, buf);
	if (ret)
		return ret;
	if (private_copy_to_user((void __user *)arg, buf, 80)) {
		COPY_TO_ERR("tx_isp_sensor_enum_input", 328);
		return -EFAULT;
	}
	return 0;
}

static int fwc_get_input(u8 *dev, unsigned long arg)
{
	/* the firmware copied this out unset when no sensor answered */
	u32 buf[1] = { 0 };
	int ret;

	ret = fwc_sensor_ioctl_all(dev, SD_EV_GET_INPUT, buf);
	if (ret)
		return ret;
	if (private_copy_to_user((void __user *)arg, buf, 4)) {
		COPY_TO_ERR("tx_isp_sensor_get_input", 353);
		return -EFAULT;
	}
	return 0;
}

static int fwc_set_input(u8 *dev, unsigned long arg)
{
	u32 buf[1];
	int ret;

	if (private_copy_from_user(buf, (void __user *)arg, 4)) {
		COPY_FROM_ERR("tx_isp_sensor_set_input", 368);
		return -EFAULT;
	}
	ret = fwc_sensor_ioctl_all(dev, SD_EV_SET_INPUT, buf);
	if (ret)
		return ret;
	if (private_copy_to_user((void __user *)arg, buf, 4)) {
		COPY_TO_ERR("tx_isp_sensor_set_input", 382);
		return -EFAULT;
	}
	return 0;
}

/* register/release carry an 80-byte sensor descriptor in, nothing back */
static int fwc_sensor_desc(u8 *dev, unsigned long arg, unsigned int ev,
			   const char *fn, int line)
{
	u32 buf[20];

	if (private_copy_from_user(buf, (void __user *)arg, 80)) {
		COPY_FROM_ERR(fn, line);
		return -EFAULT;
	}
	return fwc_sensor_ioctl_all(dev, ev, buf);
}

/*
 * The register ioctls hand the sensors a struct v4l2_dbg_register in a
 * repacked form: a pointer to its match.addr, the size, then reg and val
 * as 64-bit halves. The firmware leaves size unset going in (the sensor
 * fills it for a read); it is zeroed here.
 */
struct fwc_dbg_reg {
	u32 *match;
	u32 size;
	u32 reg[2];
	u32 val[2];
};

#define DBG_REG_SIZE	56
#define DBG_SIZE_AT	9
#define DBG_REG_AT	10
#define DBG_VAL_AT	12

static void fwc_dbg_reg_pack(struct fwc_dbg_reg *r, u32 *buf)
{
	r->match = &buf[1];
	r->reg[0] = buf[DBG_REG_AT];
	r->reg[1] = buf[DBG_REG_AT + 1];
}

static int fwc_s_register(u8 *dev, unsigned long arg)
{
	struct fwc_dbg_reg r = { };
	u32 buf[20];

	if (private_copy_from_user(buf, (void __user *)arg, DBG_REG_SIZE)) {
		COPY_FROM_ERR("tx_isp_sensor_s_register", 501);
		return -EFAULT;
	}
	fwc_dbg_reg_pack(&r, buf);
	r.val[0] = buf[DBG_VAL_AT];
	r.val[1] = buf[DBG_VAL_AT + 1];
	return fwc_sensor_ioctl_all(dev, SD_EV_S_REGISTER, &r);
}

static int fwc_g_register(u8 *dev, unsigned long arg)
{
	struct fwc_dbg_reg r = { };
	u32 buf[20];
	int ret;

	if (private_copy_from_user(buf, (void __user *)arg, DBG_REG_SIZE)) {
		COPY_FROM_ERR("tx_isp_sensor_g_register", 531);
		return -EFAULT;
	}
	fwc_dbg_reg_pack(&r, buf);
	ret = fwc_sensor_ioctl_all(dev, SD_EV_G_REGISTER, &r);
	if (ret)
		return ret;
	buf[DBG_VAL_AT] = r.val[0];
	buf[DBG_VAL_AT + 1] = r.val[1];
	buf[DBG_SIZE_AT] = r.size;
	if (private_copy_to_user((void __user *)arg, buf, DBG_REG_SIZE)) {
		/* the firmware's message says "from" for this copy too */
		COPY_FROM_ERR("tx_isp_sensor_g_register", 549);
		return -EFAULT;
	}
	return 0;
}

/* ---- video buffer ioctls ----------------------------------------------------- */

/* the first subdev's private state: the frame size the buffers are cut for */
static u8 *fwc_frame_priv(u8 *dev)
{
	void *sd = FW(dev, ISPDEV_SUBDEV_FIRST, void *);

	/* the firmware did not check */
	return sd ? FW(sd, SD_PRIV, u8 *) : NULL;
}

/* the 16-pixel-block geometry of the later planes */
static u32 fwc_blocks_w(u32 w)
{
	return ((((w + 15) >> 4) + 7) >> 3) << 4;
}

static u32 fwc_blocks_h(u32 h)
{
	return ((h + 15) >> 4) + 1;
}

/*
 * {base, size}: the ISP's buffers are laid out in the area from base:
 * luma, half of it for chroma, then two block planes of 20 bytes per block.
 * Each plane's address and geometry go to the 0x1b10.. registers; a size
 * that cannot hold the plane is reported after the registers are written,
 * as the firmware did.
 */
static int fwc_set_buf(u8 *dev, unsigned long arg)
{
	u32 req[2];
	u8 *p = fwc_frame_priv(dev);
	u32 w, h, stride, luma, end, bw, blocks;

	if (!p)
		return -EINVAL;
	if (private_copy_from_user(req, (void __user *)arg, 8)) {
		COPY_FROM_ERR("tx_isp_set_buf", 579);
		return -EFAULT;
	}
	w = FW(p, PRIV_FRAME_W, u32);
	h = FW(p, PRIV_FRAME_H, u32);

	stride = ALIGN(w, 8);
	luma = stride * h;
	system_reg_write(0x1b10, req[0]);
	system_reg_write(0x1b14, stride);
	if (req[1] < luma) {
		isp_printf(1, "%s,%d: buf size too small\n", "tx_isp_set_buf", 592);
		return -EFAULT;
	}

	stride = ALIGN(FW(p, PRIV_FRAME_W, u32), 8);
	end = (stride * FW(p, PRIV_FRAME_H, u32) >> 1) + luma;
	system_reg_write(0x1b20, luma + req[0]);
	system_reg_write(0x1b24, stride);
	if (req[1] < end) {
		isp_printf(1, "%s,%d: buf size too small\n", "tx_isp_set_buf", 603);
		return -EFAULT;
	}

	bw = fwc_blocks_w(FW(p, PRIV_FRAME_W, u32));
	blocks = bw * fwc_blocks_h(FW(p, PRIV_FRAME_H, u32));
	system_reg_write(0x1b30, end + req[0]);
	system_reg_write(0x1b34, bw);
	system_reg_write(0x1b38, blocks);
	luma = end;
	end = blocks * 20 + luma;
	if (req[1] < end) {
		isp_printf(1, "%s,%d: buf size too small\n", "tx_isp_set_buf", 616);
		return -EFAULT;
	}

	bw = fwc_blocks_w(FW(p, PRIV_FRAME_W, u32));
	blocks = bw * fwc_blocks_h(FW(p, PRIV_FRAME_H, u32));
	system_reg_write(0x1b40, end + req[0]);
	system_reg_write(0x1b44, bw);
	system_reg_write(0x1b48, blocks);
	if (req[1] < blocks * 20 + end) {
		isp_printf(1, "%s,%d: buf size too small\n", "tx_isp_set_buf", 629);
		return -EFAULT;
	}
	return 0;
}

static int fwc_get_buf(u8 *dev, unsigned long arg)
{
	u32 req[2];
	u8 *p = fwc_frame_priv(dev);
	u32 w, h, luma, planes;

	if (!p)
		return -EINVAL;
	w = FW(p, PRIV_FRAME_W, u32);
	h = FW(p, PRIV_FRAME_H, u32);
	luma = h * 8 * ((w + 7) >> 3);
	planes = fwc_blocks_h(h) * 320 * ((((w + 15) >> 4) + 7) >> 3);
	req[0] = 0;
	req[1] = (luma >> 1) + luma + 2 * planes;
	if (private_copy_to_user((void __user *)arg, req, 8)) {
		/* the firmware's message says "from" for this copy too */
		COPY_FROM_ERR("tx_isp_get_buf", 672);
		return -EFAULT;
	}
	return 0;
}

/* ---- links ---------------------------------------------------------------- */

/*
 * "setup" connects the pads link1[] names, once, when the ispdev's link
 * state is set; a link already made to something else is torn down first.
 * Only link 0 exists.
 */
static int fwc_link_setup(u8 *dev, u8 *ispdev, unsigned long arg)
{
	u32 link;
	int i;

	if (private_copy_from_user(&link, (void __user *)arg, 4)) {
		COPY_FROM_ERR("tx_isp_video_link_setup", 244);
		return -EFAULT;
	}
	if (link) {
		isp_printf(2, "link(%d) is invalid!\n", link);
		return -EINVAL;
	}
	if (!FW(dev, ISPDEV_LINK, int))
		return 0;

	for (i = 0; i < ARRAY_SIZE(link1); i++) {
		const u8 *cfg = link1[i];
		u8 *a = find_subdev_link_pad(ispdev, cfg);
		u8 *b = find_subdev_link_pad(ispdev, cfg + LINKCFG_DST);
		u32 type;
		int ret;

		if (!a || !b)
			continue;
		type = FW(cfg, LINKCFG_TYPE, u32);
		if (!(type & (a[PAD_TYPE] & b[PAD_TYPE]))) {
			isp_printf(2, "The link type is mismatch!\n");
			return -1;
		}
		if (a[PAD_STATE] == PAD_STATE_STREAMING || b[PAD_STATE] == PAD_STATE_STREAMING) {
			isp_printf(2, "Please stop active links firstly! %d\n", 198);
			return -1;
		}
		if (a[PAD_STATE] == PAD_STATE_LINKED && FW(a, PAD_PEER, u8 *) != b) {
			ret = subdev_video_destroy_link(a + PAD_LINK);
			if (ret)
				goto failed;
		}
		if (b[PAD_STATE] == PAD_STATE_LINKED && FW(b, PAD_PEER, u8 *) != a) {
			ret = subdev_video_destroy_link(b + PAD_LINK);
			if (ret)
				goto failed;
		}
		type |= 1;
		FW(a, PAD_PEER_LINK, u8 *) = b + PAD_LINK;
		FW(a, PAD_LINK, u8 *) = a;
		FW(a, PAD_PEER, u8 *) = b;
		FW(a, PAD_FLAGS, u32) = type;
		a[PAD_STATE] = PAD_STATE_LINKED;
		FW(b, PAD_LINK, u8 *) = b;
		FW(b, PAD_PEER, u8 *) = a;
		FW(b, PAD_PEER_LINK, u8 *) = a + PAD_LINK;
		FW(b, PAD_FLAGS, u32) = type;
		b[PAD_STATE] = PAD_STATE_LINKED;
		continue;
failed:
		/* -ENOIOCTLCMD skips this link and goes on */
		if (ret != FW_ENOIOCTLCMD)
			return ret;
	}
	FW(dev, ISPDEV_LINK, int) = link;
	return 0;
}

long fwc_tx_isp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u8 *dev = file->private_data;
	void *ispdev = dev - ISPDEV_TO_MISC;

	switch (cmd) {
	case TXIO_SET_BUF:
		return fwc_set_buf(dev, arg);
	case TXIO_GET_BUF:
		return fwc_get_buf(dev, arg);
	case TXIO_LINK_SETUP:
		return fwc_link_setup(dev, ispdev, arg);
	case TXIO_LINK_DESTROY:
		return tx_isp_video_link_destroy(ispdev);
	case TXIO_LINK_STREAM_ON:
		return tx_isp_video_link_stream(ispdev, 1);
	case TXIO_LINK_STREAM_OFF:
		return tx_isp_video_link_stream(ispdev, 0);
	case TXIO_VIDEO_S_STREAM_ON:
		return tx_isp_video_s_stream(ispdev, 1);
	case TXIO_VIDEO_S_STREAM_OFF:
		return tx_isp_video_s_stream(ispdev, 0);
	case TXIO_ENUM_INPUT:
		return fwc_enum_input(dev, arg);
	case TXIO_GET_INPUT:
		return fwc_get_input(dev, arg);
	case TXIO_SET_INPUT:
		return fwc_set_input(dev, arg);
	case TXIO_REGISTER_SENSOR:
		return fwc_sensor_desc(dev, arg, SD_EV_REGISTER_SENSOR,
				       "tx_isp_sensor_register_sensor", 397);
	case TXIO_RELEASE_SENSOR:
		return fwc_sensor_desc(dev, arg, SD_EV_RELEASE_SENSOR,
				       "tx_isp_sensor_release_sensor", 422);
	case TXIO_S_REGISTER:
		return fwc_s_register(dev, arg);
	case TXIO_G_REGISTER:
		return fwc_g_register(dev, arg);
	}
	return 0;
}
