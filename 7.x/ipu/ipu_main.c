// SPDX-License-Identifier: GPL-2.0
/*
 * Ingenic IPU (jz_ipu_v13) for mainline 7.x: T10, T20, T21, T23, T30, T31.
 *
 * libimp draws its PIC and COVER OSD regions with the IPU, one synchronous
 * job per ioctl on /dev/ipu. Ported from the vendor 3.10 drivers; the ioctl
 * ABI (ipu_ioctl.h) is unchanged. The SoCs differ in the clocks and in how
 * the vendor drivers programmed IPU_D_FMT (see struct ipu_soc).
 */
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/ingenic/rmem.h>
#include <linux/uaccess.h>

#include "ipu_ioctl.h"
#include "ipu_regs.h"
#include "rmem_cache.h"

#define IPU_BUF_SIZE		(2 * 1024 * 1024)
#define IPU_JOB_TIMEOUT_MS	2000

static bool check_addr = true;
module_param(check_addr, bool, 0444);
MODULE_PARM_DESC(check_addr, "reject jobs whose buffers lie outside rmem (default Y)");

/*
 * old_fmt: the T10, T20 and T30 vendor drivers program IPU_D_FMT with the
 * older bit layout (input format in bits 0-3, output format from bit 19,
 * NV12 background jobs write 2); the T21, T23 and T31 ones with the newer
 * one.
 */
struct ipu_soc {
	bool old_fmt;
};

struct jz_ipu {
	const struct ipu_soc *soc;
	struct device *dev;
	void __iomem *base;
	int irq;
	struct clk *clk;
	struct clk *ahb1;		/* only where the DT gives one */
	struct miscdevice misc;

	struct mutex lock;		/* one job at a time, pbuf */
	struct completion done_ipu;
	struct completion done_buf;

	phys_addr_t rmem_base;
	phys_addr_t rmem_size;
	bool have_rmem;

	void *pbuf;
	dma_addr_t pbuf_dma;
};

static bool trace;
module_param(trace, bool, 0644);
MODULE_PARM_DESC(trace, "log every job's parameters");

static struct platform_device *ipu_twin;

static const struct ipu_soc soc_old = { .old_fmt = true };
static const struct ipu_soc soc_new = { .old_fmt = false };

static const struct of_device_id ipu_of_match[] = {
	{ .compatible = "ingenic,t10-ipu", .data = &soc_old },
	{ .compatible = "ingenic,t20-ipu", .data = &soc_old },
	{ .compatible = "ingenic,t21-ipu", .data = &soc_new },
	{ .compatible = "ingenic,t23-ipu", .data = &soc_new },
	{ .compatible = "ingenic,t30-ipu", .data = &soc_old },
	{ .compatible = "ingenic,t31-ipu", .data = &soc_new },
	{ }
};
MODULE_DEVICE_TABLE(of, ipu_of_match);

static inline u32 ipu_rd(struct jz_ipu *ipu, unsigned int off)
{
	return readl(ipu->base + off);
}

static inline void ipu_wr(struct jz_ipu *ipu, unsigned int off, u32 val)
{
	writel(val, ipu->base + off);
}

static void ipu_set(struct jz_ipu *ipu, unsigned int off, u32 bits)
{
	ipu_wr(ipu, off, ipu_rd(ipu, off) | bits);
}

static void ipu_clr(struct jz_ipu *ipu, unsigned int off, u32 bits)
{
	ipu_wr(ipu, off, ipu_rd(ipu, off) & ~bits);
}

/* -1 for formats the OSD channels cannot take, else 0 YUV, 1 ARGB, 2 5551 */
static int ipu_osd_kind(u32 fmt)
{
	switch (fmt) {
	case HAL_PIXEL_FORMAT_ARGB_8888:
	case HAL_PIXEL_FORMAT_ABGR_8888:
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_RGBX_8888:
	case HAL_PIXEL_FORMAT_BGRA_8888:
	case HAL_PIXEL_FORMAT_BGRX_8888:
		return 1;
	case HAL_PIXEL_FORMAT_NV12:
	case HAL_PIXEL_FORMAT_NV21:
	case HAL_PIXEL_FORMAT_HSV:
		return 0;
	case HAL_PIXEL_FORMAT_BGRA_5551:
	case HAL_PIXEL_FORMAT_RGBA_5551:
		return 2;
	default:
		return -1;
	}
}

