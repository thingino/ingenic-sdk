/* SPDX-License-Identifier: GPL-2.0 */
/*
 * User ABI of /dev/ipu, byte-identical to the vendor jz_ipu_v13 driver
 * (T31 SDK 1.1.6, 3.10.14): libimp builds these structures itself.
 */
#ifndef _IPU_IOCTL_H_
#define _IPU_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/* HAL pixel formats the driver accepts (system/graphics.h numbering) */
#define HAL_PIXEL_FORMAT_RGBA_8888	1
#define HAL_PIXEL_FORMAT_RGBX_8888	2
#define HAL_PIXEL_FORMAT_RGB_888	3
#define HAL_PIXEL_FORMAT_RGB_565	4
#define HAL_PIXEL_FORMAT_BGRA_8888	5
#define HAL_PIXEL_FORMAT_RGBA_5551	6
#define HAL_PIXEL_FORMAT_ABGR_8888	8
#define HAL_PIXEL_FORMAT_ARGB_8888	9
#define HAL_PIXEL_FORMAT_YCbCr_422_SP	0x10
#define HAL_PIXEL_FORMAT_YCbCr_420_SP	0x11
#define HAL_PIXEL_FORMAT_YCbCr_422_P	0x12
#define HAL_PIXEL_FORMAT_YCbCr_420_P	0x13
#define HAL_PIXEL_FORMAT_YCbCr_422_I	0x14
#define HAL_PIXEL_FORMAT_YCbCr_420_I	0x15
#define HAL_PIXEL_FORMAT_NV12		0x18
#define HAL_PIXEL_FORMAT_NV21		0x19
#define HAL_PIXEL_FORMAT_BGRA_5551	0x1a
#define HAL_PIXEL_FORMAT_HSV		0x1b
#define HAL_PIXEL_FORMAT_YCbCr_420_B	0x24
#define HAL_PIXEL_FORMAT_BGRX_8888	0x1ff
#define HAL_PIXEL_FORMAT_JZ_YUV_420_P	0x47700001
#define HAL_PIXEL_FORMAT_JZ_YUV_420_B	0x47700002

#define IPU_CMD_OSD0	(1 << 0)
#define IPU_CMD_OSD1	(1 << 1)
#define IPU_CMD_OSD2	(1 << 2)
#define IPU_CMD_OSD3	(1 << 3)
#define IPU_CMD_OSD	(0xf << 0)
#define IPU_CMD_CSC	(1 << 4)

struct ipu_osd_ch {
	__u32 fmt;
	__u32 para;
	__u32 bak_argb;
	__u32 pos_x;
	__u32 pos_y;
	__u32 src_w;
	__u32 src_h;
	__u32 buf_p;
};

struct ipu_param {
	__u32 cmd;
	__u32 bg_w;
	__u32 bg_h;
	__u32 bg_fmt;
	__u32 bg_buf_p;
	__u32 out_fmt;
	struct ipu_osd_ch osd[4];
};

/* struct ipu_buf_info of the vendor driver: vaddr_alloc is a kernel pointer */
struct ipu_buf_info {
	__u32 vaddr_alloc;
	__u32 paddr;
	__u32 paddr_align;
	__u32 size;
};

/* addr is a user pointer (4 bytes on o32) */
struct ipu_flush_cache_para {
	__u32 addr;
	__u32 size;
};

#define JZIPU_IOC_MAGIC		'I'
#define IOCTL_IPU_START		_IO(JZIPU_IOC_MAGIC, 106)
#define IOCTL_IPU_RES_PBUFF	_IO(JZIPU_IOC_MAGIC, 114)
#define IOCTL_IPU_GET_PBUFF	_IO(JZIPU_IOC_MAGIC, 115)
#define IOCTL_IPU_BUF_LOCK	_IO(JZIPU_IOC_MAGIC, 116)
#define IOCTL_IPU_BUF_UNLOCK	_IO(JZIPU_IOC_MAGIC, 117)
#define IOCTL_IPU_BUF_FLUSH_CACHE _IO(JZIPU_IOC_MAGIC, 118)

#endif
