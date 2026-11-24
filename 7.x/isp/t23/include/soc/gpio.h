/* SPDX-License-Identifier: GPL-2.0 */
/* Stubs for the BSP soc/gpio.h API.
 * Mainline configures pin functions via pinctrl from DT, so the
 * jzgpio_*() helpers become no-ops. Enums kept so call sites compile. */
#ifndef __JZSOC_JZ_GPIO_H_STUB__
#define __JZSOC_JZ_GPIO_H_STUB__

#include <linux/types.h>

enum gpio_function {
	GPIO_FUNC_0 = 0x00,
	GPIO_FUNC_1 = 0x01,
	GPIO_FUNC_2 = 0x02,
	GPIO_FUNC_3 = 0x03,
	GPIO_OUTPUT0 = 0x04,
	GPIO_OUTPUT1 = 0x05,
	GPIO_INPUT = 0x06,
	GPIO_INT_LO = 0x08,
	GPIO_INT_HI = 0x09,
	GPIO_INT_FE = 0x0a,
	GPIO_INT_RE = 0x0b,
	GPIO_PULL_HIZ = 0x80,
	GPIO_PULL_UP = 0x90,
	GPIO_PULL_DOWN = 0xa0,
	GPIO_PULL_BUSHOLD = 0xb0,
	GPIO_PULL_DOWN_DIS = 0xc0,
	GPIO_INPUT_PULL_HI = 0x96,
};

enum gpio_port {
	GPIO_PORT_A = 0,
	GPIO_PORT_B,
	GPIO_PORT_C,
	GPIO_PORT_D,
	GPIO_NR_PORTS,
};

#define GPIO_PA(n)	(0 * 32 + (n))
#define GPIO_PB(n)	(1 * 32 + (n))
#define GPIO_PC(n)	(2 * 32 + (n))
#define GPIO_PD(n)	(3 * 32 + (n))

struct jz_gpio_func_def {
	char *name;
	int port;
	int func;
	unsigned long pins;
};

static inline int jzgpio_set_func(enum gpio_port port,
				  enum gpio_function func,
				  unsigned long pins)
{
	(void)port; (void)func; (void)pins;
	return 0;
}

static inline int jz_gpio_set_func(int gpio, enum gpio_function func)
{
	(void)gpio; (void)func;
	return 0;
}

static inline int jzgpio_ctrl_pull(int port, int pin, int up)
{
	(void)port; (void)pin; (void)up;
	return 0;
}

#endif
