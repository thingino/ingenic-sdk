/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Registers and ioctl interface of the Ingenic I2D, from the vendor T40/T41
 * 4.4.94 driver.
 */
#ifndef __INGENIC_I2D_H__
#define __INGENIC_I2D_H__

#include <linux/bits.h>
#include <linux/ioctl.h>
#include <linux/types.h>

#define I2D_RTL_VERSION		0x00
#define I2D_SHD_CTRL		0x08
#define I2D_CTRL		0x10
#define I2D_IMG_SIZE		0x14
#define I2D_IMG_MODE		0x18
#define I2D_SRC_ADDR_Y		0x20
#define I2D_SRC_ADDR_UV		0x24
#define I2D_SRC_Y_STRID		0x28
#define I2D_SRC_UV_STRID	0x2c
#define I2D_DST_ADDR_Y		0x30
#define I2D_DST_ADDR_UV		0x34
#define I2D_DST_Y_STRID		0x38
#define I2D_DST_UV_STRID	0x3c
#define I2D_IRQ_STATE		0x80
#define I2D_IRQ_CLEAR		0x84
#define I2D_IRQ_MASK		0x88
#define I2D_TIMEOUT_VALUE	0x90
#define I2D_TIMEOUT_MODE	0x94
#define I2D_CLK_GATE		0x98
#define I2D_DBG_0		0xa0

/* CTRL */
#define I2D_RESET		BIT(16)
#define I2D_SAFE_RESET		BIT(4)
#define I2D_START		BIT(0)

/* IMG_SIZE */
#define I2D_WIDTH		16
#define I2D_HEIGHT		0

/* IMG_MODE */
#define I2D_DATA_TYPE_NV12	(0 << 4)
#define I2D_DATA_TYPE_RAW8	BIT(4)
#define I2D_DATA_TYPE_RGB565	(2 << 4)
#define I2D_DATA_TYPE_ARGB8888	(3 << 4)
#define I2D_FLIP_MODE_0		(0 << 2)
#define I2D_FLIP_MODE_90	BIT(2)
#define I2D_FLIP_MODE_180	(2 << 2)
#define I2D_FLIP_MODE_270	(3 << 2)
#define I2D_MIRR		BIT(1)
#define I2D_FLIP		BIT(0)

/* IRQ_STATE bits, IRQ_MASK values */
#define I2D_IRQ_TIMEOUT		1
#define I2D_IRQ_FRAME_DONE	0
#define I2D_IRQ_TIMEOUT_MASK	(0 << 1)
#define I2D_IRQ_FRAME_DONE_MASK	(0 << 0)

/* libimp's data_type: IMPPixelFormat for NV12, HAL_PIXEL_FORMAT_* otherwise */
#define PIX_FMT_NV12			10
#define HAL_PIXEL_FORMAT_RGB_565	4
#define HAL_PIXEL_FORMAT_ARGB_8888	9
#define HAL_PIXEL_FORMAT_RAW8		0x1b

struct i2d_buf_info {
	u32 vaddr_alloc;
	u32 paddr;
	u32 paddr_align;
	u32 size;
};

struct i2d_flush_cache_para {
	void *addr;
	u32 size;
};

/* the T40's libimp passes everything up to (not including) h_align */
struct i2d_param {
	u32 src_w;
	u32 src_h;
	u32 data_type;
	u32 rotate_enable;
	u32 rotate_angle;
	u32 flip_enable;
	u32 mirr_enable;
	u32 src_addr_y;
	u32 src_addr_uv;
	u32 dst_addr_y;
	u32 dst_addr_uv;
	u32 src_y_strid;
	u32 src_uv_strid;
	u32 dst_y_strid;
	u32 dst_uv_strid;
	u32 h_align;
};

#define JZI2D_IOC_MAGIC			'D'
#define IOCTL_I2D_START			_IO(JZI2D_IOC_MAGIC, 106)
#define IOCTL_I2D_RES_PBUFF		_IO(JZI2D_IOC_MAGIC, 114)
#define IOCTL_I2D_GET_PBUFF		_IO(JZI2D_IOC_MAGIC, 115)
#define IOCTL_I2D_LISTEN		_IO(JZI2D_IOC_MAGIC, 116)
#define IOCTL_I2D_BUF_UNLOCK		_IO(JZI2D_IOC_MAGIC, 117)
#define IOCTL_I2D_BUF_FLUSH_CACHE	_IO(JZI2D_IOC_MAGIC, 118)

#endif
