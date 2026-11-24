/* SPDX-License-Identifier: GPL-2.0 */
/* IPU v13 registers (T10 to T31), as used by the OSD / CSC paths */
#ifndef _IPU_REGS_H_
#define _IPU_REGS_H_

#include <linux/bits.h>

#define IPU_STATUS		0x004
#define IPU_D_FMT		0x008
#define IPU_Y_ADDR		0x00c
#define IPU_U_ADDR		0x010
#define IPU_IN_FM_GS		0x018
#define IPU_Y_STRIDE		0x01c
#define IPU_UV_STRIDE		0x020
#define IPU_OUT_ADDR		0x024
#define IPU_OUT_STRIDE		0x02c
#define IPU_OUT_V_ADDR		0x030
#define IPU_OUT_V_STRIDE	0x034
#define IPU_FM_CTRL		0x000
#define IPU_REG_CTRL		0x064
#define IPU_TRIGGER		0x074
#define IPU_GLB_CTRL		0x07c
#define IPU_NV_OUT_ADDR		0x0b4
#define IPU_NV_OUT_STRIDE	0x0b8
#define IPU_OSD_IN_Y_ADDR(n)	(0x0bc + (n) * 0x10)
#define IPU_OSD_IN_Y_STRIDE(n)	(0x0c0 + (n) * 0x10)
#define IPU_OSD_IN_UV_ADDR(n)	(0x0c4 + (n) * 0x10)
#define IPU_OSD_IN_UV_STRIDE(n)	(0x0c8 + (n) * 0x10)
#define IPU_OSD_GS(n)		(0x0fc + (n) * 4)
#define IPU_OSD_POS(n)		(0x10c + (n) * 4)
#define IPU_OSD_PARA(n)		(0x11c + (n) * 4)
#define IPU_OSD_BK_PARA		0x12c
#define IPU_OSD_BAK_ARGB(n)	(0x130 + (n) * 4)
#define IPU_OSD_B_BAK_ARGB	0x140
#define IPU_CSC_C(n, i)		(0x148 + (n) * 0x18 + (i) * 4)
#define IPU_CSC_OFSET(n)	(0x15c + (n) * 0x18)

#define IPU_IRQ_EN		BIT(0)		/* GLB_CTRL */
#define IPU_RUN			BIT(0)		/* TRIGGER */
#define IPU_STOP		BIT(1)
#define IPU_RESET		BIT(3)
#define IPU_OUT_END		BIT(0)		/* STATUS */

#define IPU_FM_CTRL_STRIDE2	BIT(10)
#define IPU_D_FMT_BLK_SEL	BIT(4)

#define BK_EN			BIT(0)
#define BK_PIC_NV12		(2 << 11)
#define BK_PIC_NV21		(3 << 11)
#define BK_PIC_ARGB		(0 << 11)
#define BK_ARGB_TYPE(t)		((t) << 14)
#define BK_PREM			BIT(18)

#define OUT_FMT_NV12		5
#define OUT_FMT_NV21		6
#define OUT_FMT_HSV		7
#define OUT_FMT_ARGB_8888	8
#define OUT_FMT_ABGR_8888	9
#define OUT_FMT_RGBA_8888	0xa
#define OUT_FMT_BGRA_8888	0xb

#define IN_FMT_YUV420		1
#define IN_FMT_YUV420_B		2
#define IN_FMT_YUV422		3
#define IN_FMT_YUV444		4
#define IN_FMT_RGB_888		6
#define IN_FMT_YUV411		8

#endif