static u32 ipu_in_fmt(u32 hal, int *isrgb)
{
	u32 fmt = IN_FMT_YUV420;
	int rgb = 0;

	switch (hal) {
	case HAL_PIXEL_FORMAT_YCbCr_422_SP:
	case HAL_PIXEL_FORMAT_YCbCr_422_P:
	case HAL_PIXEL_FORMAT_YCbCr_422_I:
		fmt = IN_FMT_YUV422;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_420_B:
	case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
		fmt = IN_FMT_YUV420_B;
		break;
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_RGBX_8888:
	case HAL_PIXEL_FORMAT_RGB_888:
	case HAL_PIXEL_FORMAT_BGRA_8888:
	case HAL_PIXEL_FORMAT_ARGB_8888:
	case HAL_PIXEL_FORMAT_ABGR_8888:
	case HAL_PIXEL_FORMAT_BGRX_8888:
		fmt = IN_FMT_RGB_888;
		rgb = 1;
		break;
	}
	*isrgb = rgb;
	return fmt;
}

static u32 ipu_out_fmt(u32 hal)
{
	switch (hal) {
	case HAL_PIXEL_FORMAT_ARGB_8888:
		return OUT_FMT_ARGB_8888;
	case HAL_PIXEL_FORMAT_ABGR_8888:
		return OUT_FMT_ABGR_8888;
	case HAL_PIXEL_FORMAT_BGRA_8888:
		return OUT_FMT_BGRA_8888;
	case HAL_PIXEL_FORMAT_NV12:
		return OUT_FMT_NV12;
	case HAL_PIXEL_FORMAT_HSV:
		return OUT_FMT_HSV;
	case HAL_PIXEL_FORMAT_NV21:
		return OUT_FMT_NV21;
	default:
		return OUT_FMT_RGBA_8888;
	}
}

static bool ipu_range_ok(struct jz_ipu *ipu, u32 addr, u64 len)
{
	if (ipu->pbuf && addr >= ipu->pbuf_dma &&
	    addr - ipu->pbuf_dma <= IPU_BUF_SIZE &&
	    len <= IPU_BUF_SIZE - (addr - ipu->pbuf_dma))
		return true;
	if (!ipu->have_rmem)
		return true;
	return addr >= ipu->rmem_base && len <= ipu->rmem_size &&
	       addr - ipu->rmem_base <= ipu->rmem_size - len;
}

static int ipu_osd_bytes(u32 fmt, u32 w, u32 h, u64 *len)
{
	int kind = ipu_osd_kind(fmt);

	if (kind < 0)
		return -EINVAL;
	*len = (u64)w * h * (kind == 1 ? 4 : kind == 2 ? 2 : 1);
	if (kind == 0)
		*len += (u64)w * h / 2;
	return 0;
}

/*
 * The IPU is a bus master fed with physical addresses from userspace, so
 * every buffer has to lie in rmem (where libimp allocates all of them) or in
 * the driver's own buffer. A zero address is a COVER layer: no picture read.
 */
static int ipu_check_param(struct jz_ipu *ipu, const struct ipu_param *ip)
{
	u64 bg = (u64)ip->bg_w * ip->bg_h, len;
	int i;

	if (!ip->bg_w || !ip->bg_h || ip->bg_w > 0xffff || ip->bg_h > 0xffff)
		return -EINVAL;
	if (!check_addr)
		return 0;

	if (!ipu_range_ok(ipu, ip->bg_buf_p, bg + bg / 2))
		return -EINVAL;
	if (ip->cmd & IPU_CMD_CSC) {
		/* the output goes to osd[0].buf_p; the IPU writes one row more */
		len = bg + ip->bg_w;
		switch (ip->out_fmt) {
		case HAL_PIXEL_FORMAT_NV12:
		case HAL_PIXEL_FORMAT_NV21:
			len += bg / 2;
			break;
		default:
			len += 3 * bg;
		}
		if (!ipu_range_ok(ipu, ip->osd[0].buf_p, len))
			return -EINVAL;
	}
	for (i = 0; i < 4; i++) {
		const struct ipu_osd_ch *ch = &ip->osd[i];

		if (!(ip->cmd & (IPU_CMD_OSD0 << i)) || !ch->buf_p)
			continue;
		if (ch->src_w > 0xffff || ch->src_h > 0xffff ||
		    ipu_osd_bytes(ch->fmt, ch->src_w, ch->src_h, &len) ||
		    !ipu_range_ok(ipu, ch->buf_p, len))
			return -EINVAL;
	}
	return 0;
}

