/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __COMPAT_SOC_GPIO_H__
#define __COMPAT_SOC_GPIO_H__

#include <linux/gpio.h>

/* the ingenic pinctrl registers its banks at 32 * bank */
#define GPIO_PA(n)	(0 * 32 + (n))
#define GPIO_PB(n)	(1 * 32 + (n))
#define GPIO_PC(n)	(2 * 32 + (n))
#define GPIO_PD(n)	(3 * 32 + (n))
#define GPIO_PE(n)	(4 * 32 + (n))
#define GPIO_PF(n)	(5 * 32 + (n))

#endif
