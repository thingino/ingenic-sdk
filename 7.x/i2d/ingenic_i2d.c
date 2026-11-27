// SPDX-License-Identifier: GPL-2.0
/*
 * Ingenic I2D (rotate, mirror, flip) for libimp, /dev/i2d: the vendor T40
 * and T41 4.4.94 driver (drivers/video/ingenic_i2d) on Linux 7.x.
 *
 * Copyright (c) 2015 Ingenic Semiconductor Co., Ltd.
 *
 * The two vendor releases differ in their ioctl argument: the T41 one adds
 * the strides and the height alignment, which its libimp computes for NV12;
 * the T40 driver derives the NV12 strides itself and aligns to 16 lines.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "ingenic_i2d.h"
#include "rmem_cache.h"

#define I2D_BUF_SIZE		SZ_2M
#define I2D_JOB_TIMEOUT_MS	2000

struct i2d_variant {
	size_t param_size;	/* bytes of struct i2d_param libimp passes */
	bool user_strides;	/* NV12 strides and h_align come from libimp */
};

struct jz_i2d {
	const struct i2d_variant *variant;
	void __iomem *iomem;
	struct device *dev;
	struct clk *clk;
	int irq;
	struct miscdevice misc;
	struct mutex mutex;		/* one ioctl at a time: jobs and the buffer */
	struct completion done;
	struct i2d_buf_info pbuf;
	void *pbuf_virt;
};

static const char * const i2d_reg_names[] = {
	[I2D_RTL_VERSION / 4] = "VERSION",
	[I2D_SHD_CTRL / 4] = "SHD_CTRL",
	[I2D_CTRL / 4] = "CTRL",
	[I2D_IMG_SIZE / 4] = "IMG_SIZE",
	[I2D_IMG_MODE / 4] = "IMG_MODE",
	[I2D_SRC_ADDR_Y / 4] = "SRC_ADDR_Y",
	[I2D_SRC_ADDR_UV / 4] = "SRC_ADDR_UV",
	[I2D_SRC_Y_STRID / 4] = "SRC_Y_STRID",
	[I2D_SRC_UV_STRID / 4] = "SRC_UV_STRID",
	[I2D_DST_ADDR_Y / 4] = "DST_ADDR_Y",
	[I2D_DST_ADDR_UV / 4] = "DST_ADDR_UV",
	[I2D_DST_Y_STRID / 4] = "DST_Y_STRID",
	[I2D_DST_UV_STRID / 4] = "DST_UV_STRID",
	[I2D_IRQ_STATE / 4] = "IRQ_STATE",
	[I2D_IRQ_CLEAR / 4] = "IRQ_CLEAR",
	[I2D_IRQ_MASK / 4] = "IRQ_MASK",
	[I2D_TIMEOUT_VALUE / 4] = "TIMEOUT_VALUE",
	[I2D_TIMEOUT_MODE / 4] = "TIMEOUT_MODE",
	[I2D_CLK_GATE / 4] = "CLK_GATE",
	[I2D_DBG_0 / 4] = "DBG_0",
};

static u32 i2d_rd(struct jz_i2d *i2d, unsigned int reg)
{
	return readl(i2d->iomem + reg);
}

static void i2d_wr(struct jz_i2d *i2d, unsigned int reg, u32 val)
{
	writel(val, i2d->iomem + reg);
}

static void i2d_set(struct jz_i2d *i2d, unsigned int reg, u32 bits)
{
	i2d_wr(i2d, reg, i2d_rd(i2d, reg) | bits);
}

static void i2d_dump_regs(struct jz_i2d *i2d)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(i2d_reg_names); i++)
		if (i2d_reg_names[i])
			dev_info(i2d->dev, "%-14s 0x%08x\n", i2d_reg_names[i],
				 i2d_rd(i2d, i * 4));
}

static u32 round_up_to(u32 val, u32 rnd)
{
	return (val + rnd - 1) / rnd * rnd;
}

