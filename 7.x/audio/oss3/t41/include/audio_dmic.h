/* SPDX-License-Identifier: GPL-2.0 */
/* The T41 AudioSystem DMIC, as the dsp core uses it */
#ifndef _JZ_AUDIO_DMIC_H_
#define _JZ_AUDIO_DMIC_H_

#include <linux/device.h>
#include <linux/dma-mapping.h>

struct device *audio_dmic_dev(void);
int audio_dmic_register(void);
void audio_dmic_unregister(void);
dma_addr_t ingenic_dmic_get_dma_current_trans_addr(void);
int ingenic_dmic_dma_init(dma_addr_t dma_addr, unsigned int fragment_size,
			  unsigned int fragment_cnt);
int ingenic_dmic_dma_deinit(void);

#endif /* _JZ_AUDIO_DMIC_H_ */
