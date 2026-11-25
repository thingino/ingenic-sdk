/* SPDX-License-Identifier: GPL-2.0 */
/* The T32 AudioSystem DMIC, as the dsp core uses it; the T33 has none */
#ifndef _JZ_AUDIO_DMIC_H_
#define _JZ_AUDIO_DMIC_H_

#include <linux/device.h>
#include <linux/dma-mapping.h>

#ifdef AUDIO_HAS_DMIC
struct device *audio_dmic_dev(void);
int audio_dmic_register(void);
void audio_dmic_unregister(void);
dma_addr_t ingenic_dmic_get_dma_current_trans_addr(void);
int ingenic_dmic_dma_init(dma_addr_t dma_addr, unsigned int fragment_size,
			  unsigned int fragment_cnt);
int ingenic_dmic_dma_deinit(void);
#else
static inline struct device *audio_dmic_dev(void) { return NULL; }
static inline int audio_dmic_register(void) { return 0; }
static inline void audio_dmic_unregister(void) { }
static inline dma_addr_t ingenic_dmic_get_dma_current_trans_addr(void) { return 0; }
static inline int ingenic_dmic_dma_init(dma_addr_t dma_addr, unsigned int fragment_size,
					unsigned int fragment_cnt) { return -ENODEV; }
static inline int ingenic_dmic_dma_deinit(void) { return 0; }
#endif

#endif /* _JZ_AUDIO_DMIC_H_ */