enum old_out { OLD_RGB888, OLD_RGB565, OLD_RGB555, OLD_YUV422, OLD_NV12, OLD_NV21, OLD_HSV };

static enum old_out ipu_old_out(u32 hal)
{
	switch (hal) {
	case HAL_PIXEL_FORMAT_RGB_565:
		return OLD_RGB565;
	case HAL_PIXEL_FORMAT_RGBA_5551:
	case HAL_PIXEL_FORMAT_BGRA_5551:
		return OLD_RGB555;
	case HAL_PIXEL_FORMAT_YCbCr_422_I:
		return OLD_YUV422;
	case HAL_PIXEL_FORMAT_NV12:
		return OLD_NV12;
	case HAL_PIXEL_FORMAT_NV21:
		return OLD_NV21;
	case HAL_PIXEL_FORMAT_HSV:
		return OLD_HSV;
	default:
		return OLD_RGB888;
	}
}

/* IPU_D_FMT as the T10, T20 and T30 vendor drivers wrote it */
static u32 ipu_dfmt_old(const struct ipu_param *ip, u32 infmt)
{
	u32 in, out, rgb = 0;

	if (ip->cmd & IPU_CMD_OSD) {
		if (ip->bg_fmt == HAL_PIXEL_FORMAT_NV12)
			return 2;
		if (ip->bg_fmt == HAL_PIXEL_FORMAT_NV21)
			return 3;
		switch (infmt) {
		case IN_FMT_YUV422:
			in = 1;
			break;
		case IN_FMT_YUV420_B:
			in = 4;
			break;
		case IN_FMT_YUV444:
		case IN_FMT_RGB_888:
			in = 2;
			break;
		case IN_FMT_YUV411:
			in = 3;
			break;
		default:
			in = 0;
		}
		switch (ipu_old_out(ip->out_fmt)) {
		case OLD_RGB565:
			out = 1 << 19;
			break;
		case OLD_RGB555:
			out = 0;
			break;
		case OLD_YUV422:
			out = 3 << 19;
			break;
		case OLD_NV12:
			out = 6 << 19;
			break;
		case OLD_NV21:
			out = 7 << 19;
			break;
		case OLD_HSV:
			out = 1 << 2;
			break;
		default:
			out = 2 << 19;
		}
		if (ip->out_fmt == HAL_PIXEL_FORMAT_RGBX_8888)
			rgb = 5 << 22;
		return in | out | rgb;
	}

	switch (ipu_old_out(ip->out_fmt)) {
	case OLD_HSV:
		return 0x4;
	case OLD_RGB888:
		return 0x40;
	case OLD_NV21:
		return 0x3;
	default:
		return 0;
	}
}

