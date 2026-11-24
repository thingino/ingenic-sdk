/* SPDX-License-Identifier: GPL-2.0 */
/* What the vendor DMA header gave the audio driver: the request names */
#ifndef __MACH_JZDMA_H__
#define __MACH_JZDMA_H__

#include <linux/dmaengine.h>

enum jzdma_type {
	JZDMA_REQ_I2S0,
	JZDMA_REQ_AEC,
	JZDMA_REQ_DMIC,
};

#endif
