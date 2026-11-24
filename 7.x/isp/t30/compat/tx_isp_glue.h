/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __TX_ISP_GLUE_H__
#define __TX_ISP_GLUE_H__

#include <soc/gpio.h>

struct clk;
struct device;

struct clk *tx_isp_clk_get(struct device *dev, const char *name);
int tx_isp_clk_set_rate(struct clk *clk, unsigned long rate);
int tx_isp_dvp_pins(enum gpio_port port, enum gpio_function func,
		    unsigned long pins);

/* the vendor module init/exit, called once the DT node has bound */
int tx_isp_start(void);
void tx_isp_stop(void);

#endif