static int ipu_set_bg_route(struct jz_ipu *ipu, const struct ipu_param *ip)
{
	u32 srcw = ip->bg_w, srch = ip->bg_h, val = 0, outfmt, infmt;
	int bgrgb;

	infmt = ipu_in_fmt(ip->bg_fmt, &bgrgb);
	outfmt = ipu_out_fmt(ip->out_fmt);

	ipu_wr(ipu, IPU_IN_FM_GS, srcw << 16 | srch);
	if (ip->cmd & IPU_CMD_OSD)
		ipu_wr(ipu, IPU_OSD_B_BAK_ARGB, 0xffaa7733);

	if (ipu->soc->old_fmt)
		val = ipu_dfmt_old(ip, infmt);
	else switch (outfmt) {
	case OUT_FMT_HSV:
		val = 0x4;
		break;
	case OUT_FMT_ARGB_8888:
		val = 0x0;
		break;
	case OUT_FMT_ABGR_8888:
		val = 0x28;
		break;
	case OUT_FMT_RGBA_8888:
		val = 0x40;
		break;
	case OUT_FMT_BGRA_8888:
		val = 0x68;
		break;
	case OUT_FMT_NV21:
		val = 0x3;
		break;
	}
	ipu_wr(ipu, IPU_D_FMT, val);

	if (infmt == IN_FMT_YUV420_B)
		ipu_set(ipu, IPU_D_FMT, IPU_D_FMT_BLK_SEL);

	switch (ip->out_fmt) {
	case HAL_PIXEL_FORMAT_NV12:
	case HAL_PIXEL_FORMAT_NV21:
		ipu_wr(ipu, IPU_OUT_STRIDE, srcw);
		ipu_wr(ipu, IPU_NV_OUT_STRIDE, srcw);
		break;
	case HAL_PIXEL_FORMAT_HSV:
		ipu_wr(ipu, IPU_OUT_STRIDE, srcw);
		ipu_wr(ipu, IPU_NV_OUT_STRIDE, srcw * 2);
		ipu_wr(ipu, IPU_OUT_V_STRIDE, srcw);
		break;
	case HAL_PIXEL_FORMAT_BGRA_8888:
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_ARGB_8888:
	case HAL_PIXEL_FORMAT_ABGR_8888:
		ipu_wr(ipu, IPU_OUT_STRIDE, srcw << 2);
		break;
	default:
		if (!ipu->soc->old_fmt)
			ipu_wr(ipu, IPU_OUT_STRIDE, srcw);
		else if (ipu_old_out(ip->out_fmt) == OLD_RGB888)
			ipu_wr(ipu, IPU_OUT_STRIDE, srcw << 2);
		else if (ipu_old_out(ip->out_fmt) == OLD_RGB565 ||
			 ipu_old_out(ip->out_fmt) == OLD_RGB555)
			ipu_wr(ipu, IPU_OUT_STRIDE, srcw << 1);
		else
			ipu_wr(ipu, IPU_OUT_STRIDE, srcw);
	}

	if (infmt == IN_FMT_YUV420_B)
		ipu_wr(ipu, IPU_Y_STRIDE, srcw * 16);
	else if (ipu_rd(ipu, IPU_FM_CTRL) & IPU_FM_CTRL_STRIDE2)
		ipu_wr(ipu, IPU_Y_STRIDE, srcw * 2);
	else
		ipu_wr(ipu, IPU_Y_STRIDE, srcw);

	switch (infmt) {
	case IN_FMT_YUV420:
	case IN_FMT_YUV444:
		val = srcw << 16 | srcw;
		break;
	case IN_FMT_YUV422:
		val = (srcw / 2) << 16 | srcw / 2;
		break;
	case IN_FMT_YUV420_B:
		val = (8 * srcw) << 16 | 8 * srcw;
		break;
	case IN_FMT_YUV411:
		val = (srcw / 4) << 16 | srcw / 4;
		break;
	default:
		dev_err(ipu->dev, "unsupported background format 0x%x\n", ip->bg_fmt);
		return -EINVAL;
	}
	ipu_wr(ipu, IPU_UV_STRIDE, val);

	if (bgrgb) {
		switch (ip->bg_fmt) {
		case HAL_PIXEL_FORMAT_ARGB_8888:
			val = BK_ARGB_TYPE(0x0);
			break;
		case HAL_PIXEL_FORMAT_ABGR_8888:
			val = BK_ARGB_TYPE(0x5);
			break;
		case HAL_PIXEL_FORMAT_RGBA_8888:
			val = BK_ARGB_TYPE(0x8);
			break;
		default:
			val = BK_ARGB_TYPE(0xd);
		}
		val |= BK_PIC_ARGB;
	} else {
		val = ip->bg_fmt == HAL_PIXEL_FORMAT_NV21 ? BK_PIC_NV21 : BK_PIC_NV12;
	}
	if (ip->cmd & IPU_CMD_OSD)
		val |= BK_PREM;
	ipu_wr(ipu, IPU_OSD_BK_PARA, val | BK_EN);
	return 0;
}

static void ipu_set_bg_buffer(struct jz_ipu *ipu, const struct ipu_param *ip)
{
	u32 y = ip->bg_buf_p, uv = y + ip->bg_w * ip->bg_h;
	u32 out = ip->osd[0].buf_p, wh = ip->bg_w * ip->bg_h;

	ipu_wr(ipu, IPU_Y_ADDR, y);
	ipu_wr(ipu, IPU_U_ADDR, uv);

	if (!(ip->cmd & IPU_CMD_CSC)) {
		ipu_wr(ipu, IPU_OUT_ADDR, y);
		ipu_wr(ipu, IPU_NV_OUT_ADDR, uv);
	} else if (ip->out_fmt == HAL_PIXEL_FORMAT_HSV) {
		ipu_wr(ipu, IPU_OUT_ADDR, out + wh * 2);
		ipu_wr(ipu, IPU_NV_OUT_ADDR, out);
		ipu_wr(ipu, IPU_OUT_V_ADDR, out + wh * 3);
	} else if (ip->out_fmt == HAL_PIXEL_FORMAT_NV21) {
		ipu_wr(ipu, IPU_OUT_ADDR, out);
		ipu_wr(ipu, IPU_NV_OUT_ADDR, out + wh);
	} else {
		ipu_wr(ipu, IPU_OUT_ADDR, out);
	}
}

