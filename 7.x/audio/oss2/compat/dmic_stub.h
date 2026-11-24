/* SPDX-License-Identifier: GPL-2.0 */
/*
 * The vendor dsp code names the DMIC control commands even without a DMIC;
 * dp_dmic is NULL there, so these are never issued.
 */
#ifndef __DMIC_STUB_H__
#define __DMIC_STUB_H__
#ifndef CONFIG_JZ_TS_DMIC
enum {
	MIC_SET_DMIC_SAMPLERATE = 0x1000,
	MIC_SET_DMIC_CHANNEL,
	MIC_SET_PERIODS_MS,
	MIC_INIT,
	MIC_DEINIT,
	DMIC_ENABLE_DMA,
};
#endif
#endif
