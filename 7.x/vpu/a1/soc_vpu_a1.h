/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SOC_VPU_A1_H
#define SOC_VPU_A1_H

/* exported to the decoder module, which shares the hardware lock */
void vpu_work_lock(void);
void vpu_work_unlock(void);
int vpu_work_is_locked(void);

int soc_channel_init(void);
void soc_channel_exit(void);

#endif