/* strides of the packed formats (and of NV12 on the T40): from the size */
static void i2d_derive_strides(const struct i2d_param *ip, unsigned int bpp,
			       u32 *src_y, u32 *src_uv, u32 *dst_y, u32 *dst_uv)
{
	*src_y = ip->src_w * bpp;
	*src_uv = ip->src_w;
	if (ip->rotate_angle == 0 || ip->rotate_angle == 180 ||
	    ip->flip_enable == 1 || ip->mirr_enable == 1) {
		*dst_y = ip->src_w * bpp;
		*dst_uv = ip->src_w;
	} else if ((ip->rotate_angle == 90 || ip->rotate_angle == 270) &&
		   ip->rotate_enable == 1) {
		*dst_y = ip->src_h * bpp;
		*dst_uv = ip->src_h;
	}
}

static int i2d_reg_set(struct jz_i2d *i2d, const struct i2d_param *ip)
{
	u32 srcw = ip->src_w, srch = ip->src_h;
	u32 src_y_strid, src_uv_strid, dst_y_strid, dst_uv_strid;
	u32 h_align, dst_uv, mode;

	if (i2d->variant->user_strides) {
		src_y_strid = ip->src_y_strid;
		src_uv_strid = ip->src_uv_strid;
		dst_y_strid = ip->dst_y_strid;
		dst_uv_strid = ip->dst_uv_strid;
		h_align = ip->h_align;
		if (h_align != 8 && h_align != 16) {
			dev_err(i2d->dev, "h_align %u is not 8 or 16\n", h_align);
			return -EINVAL;
		}
	} else {
		dst_y_strid = srcw;
		dst_uv_strid = srcw;
		src_y_strid = 0;
		src_uv_strid = 0;
		h_align = 16;
	}

	switch (ip->data_type) {
	case PIX_FMT_NV12:
		if (!i2d->variant->user_strides)
			i2d_derive_strides(ip, 1, &src_y_strid, &src_uv_strid,
					   &dst_y_strid, &dst_uv_strid);
		mode = I2D_DATA_TYPE_NV12;
		break;
	case HAL_PIXEL_FORMAT_RAW8:
		i2d_derive_strides(ip, 1, &src_y_strid, &src_uv_strid,
				   &dst_y_strid, &dst_uv_strid);
		mode = I2D_DATA_TYPE_RAW8;
		break;
	case HAL_PIXEL_FORMAT_RGB_565:
		i2d_derive_strides(ip, 2, &src_y_strid, &src_uv_strid,
				   &dst_y_strid, &dst_uv_strid);
		mode = I2D_DATA_TYPE_RGB565;
		break;
	case HAL_PIXEL_FORMAT_ARGB_8888:
		i2d_derive_strides(ip, 4, &src_y_strid, &src_uv_strid,
				   &dst_y_strid, &dst_uv_strid);
		mode = I2D_DATA_TYPE_ARGB8888;
		break;
	default:
		i2d_derive_strides(ip, 1, &src_y_strid, &src_uv_strid,
				   &dst_y_strid, &dst_uv_strid);
		mode = I2D_DATA_TYPE_NV12;
		break;
	}
	i2d_wr(i2d, I2D_IMG_MODE, mode);
	i2d_wr(i2d, I2D_IMG_SIZE, srcw << I2D_WIDTH | srch << I2D_HEIGHT);

	dst_uv = ip->dst_addr_y + dst_y_strid * round_up_to(srch, h_align);
	if (ip->rotate_enable == 1) {
		switch (ip->rotate_angle) {
		case 0:
			mode = I2D_FLIP_MODE_0;
			break;
		case 90:
			mode = I2D_FLIP_MODE_90;
			break;
		case 180:
			mode = I2D_FLIP_MODE_180;
			break;
		case 270:
			mode = I2D_FLIP_MODE_270;
			break;
		default:
			dev_err(i2d->dev, "rotation %u not supported\n", ip->rotate_angle);
			return -EINVAL;
		}
		/* 90 and 270: the chroma plane follows the rotated luma */
		if (mode & I2D_FLIP_MODE_90)
			dst_uv = ip->dst_addr_y + dst_y_strid * srcw;
		/* as the vendor driver: the rotation replaces the data type */
		i2d_wr(i2d, I2D_IMG_MODE, mode);
	}

	if (ip->mirr_enable == 1) {
		dst_uv = ip->dst_addr_y + dst_y_strid * round_up_to(srch, h_align);
		i2d_set(i2d, I2D_IMG_MODE, I2D_MIRR);
	}
	if (ip->flip_enable == 1) {
		dst_uv = ip->dst_addr_y + dst_y_strid * round_up_to(srch, h_align);
		i2d_set(i2d, I2D_IMG_MODE, I2D_FLIP);
	}

	i2d_wr(i2d, I2D_SRC_ADDR_Y, ip->src_addr_y);
	i2d_wr(i2d, I2D_SRC_ADDR_UV,
	       ip->src_addr_y + src_y_strid * round_up_to(srch, h_align));
	i2d_wr(i2d, I2D_DST_ADDR_Y, ip->dst_addr_y);
	i2d_wr(i2d, I2D_DST_ADDR_UV, dst_uv);

	i2d_wr(i2d, I2D_SRC_Y_STRID, src_y_strid);
	i2d_wr(i2d, I2D_SRC_UV_STRID, src_uv_strid);
	i2d_wr(i2d, I2D_DST_Y_STRID, dst_y_strid);
	i2d_wr(i2d, I2D_DST_UV_STRID, dst_uv_strid);
	return 0;
}