/* Returns 0, or 1 when the channel lies outside the frame and stays off */
static int ipu_set_osd_ch(struct jz_ipu *ipu, const struct ipu_param *ip, int n)
{
	const struct ipu_osd_ch *ch = &ip->osd[n];
	int kind = ipu_osd_kind(ch->fmt);
	s64 srcw, srch;
	u32 para;

	if (kind < 0) {
		dev_err(ipu->dev, "osd channel %d: format 0x%x unsupported\n", n, ch->fmt);
		return -EINVAL;
	}

	/* cut to the frame: most vendor drivers did not and let the IPU write past it */
	srcw = (s64)ip->bg_w > (s64)ch->pos_x + ch->src_w ?
	       (s64)ch->src_w : (s64)ip->bg_w - ch->pos_x;
	srch = (s64)ip->bg_h > (s64)ch->pos_y + ch->src_h ?
	       (s64)ch->src_h : (s64)ip->bg_h - ch->pos_y;
	if (srcw <= 0 || srch <= 0)
		return 1;

	if (kind == 1) {
		ipu_wr(ipu, IPU_OSD_IN_Y_STRIDE(n), ch->src_w * 4);
		ipu_wr(ipu, IPU_OSD_GS(n), srcw << 18 | srch);
	} else if (kind == 2) {
		ipu_wr(ipu, IPU_OSD_IN_Y_STRIDE(n), ch->src_w * 2);
		ipu_wr(ipu, IPU_OSD_GS(n), srcw << 17 | srch);
	} else {
		ipu_wr(ipu, IPU_OSD_IN_Y_STRIDE(n), ch->src_w);
		ipu_wr(ipu, IPU_OSD_IN_UV_STRIDE(n), ch->src_w);
		ipu_wr(ipu, IPU_OSD_GS(n), srcw << 16 | srch);
	}
	ipu_wr(ipu, IPU_OSD_POS(n), ch->pos_x << 16 | ch->pos_y);

	if (kind > 0) {
		ipu_wr(ipu, IPU_CSC_C(n, 0), 0x133 | 0x25a << 20);
		ipu_wr(ipu, IPU_CSC_C(n, 1), 0x75 | 0x97 << 20);
		ipu_wr(ipu, IPU_CSC_C(n, 2), 0x128 | 0x1bf << 20);
		ipu_wr(ipu, IPU_CSC_C(n, 3), 0x276 | 0x210 << 20);
		ipu_wr(ipu, IPU_CSC_C(n, 4), 0x67);
	} else {
		ipu_wr(ipu, IPU_CSC_C(n, 0), 0x400);
		ipu_wr(ipu, IPU_CSC_C(n, 1), 0x490 | 0x400 << 20);
		ipu_wr(ipu, IPU_CSC_C(n, 2), 0x190 | 0x252 << 20);
		ipu_wr(ipu, IPU_CSC_C(n, 3), 0x400 | 0x81f << 20);
		ipu_wr(ipu, IPU_CSC_C(n, 4), 0);
	}
	ipu_wr(ipu, IPU_CSC_OFSET(n), 0x10 | 0x80 << 16);

	ipu_wr(ipu, IPU_OSD_BAK_ARGB(n), ch->bak_argb);
	para = ch->para ? ch->para :
	       (0x1 | 0x2 << 11 | 0x1 << 1 | 0x32 << 3);
	ipu_wr(ipu, IPU_OSD_PARA(n), para);

	ipu_wr(ipu, IPU_OSD_IN_Y_ADDR(n), ch->buf_p);
	ipu_wr(ipu, IPU_OSD_IN_UV_ADDR(n), ch->buf_p + ch->src_w * ch->src_h);
	return 0;
}

