/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __COMPAT_SOC_GPIO_H__
#define __COMPAT_SOC_GPIO_H__

#include <linux/gpio.h>

/* the ingenic pinctrl registers its banks at 32 * bank */
#define GPIO_PA(n)	(0 * 32 + (n))
#define GPIO_PB(n)	(1 * 32 + (n))
#define GPIO_PC(n)	(2 * 32 + (n))
#define GPIO_PD(n)	(3 * 32 + (n))

enum gpio_function {
	GPIO_FUNC_0 = 0x00,
	GPIO_FUNC_1 = 0x01,
	GPIO_FUNC_2 = 0x02,
	GPIO_FUNC_3 = 0x03,
};

enum gpio_port {
	GPIO_PORT_A = 0, GPIO_PORT_B,
	GPIO_PORT_C, GPIO_PORT_D,
	GPIO_NR_PORTS,
};

#endif