static int i2d_start(struct jz_i2d *i2d, const struct i2d_param *ip)
{
	long left;
	int ret;

	i2d_set(i2d, I2D_CTRL, I2D_SAFE_RESET);
	ret = i2d_reg_set(i2d, ip);
	if (ret)
		return ret;
	i2d_set(i2d, I2D_IRQ_MASK, I2D_IRQ_TIMEOUT_MASK | I2D_IRQ_FRAME_DONE_MASK);
	i2d_wr(i2d, I2D_SHD_CTRL, 0xffffffff);

	/* a late interrupt of a timed-out job must not complete this one */
	reinit_completion(&i2d->done);
	i2d_set(i2d, I2D_CTRL, I2D_START);

	left = wait_for_completion_interruptible_timeout(&i2d->done,
							 msecs_to_jiffies(I2D_JOB_TIMEOUT_MS));
	if (left > 0)
		return 0;
	if (left < 0)
		return left;
	dev_err(i2d->dev, "job timed out\n");
	i2d_dump_regs(i2d);
	return -ETIMEDOUT;
}

static int i2d_get_pbuf(struct jz_i2d *i2d, void __user *uarg)
{
	if (!i2d->pbuf_virt) {
		i2d->pbuf_virt = kzalloc(I2D_BUF_SIZE, GFP_KERNEL);
		if (!i2d->pbuf_virt)
			return -ENOMEM;
		i2d->pbuf.vaddr_alloc = (u32)(unsigned long)i2d->pbuf_virt;
		i2d->pbuf.size = I2D_BUF_SIZE;
		i2d->pbuf.paddr = virt_to_phys(i2d->pbuf_virt);
		i2d->pbuf.paddr_align = i2d->pbuf.paddr;
	}
	if (copy_to_user(uarg, &i2d->pbuf, sizeof(i2d->pbuf)))
		return -EFAULT;
	return 0;
}

static void i2d_put_pbuf(struct jz_i2d *i2d)
{
	kfree(i2d->pbuf_virt);
	i2d->pbuf_virt = NULL;
	memset(&i2d->pbuf, 0, sizeof(i2d->pbuf));
}

