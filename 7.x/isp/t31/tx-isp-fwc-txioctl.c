// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-txioctl.c - C replacement for the firmware's /dev/tx-isp ioctl
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
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/uaccess.h>
#include <linux/err.h>

#include "tx-isp-abi.h"

#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

/* ---- firmware symbols ------------------------------------------------------ */

extern int isp_printf(int level, const char *fmt, ...);
extern int private_copy_from_user(void *to, const void __user *from, unsigned long n);
extern int private_copy_to_user(void __user *to, const void *from, unsigned long n);
extern int private_wait_for_completion_interruptible(struct completion *c);
extern void private_kfree(const void *p);
extern int private_math_exp2(int val, int a, int b);

extern int system_reg_read(u32 reg);
extern void system_reg_write(u32 reg, u32 val);

extern void tisp_s_wdr_en(int en);
extern int tisp_ae_algo_init(int en, void *info);
extern void tisp_ae_algo_deinit(void);
extern void tisp_ae_algo_handle(void *info);
extern int tisp_awb_algo_init(int en);
extern void tisp_awb_algo_deinit(void);
extern void tisp_awb_algo_handle(void *info);
extern int tisp_g_wb_zone(void *zone);
extern int tisp_g_wb_attr(void *attr);
extern int tisp_set_frame_drop(int chan, void *attr);
extern int tisp_get_frame_drop(int chan, void *attr);

/* firmware data; the AE ones and configs are local in the blob */
extern int isp_memopt;
extern u8 *awb_info_mine;
extern u8 *ae_info_mine;
extern u8 *ae_statis_mine;
extern int wdr_switch;
extern struct completion awb_algo_comp;
extern struct completion ae_algo_comp;
extern struct fwc_link_cfg {
	const u8 *links;
	int count;
} configs[2];

/* local functions of the blob */
extern int tx_isp_video_s_stream(void *ispdev, int enable);
extern int tx_isp_video_link_stream(void *ispdev, int enable);
extern void *find_subdev_link_pad(void *ispdev, const void *pad_name);
extern int subdev_video_destroy_link(void *link);
extern int tx_isp_video_link_destroy(void *ispdev) asm("tx_isp_video_link_destroy.isra.5");
extern int tx_isp_get_ae_algo_handle(void *sd, void __user *arg)
	asm("tx_isp_get_ae_algo_handle.isra.16");

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
#define SD_EVENT		0x7c	/* int (*)(sd, cmd, arg) */
#define SD_PRIV			0xd4

/* the sensor subdev's private state, and the core state at +0x120 */
#define PRIV_FRAME_W		0xec
#define PRIV_FRAME_H		0xf0
#define PRIV_CORE		0x120
#define PRIV_WIDTH		0x124
#define PRIV_HEIGHT		0x128
#define PRIV_AE_INFO		0x12c
#define PRIV_WDR_EN		0x17c
#define PRIV_BIN_PATH		0x1d8

#define CORE_WDR_MODE		0x90
#define CORE_WDR_TSIZE		0xe8
#define CORE_AGAIN		0xac
#define CORE_AGAIN_EXP		0x9c
#define CORE_AGAIN_S		0xa0
#define CORE_DGAIN		0xdc
#define CORE_DGAIN_EXP		0xe4

/* link pads: +6 type mask, +7 state, +8 the link {self, peer, peer link, flags} */
#define PAD_TYPE		6
#define PAD_STATE		7
#define PAD_LINK		8
#define PAD_PEER		0xc
#define PAD_PEER_LINK		0x10
#define PAD_FLAGS		0x14
#define PAD_STATE_LINKED	3
#define PAD_STATE_STREAMING	4

/* a configs[] entry: two pad names (8 bytes each) and the link type mask */
#define LINKCFG_SIZE		0x14
#define LINKCFG_DST		8
#define LINKCFG_TYPE		0x10