static int ipu_start(struct jz_ipu *ipu, const struct ipu_param *ip)
{
	long left;
	int i, ret;

	if (!(ip->cmd & (IPU_CMD_OSD | IPU_CMD_CSC))) {
		dev_err(ipu->dev, "bad command 0x%08x\n", ip->cmd);
		return -EINVAL;
	}
	if (trace) {
		int k;

		dev_info(ipu->dev, "job cmd %x bg %ux%u fmt %x buf %x out %x\n", ip->cmd,
			 ip->bg_w, ip->bg_h, ip->bg_fmt, ip->bg_buf_p, ip->out_fmt);
		for (k = 0; k < 4; k++)
			if (ip->cmd & (IPU_CMD_OSD0 << k))
				dev_info(ipu->dev, " ch%d fmt %x para %08x bak %08x pos %u,%u %ux%u buf %x\n", k,
					 ip->osd[k].fmt, ip->osd[k].para, ip->osd[k].bak_argb, ip->osd[k].pos_x,
					 ip->osd[k].pos_y, ip->osd[k].src_w, ip->osd[k].src_h, ip->osd[k].buf_p);
	}
	ret = ipu_check_param(ipu, ip);
	if (ret) {
		dev_err_ratelimited(ipu->dev, "job rejected: bad size or buffer outside rmem\n");
		return ret;
	}

	ipu_set(ipu, IPU_TRIGGER, IPU_STOP);
	ipu_set(ipu, IPU_TRIGGER, IPU_RESET);

	ret = ipu_set_bg_route(ipu, ip);
	if (ret)
		return ret;
	ipu_set_bg_buffer(ipu, ip);

	if (ip->cmd & IPU_CMD_CSC) {
		ipu_wr(ipu, IPU_REG_CTRL, 0xffffffff);
		ipu_wr(ipu, IPU_OSD_PARA(0), 0x010417fa);
		ipu_wr(ipu, IPU_OSD_BK_PARA, 0x141000);
		ipu_wr(ipu, IPU_CSC_C(0, 0), 0x4ad);
		ipu_wr(ipu, IPU_CSC_C(0, 1), 0x669 | 0x4ad << 20);
		ipu_wr(ipu, IPU_CSC_C(0, 2), 0x193 | 0x344 << 20);
		ipu_wr(ipu, IPU_CSC_C(0, 3), 0x4ad | 0x81a << 20);
		ipu_wr(ipu, IPU_CSC_C(0, 4), 0);
		ipu_wr(ipu, IPU_CSC_OFSET(0), 0x10 | 0x80 << 16);
	}

	for (i = 0; i < 4; i++) {
		if (!(ip->cmd & (IPU_CMD_OSD0 << i))) {
			if (!(ip->cmd & IPU_CMD_CSC) || i)
				ipu_wr(ipu, IPU_OSD_PARA(i), 0);
			continue;
		}
		ret = ipu_set_osd_ch(ipu, ip, i);
		if (ret < 0)
			return ret;
		if (ret)
			ipu_wr(ipu, IPU_OSD_PARA(i), 0);
	}

	/* a late interrupt of a timed-out job must not complete this one */
	reinit_completion(&ipu->done_ipu);
	ipu_clr(ipu, IPU_STATUS, IPU_OUT_END);
	ipu_set(ipu, IPU_GLB_CTRL, IPU_IRQ_EN);
	ipu_wr(ipu, IPU_REG_CTRL, 0xffffffff);
	ipu_set(ipu, IPU_TRIGGER, IPU_RUN);

	left = wait_for_completion_interruptible_timeout(&ipu->done_ipu,
				msecs_to_jiffies(IPU_JOB_TIMEOUT_MS));
	if (left > 0)
		return 0;
	ipu_clr(ipu, IPU_GLB_CTRL, IPU_IRQ_EN);
	if (left < 0)
		return left;
	dev_err(ipu->dev, "job timed out, status 0x%08x\n", ipu_rd(ipu, IPU_STATUS));
	return -ETIMEDOUT;
}

/*
 * IOCTL_IPU_BUF_FLUSH_CACHE: the vendor driver called dma_cache_sync()
 * (writeback+invalidate) on the address: a user address inside libimp's
 * /dev/rmem mapping (see rmem_cache.h) or the driver's own buffer by its
 * kernel address (vaddr_alloc), which is coherent here and needs nothing.
 */
static int ipu_flush(struct jz_ipu *ipu, const struct ipu_flush_cache_para *fc)
{
	unsigned long base = (unsigned long)ipu->pbuf;
	int ret;

	if (trace)
		dev_info(ipu->dev, "flush %08x+%x\n", fc->addr, fc->size);
	if (ipu->pbuf && fc->addr >= base && fc->addr - base < IPU_BUF_SIZE &&
	    fc->size <= IPU_BUF_SIZE - (fc->addr - base))
		return 0;
	ret = rmem_cache_sync_user(fc->addr, fc->size, DMA_BIDIRECTIONAL);
	if (ret)
		dev_warn_ratelimited(ipu->dev,
				     "flush %08x+%x: neither the IPU buffer nor a user mapping (%d)\n",
				     fc->addr, fc->size, ret);
	return ret;
}

