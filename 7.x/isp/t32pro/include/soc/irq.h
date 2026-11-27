/* SPDX-License-Identifier: GPL-2.0 */
/* Stubs for the BSP soc/irq.h - IRQ numbers.
 * On mainline these come from DT/interrupt-controller. */
#ifndef __JZSOC_JZ_IRQ_H_STUB__
#define __JZSOC_JZ_IRQ_H_STUB__

/* Provide platform IRQ defines if anything legacy hardcodes them.
 * Real IRQ wiring should come via platform_get_irq() from DT. */
#define IRQ_ISP			0
#define IRQ_VIC			0
#define IRQ_CSI			0

#endif