/* subdev event commands the ioctls forward */
#define SD_EV_REGISTER_SENSOR	0x2000000
#define SD_EV_RELEASE_SENSOR	0x2000001
#define SD_EV_ENUM_INPUT	0x2000002
#define SD_EV_GET_INPUT		0x2000003
#define SD_EV_SET_INPUT		0x2000004
#define SD_EV_WDR_AGAIN_CLR	0x200000c
#define SD_EV_S_REGISTER	0x2000011
#define SD_EV_G_REGISTER	0x2000012
#define SD_EV_WDR_EN		0x2000013
#define SD_EV_GPIO_INIT		0x2000017
#define SD_EV_GPIO_STA		0x2000018

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
#define TXIO_DEFAULT_BIN_SET	0xc00456c7
#define TXIO_DEFAULT_BIN_GET	0xc00456c8
#define TXIO_LINK_SETUP		0x800456d0
#define TXIO_LINK_DESTROY	0x800456d1
#define TXIO_LINK_STREAM_ON	0x800456d2
#define TXIO_LINK_STREAM_OFF	0x800456d3
#define TXIO_SET_BUF		0x800856d4
#define TXIO_GET_BUF		0x800856d5
#define TXIO_WDR_SET_BUF	0x800856d6
#define TXIO_WDR_GET_BUF	0x800856d7
#define TXIO_WDR_ENABLE		0x800456d8
#define TXIO_WDR_DISABLE	0x800456d9
#define TXIO_GET_AE_ALGO	0x800456db
#define TXIO_SET_AE_HANDLE	0x800456dc
#define TXIO_AE_OPEN		0x800456dd
#define TXIO_AE_CLOSE		0x800456de
#define TXIO_GET_AWB_ALGO	0xc00456e1
#define TXIO_SET_AWB_HANDLE	0xc00456e2
#define TXIO_AWB_OPEN		0xc00456e3
#define TXIO_AWB_CLOSE		0xc00456e4
#define TXIO_SET_FRAME_DROP	0xc00456e6
#define TXIO_GET_FRAME_DROP	0xc00456e7
#define TXIO_GPIO_INIT		0xc00456e8
#define TXIO_GPIO_STA		0xc00456e9

#define AE_ALGO_VERSION		0x336ac

typedef int (*fw_sd_event)(void *sd, unsigned int cmd, void *arg);

#define COPY_FROM_ERR(fn, line) \
	isp_printf(2, "[%s][%d] copy from user error\n", fn, line)
#define COPY_TO_ERR(fn, line) \
	isp_printf(2, "[%s][%d] copy to user error\n", fn, line)
#define COPY_FROM_ERR2(fn, line) \
	isp_printf(2, "[ %s:%d ] copy from user error\n", fn, line)
#define COPY_TO_ERR2(fn, line) \
	isp_printf(2, "[ %s:%d ] copy to user error\n", fn, line)

/*
 * Pass cmd to the sensor ops ioctl of every subdev that has one. The first
 * real error stops the walk; -ENOIOCTLCMD does not.
 */
static int fwc_sensor_ioctl_all(u8 *dev, unsigned int cmd, void *arg)
{
	int off;

	for (off = ISPDEV_SUBDEV_FIRST; off < ISPDEV_SUBDEV_END; off += 4) {
		void *sd = FW(dev, off, void *);
		u8 *sensor_ops;
		fw_sd_event fn;
		int ret;

		if (!sd)
			continue;
		sensor_ops = FW(FW(sd, SD_OPS, u8 *), SD_OPS_SENSOR, u8 *);
		if (!sensor_ops)
			continue;
		fn = FW(sensor_ops, SENSOR_IOCTL, fw_sd_event);
		if (!fn)
			continue;
		ret = fn(sd, cmd, arg);
		if (ret && ret != FW_ENOIOCTLCMD)
			return ret;
	}
	return 0;
}

static int fwc_sd_event(void *sd, unsigned int cmd, void *arg)
{
	fw_sd_event fn = FW(sd, SD_EVENT, fw_sd_event);

	return fn ? fn(sd, cmd, arg) : FW_ENOIOCTLCMD;
}

/* ---- sensor input ioctls ---------------------------------------------------- */

static int fwc_enum_input(u8 *dev, unsigned long arg)
{
	u32 buf[20];
	int ret;

	if (private_copy_from_user(buf, (void __user *)arg, 80)) {
		COPY_FROM_ERR("tx_isp_sensor_enum_input", 337);
		return -EFAULT;
	}
	ret = fwc_sensor_ioctl_all(dev, SD_EV_ENUM_INPUT, buf);
	if (ret)
		return ret;
	if (private_copy_to_user((void __user *)arg, buf, 80)) {
		COPY_TO_ERR("tx_isp_sensor_enum_input", 351);
		return -EFAULT;
	}
	return 0;
}

static int fwc_get_input(u8 *dev, unsigned long arg)
{
	u32 buf[20];
	int ret;

	ret = fwc_sensor_ioctl_all(dev, SD_EV_GET_INPUT, buf);
	if (ret)
		return ret;
	if (private_copy_to_user((void __user *)arg, buf, 4)) {
		COPY_TO_ERR("tx_isp_sensor_get_input", 377);
		return -EFAULT;
	}
	return 0;
}