static long ipu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct jz_ipu *ipu = container_of(filp->private_data, struct jz_ipu, misc);
	void __user *uarg = (void __user *)arg;
	long ret = 0;

	if (_IOC_TYPE(cmd) != JZIPU_IOC_MAGIC)
		return -EFAULT;

	switch (cmd) {
	case IOCTL_IPU_START: {
		struct ipu_param ip;

		if (copy_from_user(&ip, uarg, sizeof(ip)))
			return -EFAULT;
		mutex_lock(&ipu->lock);
		ret = ipu_start(ipu, &ip);
		mutex_unlock(&ipu->lock);
		return ret;
	}
	case IOCTL_IPU_GET_PBUFF: {
		struct ipu_buf_info bi;

		mutex_lock(&ipu->lock);
		if (!ipu->pbuf) {
			ipu->pbuf = dma_alloc_coherent(ipu->dev, IPU_BUF_SIZE,
						       &ipu->pbuf_dma, GFP_KERNEL);
			if (!ipu->pbuf)
				ret = -ENOMEM;
		}
		if (!ret) {
			bi.vaddr_alloc = (u32)(unsigned long)ipu->pbuf;
			bi.paddr = bi.paddr_align = ipu->pbuf_dma;
			bi.size = IPU_BUF_SIZE;
		}
		mutex_unlock(&ipu->lock);
		if (!ret && copy_to_user(uarg, &bi, sizeof(bi)))
			ret = -EFAULT;
		return ret;
	}
	case IOCTL_IPU_RES_PBUFF:
		mutex_lock(&ipu->lock);
		if (ipu->pbuf) {
			dma_free_coherent(ipu->dev, IPU_BUF_SIZE, ipu->pbuf, ipu->pbuf_dma);
			ipu->pbuf = NULL;
		}
		mutex_unlock(&ipu->lock);
		return 0;
	case IOCTL_IPU_BUF_LOCK:
		ret = wait_for_completion_interruptible_timeout(&ipu->done_buf,
					msecs_to_jiffies(IPU_JOB_TIMEOUT_MS));
		if (ret < 0)
			return ret;
		return ret ? 0 : -ETIMEDOUT;
	case IOCTL_IPU_BUF_UNLOCK:
		complete(&ipu->done_buf);
		return 0;
	case IOCTL_IPU_BUF_FLUSH_CACHE: {
		struct ipu_flush_cache_para fc;

		if (copy_from_user(&fc, uarg, sizeof(fc)))
			return -EFAULT;
		return ipu_flush(ipu, &fc);
	}
	default:
		dev_err_ratelimited(ipu->dev, "invalid command 0x%08x\n", cmd);
		return -EINVAL;
	}
}

static const struct file_operations ipu_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ipu_ioctl,
};

static irqreturn_t ipu_irq(int irq, void *data)
{
	struct jz_ipu *ipu = data;

	if (!(ipu_rd(ipu, IPU_GLB_CTRL) & IPU_IRQ_EN))
		return IRQ_NONE;
	ipu_clr(ipu, IPU_GLB_CTRL, IPU_IRQ_EN);
	if (ipu_rd(ipu, IPU_STATUS) & IPU_OUT_END)
		complete(&ipu->done_ipu);
	return IRQ_HANDLED;
}

