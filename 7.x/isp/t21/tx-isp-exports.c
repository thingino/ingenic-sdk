// SPDX-License-Identifier: GPL-2.0
/*
 * The firmware exports 25 symbols for the sensor drivers in 3.10's 8-byte
 * __ksymtab format, which 7.x cannot read; the Kbuild strips those sections
 * and the symbols are exported again here in the native format.
 */
#include <linux/module.h>

extern void private_driver_get_interface(void);
extern void private_capable(void);
extern void private_msleep(void);
extern void private_jzgpio_set_func(void);
extern void private_gpio_direction_output(void);
extern void private_gpio_free(void);
extern void private_gpio_request(void);
extern void private_i2c_add_driver(void);
extern void private_i2c_set_clientdata(void);
extern void private_i2c_get_clientdata(void);
extern void private_i2c_del_driver(void);
extern void private_i2c_transfer(void);
extern void private_clk_set_rate(void);
extern void private_clk_put(void);
extern void private_clk_disable(void);
extern void private_clk_enable(void);
extern void private_log2_fixed_to_fixed(void);
extern void private_log2_int_to_fixed(void);
extern void private_math_exp2(void);
extern void tx_isp_exit(void);
extern void tx_isp_init(void);
extern void tx_isp_subdev_deinit(void);
extern void tx_isp_subdev_init(void);
extern void tisp_log2_fixed_to_fixed(void);
extern void tisp_math_exp2(void);

EXPORT_SYMBOL(private_driver_get_interface);
EXPORT_SYMBOL(private_capable);
EXPORT_SYMBOL(private_msleep);
EXPORT_SYMBOL(private_jzgpio_set_func);
EXPORT_SYMBOL(private_gpio_direction_output);
EXPORT_SYMBOL(private_gpio_free);
EXPORT_SYMBOL(private_gpio_request);
EXPORT_SYMBOL(private_i2c_add_driver);
EXPORT_SYMBOL(private_i2c_set_clientdata);
EXPORT_SYMBOL(private_i2c_get_clientdata);
EXPORT_SYMBOL(private_i2c_del_driver);
EXPORT_SYMBOL(private_i2c_transfer);
EXPORT_SYMBOL(private_clk_set_rate);
EXPORT_SYMBOL(private_clk_put);
EXPORT_SYMBOL(private_clk_disable);
EXPORT_SYMBOL(private_clk_enable);
EXPORT_SYMBOL(private_log2_fixed_to_fixed);
EXPORT_SYMBOL(private_log2_int_to_fixed);
EXPORT_SYMBOL(private_math_exp2);
EXPORT_SYMBOL(tx_isp_exit);
EXPORT_SYMBOL(tx_isp_init);
EXPORT_SYMBOL(tx_isp_subdev_deinit);
EXPORT_SYMBOL(tx_isp_subdev_init);
EXPORT_SYMBOL(tisp_log2_fixed_to_fixed);
EXPORT_SYMBOL(tisp_math_exp2);