static long i2d_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct jz_i2d *i2d = container_of(filp->private_data, struct jz_i2d, misc);
	void __user *uarg = (void __user *)arg;
	struct i2d_flush_cache_para fc;
	struct i2d_param ip = { };
	long ret = 0;

	if (_IOC_TYPE(cmd) != JZI2D_IOC_MAGIC)
		return -EFAULT;

	mutex_lock(&i2d->mutex);
	switch (cmd) {
	case IOCTL_I2D_START:
		if (copy_from_user(&ip, uarg, i2d->variant->param_size)) {
			ret = -EFAULT;
			break;
		}
		ret = i2d_start(i2d, &ip);
		break;
	case IOCTL_I2D_LISTEN:
		break;
	case IOCTL_I2D_GET_PBUFF:
		ret = i2d_get_pbuf(i2d, uarg);
		break;
	case IOCTL_I2D_RES_PBUFF:
		if (i2d->pbuf_virt)
			i2d_put_pbuf(i2d);
		else
			dev_warn(i2d->dev, "no buffer to free\n");
		break;
	case IOCTL_I2D_BUF_FLUSH_CACHE:
		/* the vendor driver ran dma_cache_sync() on the user address */
		if (copy_from_user(&fc, uarg, sizeof(fc)))
			ret = -EFAULT;
		else
			ret = rmem_cache_sync_user((unsigned long)fc.addr, fc.size,
						   DMA_BIDIRECTIONAL);
		break;
	default:
		dev_err(i2d->dev, "invalid command 0x%08x\n", cmd);
		ret = -EINVAL;
	}
	mutex_unlock(&i2d->mutex);
	return ret;
}

static const struct file_operations i2d_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= i2d_ioctl,
};

static irqreturn_t i2d_irq_handler(int irq, void *data)
{
	struct jz_i2d *i2d = data;
	u32 status = i2d_rd(i2d, I2D_IRQ_STATE);

	if (!status)
		return IRQ_NONE;
	i2d_set(i2d, I2D_IRQ_CLEAR, status);
	if (status & BIT(I2D_IRQ_FRAME_DONE))
		complete(&i2d->done);
	return IRQ_HANDLED;
}

static int i2d_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct jz_i2d *i2d;
	int ret;

	i2d = devm_kzalloc(dev, sizeof(*i2d), GFP_KERNEL);
	if (!i2d)
		return -ENOMEM;

	i2d->dev = dev;
	i2d->variant = of_device_get_match_data(dev);
	mutex_init(&i2d->mutex);
	init_completion(&i2d->done);

	i2d->iomem = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(i2d->iomem))
		return PTR_ERR(i2d->iomem);

	i2d->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(i2d->clk))
		return dev_err_probe(dev, PTR_ERR(i2d->clk), "no clock\n");

	i2d->irq = platform_get_irq(pdev, 0);
	if (i2d->irq < 0)
		return i2d->irq;
	ret = devm_request_irq(dev, i2d->irq, i2d_irq_handler, IRQF_SHARED,
			       "i2d", i2d);
	if (ret)
		return ret;

	i2d_set(i2d, I2D_CTRL, I2D_SAFE_RESET);

	i2d->misc.minor = MISC_DYNAMIC_MINOR;
	i2d->misc.name = "i2d";
	i2d->misc.fops = &i2d_fops;
	ret = misc_register(&i2d->misc);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, i2d);
	dev_info(dev, "I2D version 0x%08x\n", i2d_rd(i2d, I2D_RTL_VERSION));
	return 0;
}

static void i2d_remove(struct platform_device *pdev)
{
	struct jz_i2d *i2d = platform_get_drvdata(pdev);

	misc_deregister(&i2d->misc);
	i2d_put_pbuf(i2d);
}

static const struct i2d_variant i2d_t40 = {
	.param_size = offsetof(struct i2d_param, h_align),
	.user_strides = false,
};

static const struct i2d_variant i2d_t41 = {
	.param_size = sizeof(struct i2d_param),
	.user_strides = true,
};

static const struct of_device_id ingenic_i2d_dt_match[] = {
	{ .compatible = "ingenic,t40-i2d", .data = &i2d_t40 },
	{ .compatible = "ingenic,t41-i2d", .data = &i2d_t41 },
	{ }
};
MODULE_DEVICE_TABLE(of, ingenic_i2d_dt_match);

static struct platform_driver ingenic_i2d_driver = {
	.probe	= i2d_probe,
	.remove	= i2d_remove,
	.driver	= {
		.name = "ingenic-i2d",
		.of_match_table = ingenic_i2d_dt_match,
	},
};
module_platform_driver(ingenic_i2d_driver);

MODULE_DESCRIPTION("Ingenic I2D rotate/mirror/flip");
MODULE_AUTHOR("jiansheng.zhang <jiansheng.zhang@ingenic.cn>");
MODULE_LICENSE("GPL");
