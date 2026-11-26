/* SPDX-License-Identifier: GPL-2.0 */
/* The T40 DMIC host driver, registered by the dsp core with dmic_enable=1 */
#ifndef _JZ_AUDIO_DMIC_H_
#define _JZ_AUDIO_DMIC_H_

int audio_dmic_register(void);
void audio_dmic_unregister(void);

#endif /* _JZ_AUDIO_DMIC_H_ */
