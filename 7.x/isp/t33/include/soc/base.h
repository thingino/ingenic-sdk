/* SPDX-License-Identifier: GPL-2.0 */
/* Stubs for the BSP soc/base.h - hardware base addresses.
 * On mainline these come from device tree; the shim keeps the constants
 * defined so legacy code that hardcodes addresses still compiles, but
 * we should prefer DT for any new code. */
#ifndef __JZSOC_JZ_BASE_H_STUB__
#define __JZSOC_JZ_BASE_H_STUB__

/* T31 register block bases */
#define CPM_IOBASE		0x10000000
#define INTC_IOBASE		0x10001000
#define TCU_IOBASE		0x10002000
#define WDT_IOBASE		0x10002000
#define UART0_IOBASE		0x10030000
#define UART1_IOBASE		0x10031000
#define UART2_IOBASE		0x10032000
#define DMIC_IOBASE		0x10020000
#define I2C0_IOBASE		0x10050000
#define I2C1_IOBASE		0x10051000
#define I2C2_IOBASE		0x10052000
#define MSC0_IOBASE		0x13450000
#define MSC1_IOBASE		0x13460000
#define EMC_IOBASE		0x13010000
#define DDRC_IOBASE		0x13011000
#define GPIO_IOBASE		0x10010000
#define ISP_IOBASE		0x13300000
#define VIC_IOBASE		0x10023000
#define CSI_IOBASE		0x10022000
#define AVPU_IOBASE		0x13200000
#define IPU_IOBASE		0x13080000
#define SFC_IOBASE		0x13440000

#endif