static int ipu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct jz_ipu *ipu;
	int ret;

	ipu = devm_kzalloc(dev, sizeof(*ipu), GFP_KERNEL);
	if (!ipu)
		return -ENOMEM;
	ipu->dev = dev;
	ipu->soc = of_device_get_match_data(dev);
	if (!ipu->soc)
		return -ENODEV;
	mutex_init(&ipu->lock);
	init_completion(&ipu->done_ipu);
	init_completion(&ipu->done_buf);
	complete(&ipu->done_buf);
	ipu->have_rmem = !ingenic_rmem_region(&ipu->rmem_base, &ipu->rmem_size);
	if (!ipu->have_rmem && check_addr)
		dev_warn(dev, "no rmem= region: buffer addresses are not checked\n");

	ipu->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ipu->base))
		return PTR_ERR(ipu->base);
	ipu->irq = platform_get_irq(pdev, 0);
	if (ipu->irq < 0)
		return ipu->irq;

	ipu->clk = devm_clk_get(dev, "ipu");
	if (IS_ERR(ipu->clk))
		return dev_err_probe(dev, PTR_ERR(ipu->clk), "ipu clock\n");
	ipu->ahb1 = devm_clk_get_optional(dev, "ahb1");
	if (IS_ERR(ipu->ahb1))
		return dev_err_probe(dev, PTR_ERR(ipu->ahb1), "ahb1 clock\n");

	ret = clk_prepare_enable(ipu->ahb1);
	if (ret)
		return ret;
	ret = clk_prepare_enable(ipu->clk);
	if (ret)
		goto err_ahb1;

	ipu_wr(ipu, IPU_GLB_CTRL, ipu_rd(ipu, IPU_GLB_CTRL) & ~IPU_IRQ_EN);
	ipu_set(ipu, IPU_TRIGGER, IPU_STOP);
	ipu_set(ipu, IPU_TRIGGER, IPU_RESET);

	ret = request_irq(ipu->irq, ipu_irq, IRQF_SHARED, dev_name(dev), ipu);
	if (ret)
		goto err_clk;

	ipu->misc.minor = MISC_DYNAMIC_MINOR;
	ipu->misc.name = "ipu";
	ipu->misc.fops = &ipu_fops;
	ipu->misc.parent = dev;
	ret = misc_register(&ipu->misc);
	if (ret)
		goto err_irq;

	platform_set_drvdata(pdev, ipu);
	dev_info(dev, "irq %d, ipu clock %lu Hz\n", ipu->irq, clk_get_rate(ipu->clk));
	return 0;

err_irq:
	free_irq(ipu->irq, ipu);
err_clk:
	clk_disable_unprepare(ipu->clk);
err_ahb1:
	clk_disable_unprepare(ipu->ahb1);
	return ret;
}

static void ipu_remove(struct platform_device *pdev)
{
	struct jz_ipu *ipu = platform_get_drvdata(pdev);

	misc_deregister(&ipu->misc);
	free_irq(ipu->irq, ipu);
	if (ipu->pbuf)
		dma_free_coherent(ipu->dev, IPU_BUF_SIZE, ipu->pbuf, ipu->pbuf_dma);
	clk_disable_unprepare(ipu->clk);
	clk_disable_unprepare(ipu->ahb1);
}

static struct platform_driver ipu_driver = {
	.probe = ipu_probe,
	.remove = ipu_remove,
	.driver = {
		.name = "jz-ipu",
		.of_match_table = ipu_of_match,
	},
};

/*
 * The DT node is disabled on this kernel, so no platform device exists for
 * it. If it is ever enabled the driver binds natively; otherwise a device is
 * made here from the same node, with the node attached so the clocks and the
 * interrupt come from the DT.
 */
static int ipu_create_twin(struct device_node *np)
{
	struct resource res;
	int ret;

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		return ret;

	ipu_twin = platform_device_alloc("jz-ipu", 0);
	if (!ipu_twin)
		return -ENOMEM;
	ret = platform_device_add_resources(ipu_twin, &res, 1);
	if (ret)
		goto err;
	device_set_node(&ipu_twin->dev, of_fwnode_handle(of_node_get(np)));
	ipu_twin->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	ipu_twin->dev.dma_mask = &ipu_twin->dev.coherent_dma_mask;
	ret = platform_device_add(ipu_twin);
	if (ret)
		goto err;
	return 0;
err:
	platform_device_put(ipu_twin);
	ipu_twin = NULL;
	return ret;
}

static int __init ipu_init(void)
{
	struct device_node *np;
	int ret;

	np = of_find_matching_node(NULL, ipu_of_match);
	if (!np) {
		pr_err("ipu: no ingenic,tNN-ipu node in the device tree\n");
		return -ENODEV;
	}
	if (!of_device_is_available(np)) {
		ret = ipu_create_twin(np);
		if (ret) {
			of_node_put(np);
			return ret;
		}
	}
	of_node_put(np);

	ret = platform_driver_register(&ipu_driver);
	if (ret && ipu_twin) {
		platform_device_unregister(ipu_twin);
		ipu_twin = NULL;
	}
	return ret;
}

static void __exit ipu_exit(void)
{
	platform_driver_unregister(&ipu_driver);
	if (ipu_twin)
		platform_device_unregister(ipu_twin);
}

module_init(ipu_init);
module_exit(ipu_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ingenic IPU (OSD) driver, T10 to T31");