static int fwc_set_input(u8 *dev, unsigned long arg)
{
	u32 buf[20];
	int ret;

	if (private_copy_from_user(buf, (void __user *)arg, 4)) {
		COPY_FROM_ERR("tx_isp_sensor_set_input", 392);
		return -EFAULT;
	}
	ret = fwc_sensor_ioctl_all(dev, SD_EV_SET_INPUT, buf);
	if (ret)
		return ret;
	if (private_copy_to_user((void __user *)arg, buf, 4)) {
		COPY_TO_ERR("tx_isp_sensor_set_input", 406);
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

static int fwc_s_register(u8 *dev, unsigned long arg)
{
	struct fwc_dbg_reg r = { };
	u32 buf[20];

	if (private_copy_from_user(buf, (void __user *)arg, 56)) {
		COPY_FROM_ERR("tx_isp_sensor_s_register", 531);
		return -EFAULT;
	}
	r.match = &buf[1];
	r.reg[0] = buf[10];
	r.reg[1] = buf[11];
	r.val[0] = buf[12];
	r.val[1] = buf[13];
	return fwc_sensor_ioctl_all(dev, SD_EV_S_REGISTER, &r);
}

static int fwc_g_register(u8 *dev, unsigned long arg)
{
	struct fwc_dbg_reg r = { };
	u32 buf[20];
	int ret;

	if (private_copy_from_user(buf, (void __user *)arg, 56)) {
		COPY_FROM_ERR("tx_isp_sensor_g_register", 562);
		return -EFAULT;
	}
	r.match = &buf[1];
	r.reg[0] = buf[10];
	r.reg[1] = buf[11];
	ret = fwc_sensor_ioctl_all(dev, SD_EV_G_REGISTER, &r);
	if (ret)
		return ret;
	buf[9] = r.size;
	buf[12] = r.val[0];
	buf[13] = r.val[1];
	if (private_copy_to_user((void __user *)arg, buf, 56)) {
		COPY_FROM_ERR("tx_isp_sensor_g_register", 580);
		return -EFAULT;
	}
	return 0;
}

/* ---- video links --------------------------------------------------------------- */

static int fwc_link_setup(u8 *dev, unsigned long arg)
{
	u8 *ispdev = dev - ISPDEV_TO_MISC;
	const u8 *e;
	u32 link;
	int count, i;

	if (private_copy_from_user(&link, (void __user *)arg, 4)) {
		COPY_FROM_ERR("tx_isp_video_link_setup", 264);
		return -EFAULT;
	}
	if (link >= 2) {
		isp_printf(2, "link(%d) is invalid!\n", link);
		return -EINVAL;
	}
	if (link == FW(dev, ISPDEV_LINK, u32))
		return 0;

	e = configs[link].links;
	count = configs[link].count;
	for (i = 0; i < count; i++, e += LINKCFG_SIZE) {
		u8 *a = find_subdev_link_pad(ispdev, e);
		u8 *b = find_subdev_link_pad(ispdev, e + LINKCFG_DST);
		u32 flags;
		int ret;

		if (!a || !b)
			continue;
		flags = FW(e, LINKCFG_TYPE, u32);
		if (!(a[PAD_TYPE] & b[PAD_TYPE] & flags)) {
			isp_printf(2, "The link type is mismatch!\n");
			return -1;
		}
		if (a[PAD_STATE] == PAD_STATE_STREAMING ||
		    b[PAD_STATE] == PAD_STATE_STREAMING) {
			isp_printf(2, "Please stop active links firstly! %d\n", 217);
			return -1;
		}
		if (a[PAD_STATE] == PAD_STATE_LINKED && b != FW(a, PAD_PEER, u8 *)) {
			ret = subdev_video_destroy_link(a + PAD_LINK);
			if (ret)
				goto failed;
		}
		if (b[PAD_STATE] == PAD_STATE_LINKED && a != FW(b, PAD_PEER, u8 *)) {
			ret = subdev_video_destroy_link(b + PAD_LINK);
			if (ret)
				goto failed;
		}
		flags |= 1;
		FW(a, PAD_LINK, u8 *) = a;
		FW(a, PAD_PEER, u8 *) = b;
		FW(a, PAD_PEER_LINK, u8 *) = b + PAD_LINK;
		FW(a, PAD_FLAGS, u32) = flags;
		a[PAD_STATE] = PAD_STATE_LINKED;
		FW(b, PAD_LINK, u8 *) = b;
		FW(b, PAD_PEER, u8 *) = a;
		FW(b, PAD_PEER_LINK, u8 *) = a + PAD_LINK;
		FW(b, PAD_FLAGS, u32) = flags;
		b[PAD_STATE] = PAD_STATE_LINKED;
		continue;
failed:
		if (ret != FW_ENOIOCTLCMD)
			return ret;
	}
	FW(dev, ISPDEV_LINK, u32) = link;
	return 0;
}

/* ---- frame buffer layout ------------------------------------------------------- */

static int fwc_buf_small(int line)
{
	isp_printf(1, "%s,%d: buf size too small\n", "tx_isp_set_buf", line);
	return -EFAULT;
}

/*
 * Lay the ISP's frame buffers out from a user-given base and size: luma at
 * 0x7820, chroma at 0x7828, then the planes the memory mode needs.
 */
static int fwc_set_buf(u8 *dev, unsigned long arg)
{
	u8 *priv = FW(FW(dev, ISPDEV_SUBDEV_FIRST, u8 *), SD_PRIV, u8 *);
	u32 buf[2];
	u32 w, h, stride, luma, off, t, stride2, stride3;
	u32 base;

	if (private_copy_from_user(buf, (void __user *)arg, 8)) {
		COPY_FROM_ERR("tx_isp_set_buf", 610);
		return -EFAULT;
	}
	base = buf[0];
	w = FW(priv, PRIV_FRAME_W, u32);
	h = FW(priv, PRIV_FRAME_H, u32);
	stride = (w + 7) >> 3 << 3;
	luma = stride * h;
	system_reg_write(0x7820, base);
	system_reg_write(0x7824, stride);
	if (buf[1] < luma)
		return fwc_buf_small(624);

	w = FW(priv, PRIV_FRAME_W, u32);
	h = FW(priv, PRIV_FRAME_H, u32);
	stride = (w + 7) >> 3 << 3;
	system_reg_write(0x7828, luma + base);
	system_reg_write(0x782c, stride);
	off = luma + (stride * h >> 1);
	if (buf[1] < off)
		return fwc_buf_small(636);

	w = FW(priv, PRIV_FRAME_W, u32);
	h = FW(priv, PRIV_FRAME_H, u32);
	stride2 = (((w + 31) >> 5) + 7) >> 3 << 3;
	system_reg_write(0x7830, off + base);
	system_reg_write(0x7834, stride2);
	t = (((h + 15) >> 4) + 1) * stride2;
	if (!READ_ONCE(isp_memopt)) {
		system_reg_write(0x7840, off + t + base);
		system_reg_write(0x7844, stride2);
		system_reg_write(0x7848, off + base + (t << 1));
		system_reg_write(0x784c, stride2);
		system_reg_write(0x7850, off + base + (t << 1) + t);
		system_reg_write(0x7854, stride2);
		t <<= 2;
	} else {
		system_reg_write(0x7840, off + base);
		system_reg_write(0x7844, 0);
		system_reg_write(0x7848, off + base);
		system_reg_write(0x784c, 0);
		system_reg_write(0x7850, off + base);
		system_reg_write(0x7854, 0);
	}
	system_reg_write(0x7838, 0);
	system_reg_write(0x783c, 1);
	off += t;
	if (buf[1] < off)
		return fwc_buf_small(667);

	if (!READ_ONCE(isp_memopt)) {
		w = FW(priv, PRIV_FRAME_W, u32);
		h = FW(priv, PRIV_FRAME_H, u32);
		stride3 = ((w >> 1) + 7) >> 3 << 3;
		t = stride3 * h;
		system_reg_write(0x7858, off + base);
		system_reg_write(0x785c, stride3);
		off += t;
		if (buf[1] < off)
			return fwc_buf_small(680);

		w = FW(priv, PRIV_FRAME_W, u32);
		h = FW(priv, PRIV_FRAME_H, u32);
		stride3 = ((w >> 1) + 7) >> 3 << 3;
		t = stride3 * h;
		system_reg_write(0x7860, off + base);
		system_reg_write(0x7864, stride3);
		off += t >> 1;
		if (buf[1] < off)
			return fwc_buf_small(692);

		w = FW(priv, PRIV_FRAME_W, u32);
		h = FW(priv, PRIV_FRAME_H, u32);
		stride3 = ((w >> 5) + 7) >> 3 << 3;
		t = stride3 * h;
		system_reg_write(0x7868, off + base);
		system_reg_write(0x786c, stride3);
		off += t >> 5;
	} else {
		system_reg_write(0x7858, base);
		system_reg_write(0x785c, 0);
		system_reg_write(0x7860, base);
		system_reg_write(0x7864, 0);
		system_reg_write(0x7868, base);
		system_reg_write(0x786c, 0);
	}
	if (buf[1] < off)
		return fwc_buf_small(716);
	return 0;
}

static int fwc_get_buf(u8 *dev, unsigned long arg)
{
	u8 *priv = FW(FW(dev, ISPDEV_SUBDEV_FIRST, u8 *), SD_PRIV, u8 *);
	u32 buf[2];
	u32 w = FW(priv, PRIV_FRAME_W, u32);
	u32 h8 = FW(priv, PRIV_FRAME_H, u32) << 3;
	u32 luma = ((w + 7) >> 3) * h8;
	u32 yuv = (luma >> 1) + luma;
	u32 c = ((((w + 31) >> 5) + 7) >> 3) *
		((((FW(priv, PRIV_FRAME_H, u32) + 15) >> 4) + 1) << 3);
	u32 total = yuv + c;

	if (!READ_ONCE(isp_memopt)) {
		u32 a = ((((w >> 1) + 7) >> 3)) * h8;
		u32 b = ((((w >> 5) + 7) >> 3)) * h8;

		total = (c << 2) + (a >> 1) + yuv + a + (b >> 5);
	}
	buf[0] = 0;
	buf[1] = total;
	if (private_copy_to_user((void __user *)arg, buf, 8)) {
		COPY_FROM_ERR("tx_isp_get_buf", 778);
		return -EFAULT;
	}
	return 0;
}

/* ---- WDR ------------------------------------------------------------------------ */

static void fwc_wdr_clear_core(u8 *core)
{
	FW(core, CORE_AGAIN, u32) = 0;
	FW(core, CORE_DGAIN, u32) = 0;
	FW(core, CORE_AGAIN_EXP, u32) = 0;
	FW(core, CORE_DGAIN_EXP, u32) = 0;
	FW(core, CORE_AGAIN_S, u32) = 0;
}

static int fwc_wdr_enable(u8 *dev)
{
	void *sd = FW(dev, ISPDEV_SUBDEV_FIRST, void *);
	u8 *priv = FW(sd, SD_PRIV, u8 *);
	u32 en = 1;

	if (FW(priv, PRIV_WDR_EN, int) == 1)
		return 0;
	FW(priv, PRIV_WDR_EN, int) = 1;
	if (wdr_switch) {
		if (sd && FW(sd, SD_EVENT, fw_sd_event))
			FW(sd, SD_EVENT, fw_sd_event)(sd, SD_EV_WDR_EN, &en);
		tisp_s_wdr_en(en);
		if (sd && FW(sd, SD_EVENT, fw_sd_event))
			FW(sd, SD_EVENT, fw_sd_event)(sd, SD_EV_WDR_AGAIN_CLR, &en);
		fwc_wdr_clear_core(FW(priv, PRIV_CORE, u8 *));
	}
	wdr_switch = 1;
	return 0;
}

static int fwc_wdr_disable(u8 *dev)
{
	void *sd = FW(dev, ISPDEV_SUBDEV_FIRST, void *);
	u8 *priv = FW(sd, SD_PRIV, u8 *);
	u32 en = 0;
	u8 *core;

	if (!FW(priv, PRIV_WDR_EN, int))
		return 0;
	core = FW(priv, PRIV_CORE, u8 *);
	FW(priv, PRIV_WDR_EN, int) = 0;
	if (wdr_switch) {
		if (sd && FW(sd, SD_EVENT, fw_sd_event))
			FW(sd, SD_EVENT, fw_sd_event)(sd, SD_EV_WDR_EN, &en);
		tisp_s_wdr_en(en);
		if (sd && FW(sd, SD_EVENT, fw_sd_event))
			FW(sd, SD_EVENT, fw_sd_event)(sd, SD_EV_WDR_AGAIN_CLR, &en);
		fwc_wdr_clear_core(core);
	}
	wdr_switch = 1;
	return 0;
}

static int fwc_wdr_get_buf(u8 *dev, unsigned long arg)
{
	u8 *priv = FW(FW(dev, ISPDEV_SUBDEV_FIRST, u8 *), SD_PRIV, u8 *);
	u8 *core = FW(priv, PRIV_CORE, u8 *);
	u32 buf[2] = { 0, 0 };
	int mode = FW(core, CORE_WDR_MODE, int);

	if (mode == 1)
		buf[1] = FW(priv, PRIV_WIDTH, u32) * FW(priv, PRIV_HEIGHT, u32) * 2;
	else if (mode == 2)
		buf[1] = FW(core, CORE_WDR_TSIZE, u32);
	else
		isp_printf(1, "Not the wdr mode, do not need to alloc buf!!!\n");
	isp_printf(0, "%s:%d::tsize is %d\n", "tx_isp_wdr_get_buf", 857, buf[1]);
	if (private_copy_to_user((void __user *)arg, buf, 8)) {
		COPY_FROM_ERR("tx_isp_wdr_get_buf", 859);
		return -EFAULT;
	}
	return 0;
}

static int fwc_wdr_set_buf(u8 *dev, unsigned long arg)
{
	u8 *priv = FW(FW(dev, ISPDEV_SUBDEV_FIRST, u8 *), SD_PRIV, u8 *);
	u8 *core;
	u32 buf[2];
	u32 tsize, line;
	int mode;

	if (private_copy_from_user(buf, (void __user *)arg, 8)) {
		COPY_FROM_ERR("tx_isp_wdr_set_buf", 802);
		return -EFAULT;
	}
	isp_printf(0, "%s:%d::tsize is %d, buf info size is %d, paddr is 0x%x\n",
		   "tx_isp_wdr_set_buf", 806, 0, buf[1], buf[0]);
	core = FW(priv, PRIV_CORE, u8 *);
	mode = FW(core, CORE_WDR_MODE, int);
	if (mode == 1) {
		line = FW(priv, PRIV_HEIGHT, u32);
		tsize = line * FW(priv, PRIV_WIDTH, u32) * 2;
	} else if (mode == 2) {
		u32 row = FW(priv, PRIV_WIDTH, u32) << 1;

		tsize = FW(core, CORE_WDR_TSIZE, u32);
		/* the firmware divided unguarded; the hardware result of /0 was garbage */
		line = row ? tsize / row : 0;
	} else {
		isp_printf(2, "Not the wdr mode, do not need to alloc buf!!!\n");
		return -1;
	}
	isp_printf(0, "%s:%d::tsize is %d, buf info size is %d, paddr is 0x%x\n",
		   "tx_isp_wdr_set_buf", 817, tsize, buf[1], buf[0]);
	if (buf[1] < tsize) {
		isp_printf(0, "%s,%d: buf size too small\n", "tx_isp_wdr_set_buf", 820);
		FW(priv, PRIV_WDR_EN, int) = 0;
		tisp_s_wdr_en(0);
		return -EFAULT;
	}
	system_reg_write(0x2004, buf[0]);
	system_reg_write(0x2008, FW(priv, PRIV_WIDTH, u32) << 1);
	system_reg_write(0x200c, line);
	return 0;
}

/* ---- AE / AWB algorithm handles ---------------------------------------------- */

static int fwc_set_ae_handle(unsigned long arg)
{
	u32 buf[20];

	if (private_copy_from_user(buf, (void __user *)arg, 56)) {
		COPY_FROM_ERR2("tx_isp_set_ae_algo_handle", 1073);
		return -EFAULT;
	}
	if (buf[2] == 1)
		tisp_ae_algo_handle(buf);
	return 0;
}

static int fwc_ae_open(u8 *dev, unsigned long arg)
{
	u8 *priv = FW(FW(dev, ISPDEV_SUBDEV_FIRST, u8 *), SD_PRIV, u8 *);
	u8 *core;
	u32 *a;
	int ret = -EFAULT;

	/* inlined kmalloc(128) in the firmware, not checked there */
	a = kmalloc(128, GFP_KERNEL);
	if (!a)
		return -ENOMEM;
	if (private_copy_from_user(a, (void __user *)arg, 128)) {
		COPY_FROM_ERR2("tx_isp_set_ae_algo_open", 1101);
		goto out;
	}
	if (a[0] != AE_ALGO_VERSION) {
		isp_printf(2, "[ %s:%d ] Ae Algo Function register version error\n",
			   "tx_isp_set_ae_algo_open", 1106);
		ret = -1;
		goto out;
	}
	core = FW(priv, PRIV_CORE, u8 *);
	tisp_ae_algo_init(1, a);
	a[27] = FW(priv, PRIV_AE_INFO, u32);
	a[3] = FW(core, CORE_AGAIN, u32);
	a[4] = private_math_exp2(FW(core, CORE_AGAIN_EXP, int), 16, 10);
	a[5] = 1024;
	a[15] = FW(core, CORE_DGAIN, u32);
	a[16] = private_math_exp2(FW(core, CORE_DGAIN_EXP, int), 16, 10);
	a[17] = 1024;
	if (!FW(priv, PRIV_WDR_EN, int)) {
		a[6] = system_reg_read(0x1030) & 0xffff;
	} else {
		a[6] = system_reg_read(0x1000) & 0xffff;
		a[18] = system_reg_read(0x100c) & 0xffff;
	}
	ae_info_mine = kmalloc(2048, GFP_KERNEL);
	ae_statis_mine = kmalloc(2048, GFP_KERNEL);
	init_completion(&ae_algo_comp);
	if (private_copy_to_user((void __user *)arg, a, 128)) {
		COPY_TO_ERR2("tx_isp_set_ae_algo_open", 1135);
		goto out;
	}
	ret = 0;
out:
	/* the firmware leaked the copy on its error paths */
	private_kfree(a);
	return ret;
}

static int fwc_ae_close(unsigned long arg)
{
	u32 buf[2];

	if (private_copy_from_user(buf, (void __user *)arg, 8)) {
		COPY_FROM_ERR2("tx_isp_set_ae_algo_close", 1149);
		return -EFAULT;
	}
	tisp_ae_algo_deinit();
	tisp_ae_algo_init(0, NULL);
	private_kfree(ae_info_mine);
	private_kfree(ae_statis_mine);
	return 0;
}

static int fwc_get_awb(unsigned long arg)
{
	u8 *info = awb_info_mine;
	u32 buf[20];

	private_wait_for_completion_interruptible(&awb_algo_comp);
	/* never opened or already closed: the firmware wrote through the stale pointer */
	if (!info)
		return -EINVAL;
	memset(info + 8, 0, 699);
	tisp_g_wb_zone(info + 0x20);
	tisp_g_wb_attr(buf);
	FW(info, 0x10, u32) = buf[3];
	FW(info, 0x14, u32) = buf[4];
	FW(info, 0x18, u32) = buf[5];
	FW(info, 0x1c, u32) = buf[6];
	FW(info, 0x08, u32) = buf[1];
	FW(info, 0x0c, u32) = buf[2];
	if (private_copy_to_user((void __user *)arg, info, 707)) {
		COPY_TO_ERR2("tx_isp_get_awb_algo_handle", 1201);
		return -EFAULT;
	}
	return 0;
}

static int fwc_set_awb_handle(unsigned long arg)
{
	u32 buf[20];

	if (private_copy_from_user(buf, (void __user *)arg, 24)) {
		COPY_FROM_ERR2("tx_isp_set_awb_algo_handle", 1213);
		return -EFAULT;
	}
	if (buf[2] == 1)
		tisp_awb_algo_handle(buf);
	return 0;
}

static int fwc_awb_open(void)
{
	tisp_awb_algo_init(1);
	awb_info_mine = kmalloc(1024, GFP_KERNEL);
	init_completion(&awb_algo_comp);
	return 0;
}

static int fwc_awb_close(unsigned long arg)
{
	u32 buf[2];

	if (private_copy_from_user(buf, (void __user *)arg, 8)) {
		COPY_FROM_ERR2("tx_isp_set_awb_algo_close", 1248);
		return -EFAULT;
	}
	tisp_awb_algo_deinit();
	tisp_awb_algo_init(0);
	private_kfree(awb_info_mine);
	awb_info_mine = NULL;
	return 0;
}

/* ---- default bin path, frame drop, gpio ---------------------------------------- */

static int fwc_set_bin_path(u8 *dev, unsigned long arg)
{
	u8 *priv = FW(FW(dev, ISPDEV_SUBDEV_FIRST, u8 *), SD_PRIV, u8 *);
	u32 buf[20];

	if (private_copy_from_user(buf, (void __user *)arg, 64)) {
		COPY_FROM_ERR2("tx_isp_set_default_bin_path", 1282);
		return -EFAULT;
	}
	memcpy(priv + PRIV_BIN_PATH, buf, 64);
	return 0;
}

static int fwc_get_bin_path(u8 *dev, unsigned long arg)
{
	u8 *priv = FW(FW(dev, ISPDEV_SUBDEV_FIRST, u8 *), SD_PRIV, u8 *);
	char buf[64];

	/*
	 * The firmware sprintf()ed the stored path as the format string into
	 * a stack buffer, leaving the tail of the 64 bytes it copied out
	 * uninitialised; a path with a '%' or no terminator broke it.
	 */
	memset(buf, 0, sizeof(buf));
	strscpy(buf, (const char *)(priv + PRIV_BIN_PATH), sizeof(buf));
	if (private_copy_to_user((void __user *)arg, buf, 64)) {
		COPY_TO_ERR2("tx_isp_get_default_bin_path", 1310);
		return -EFAULT;
	}
	return 0;
}

static int fwc_set_frame_drop(unsigned long arg)
{
	u32 buf[20];
	int ret = 0;
	int i;

	if (private_copy_from_user(buf, (void __user *)arg, 36)) {
		COPY_FROM_ERR2("tx_isp_set_frame_drop", 1324);
		return -EFAULT;
	}
	for (i = 0; i < 3; i++)
		ret = tisp_set_frame_drop(i, &buf[i * 3]);
	return ret;
}

static int fwc_get_frame_drop(unsigned long arg)
{
	u32 buf[20];
	int ret = 0;
	int i;

	for (i = 0; i < 3; i++)
		ret = tisp_get_frame_drop(i, &buf[i * 3]);
	if (private_copy_to_user((void __user *)arg, buf, 36)) {
		COPY_TO_ERR2("tx_isp_get_frame_drop", 1342);
		return -EFAULT;
	}
	return ret;
}

static int fwc_gpio(u8 *dev, unsigned long arg, unsigned int ev,
		    const char *fn, int line)
{
	void *sd = FW(dev, ISPDEV_SUBDEV_FIRST, void *);
	u32 buf[20];

	if (private_copy_from_user(buf, (void __user *)arg, 42)) {
		COPY_FROM_ERR(fn, line);
		return -EFAULT;
	}
	if (!sd)
		return -ENOENT;
	return fwc_sd_event(sd, ev, buf);
}

/* ---- the ioctl -------------------------------------------------------------------- */

long fwc_tx_isp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u8 *dev = file->private_data;

	switch (cmd) {
	case TXIO_WDR_GET_BUF:
		return fwc_wdr_get_buf(dev, arg);
	case TXIO_WDR_ENABLE:
		return fwc_wdr_enable(dev);
	case TXIO_WDR_DISABLE:
		return fwc_wdr_disable(dev);
	case TXIO_WDR_SET_BUF:
		return fwc_wdr_set_buf(dev, arg);

	case TXIO_VIDEO_S_STREAM_ON:
		return tx_isp_video_s_stream(dev - ISPDEV_TO_MISC, 1);
	case TXIO_VIDEO_S_STREAM_OFF:
		return tx_isp_video_s_stream(dev - ISPDEV_TO_MISC, 0);
	case TXIO_LINK_STREAM_ON:
		return tx_isp_video_link_stream(dev - ISPDEV_TO_MISC, 1);
	case TXIO_LINK_STREAM_OFF:
		return tx_isp_video_link_stream(dev - ISPDEV_TO_MISC, 0);
	case TXIO_LINK_DESTROY:
		return tx_isp_video_link_destroy(dev - ISPDEV_TO_MISC);
	case TXIO_LINK_SETUP:
		return fwc_link_setup(dev, arg);

	case TXIO_ENUM_INPUT:
		return fwc_enum_input(dev, arg);
	case TXIO_GET_INPUT:
		return fwc_get_input(dev, arg);
	case TXIO_SET_INPUT:
		return fwc_set_input(dev, arg);
	case TXIO_REGISTER_SENSOR:
		return fwc_sensor_desc(dev, arg, SD_EV_REGISTER_SENSOR,
				       "tx_isp_sensor_register_sensor", 422);
	case TXIO_RELEASE_SENSOR:
		return fwc_sensor_desc(dev, arg, SD_EV_RELEASE_SENSOR,
				       "tx_isp_sensor_release_sensor", 448);
	case TXIO_S_REGISTER:
		return fwc_s_register(dev, arg);
	case TXIO_G_REGISTER:
		return fwc_g_register(dev, arg);

	case TXIO_SET_BUF:
		return fwc_set_buf(dev, arg);
	case TXIO_GET_BUF:
		return fwc_get_buf(dev, arg);

	case TXIO_GET_AE_ALGO:
		return tx_isp_get_ae_algo_handle(FW(dev, ISPDEV_SUBDEV_FIRST, void *),
						 (void __user *)arg);
	case TXIO_SET_AE_HANDLE:
		return fwc_set_ae_handle(arg);
	case TXIO_AE_OPEN:
		return fwc_ae_open(dev, arg);
	case TXIO_AE_CLOSE:
		return fwc_ae_close(arg);

	case TXIO_GET_AWB_ALGO:
		return fwc_get_awb(arg);
	case TXIO_SET_AWB_HANDLE:
		return fwc_set_awb_handle(arg);
	case TXIO_AWB_OPEN:
		return fwc_awb_open();
	case TXIO_AWB_CLOSE:
		return fwc_awb_close(arg);

	case TXIO_GPIO_INIT:
		return fwc_gpio(dev, arg, SD_EV_GPIO_INIT, "tx_isp_set_gpio_init", 958);
	case TXIO_GPIO_STA:
		return fwc_gpio(dev, arg, SD_EV_GPIO_STA, "tx_isp_set_gpio_sta", 976);

	case TXIO_DEFAULT_BIN_SET:
		return fwc_set_bin_path(dev, arg);
	case TXIO_DEFAULT_BIN_GET:
		return fwc_get_bin_path(dev, arg);
	case TXIO_SET_FRAME_DROP:
		return fwc_set_frame_drop(arg);
	case TXIO_GET_FRAME_DROP:
		return fwc_get_frame_drop(arg);
	}
	return 0;
}
