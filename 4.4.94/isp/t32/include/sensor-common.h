#ifndef __TX_SENSOR_COMMON_H__
#define __TX_SENSOR_COMMON_H__
#include <soc/gpio.h>
#include <txx-funcs.h>

#ifdef CONFIG_SOC_PRJ008
#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
#define SEN_TCLK "ext"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_TCLK "ext1"
#endif
#elif defined(CONFIG_SOC_PRJ007)
#define SEN_TCLK "vpll"
#endif
#define SEN_TCLK1 "apll"

#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
#define SEN_MCLK "mux_cim"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_MCLK "div_cim"
#endif

#define SEN_BCLK "div_cim"

static inline int set_sensor_gpio_function(int func_set)
{
	int ret = 0;
	/* VDD select 1.8V */
	//      *(volatile unsigned int*)(0xB0010104) = 0x1;
	*(volatile unsigned int*)(0xB0010130) = 0x2aaa000;
	switch (func_set) {
	case DVP_PA_LOW_8BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0002c000);
		pr_info("set sensor gpio as PA-low-8bit\n");
		break;
	case DVP_PA_HIGH_8BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0002c000);
		pr_info("set sensor gpio as PA-high-8bit\n");
		break;
	case DVP_PA_LOW_10BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0002c000);
		pr_info("set sensor gpio as PA-low-10bit\n");
		break;
	case DVP_PA_HIGH_10BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0002c000);
		pr_info("set sensor gpio as PA-high-10bit\n");
		break;
	case DVP_PA_12BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x0002c000);
		pr_info("set sensor gpio as PA-12bit\n");
		break;
	default:
		pr_err("set sensor gpio error: unknow function %d\n", func_set);
		ret = -1;
		break;
	}


	return ret;
}

static inline int set_sensor_mclk_function(int mclk_set)
{
	int ret = 0;
	/* VDD select 1.8V */
	//      *(volatile unsigned int*)(0xB0010104) = 0x1;
	/* *(volatile unsigned int*)(0xB0010130) = 0x2aaa000; */
	switch (mclk_set) {
	case 0:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_0, 0x00010000);
		pr_info("set sensor mclk(%d) gpio\n", mclk_set);
		break;
	case 1:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_0, 0x00020000);
		pr_info("set sensor mclk(%d) gpio\n", mclk_set);
		break;
	default:
		pr_err("set sensor mclk error: unknow function %d\n", mclk_set);
		ret = -1;
		break;
	}

	return ret;
}

/*
 * tx-isp sensor info registry hooks (isp/common/tx-isp-sinfo.c).
 *
 * Every sensor driver includes this header, so wrapping the two calls
 * every driver already makes — private_i2c_add_driver() at module load
 * and tx_isp_subdev_init() at probe — registers it with /proc/jz/sensor
 * with no per-driver code. The parenthesized calls below bypass the
 * function-like macros and reach the real symbols.
 *
 * SENSOR_I2C_ADDRESS is expanded at the driver's call site, where the
 * driver's own #define is in scope. A driver without that macro fails
 * to compile, which is the enforcement.
 */
#ifndef TX_ISP_SINFO_NO_HOOK
#include <tx-isp-common.h>

int tx_isp_sinfo_driver_add(struct i2c_driver *drv, int def_i2c_addr,
			    struct module *owner);
void tx_isp_sinfo_driver_del(struct i2c_driver *drv);
int tx_isp_sinfo_sensor_bind(struct tx_isp_subdev *sd, struct module *owner);
void tx_isp_sinfo_sensor_unbind(struct tx_isp_subdev *sd, struct module *owner);

static inline int __sinfo_i2c_add_driver(struct i2c_driver *drv,
					 int def_i2c_addr, struct module *owner)
{
	int ret = (private_i2c_add_driver)(drv);
	if (!ret)
		tx_isp_sinfo_driver_add(drv, def_i2c_addr, owner);
	return ret;
}

static inline void __sinfo_i2c_del_driver(struct i2c_driver *drv)
{
	tx_isp_sinfo_driver_del(drv);
	(private_i2c_del_driver)(drv);
}

static inline int __sinfo_subdev_init(struct platform_device *pdev,
				      struct tx_isp_subdev *sd,
				      struct tx_isp_subdev_ops *ops,
				      struct module *owner)
{
	int ret = (tx_isp_subdev_init)(pdev, sd, ops);
	if (!ret)
		tx_isp_sinfo_sensor_bind(sd, owner);
	return ret;
}

static inline void __sinfo_subdev_deinit(struct tx_isp_subdev *sd,
					 struct module *owner)
{
	tx_isp_sinfo_sensor_unbind(sd, owner);
	(tx_isp_subdev_deinit)(sd);
}

#define private_i2c_add_driver(drv) \
	__sinfo_i2c_add_driver((drv), SENSOR_I2C_ADDRESS, THIS_MODULE)
#define private_i2c_del_driver(drv) \
	__sinfo_i2c_del_driver((drv))
#define tx_isp_subdev_init(pdev, sd, ops) \
	__sinfo_subdev_init((pdev), (sd), (ops), THIS_MODULE)
#define tx_isp_subdev_deinit(sd) \
	__sinfo_subdev_deinit((sd), THIS_MODULE)


/*
 * A few vendor drivers call the plain i2c_add_driver() instead of the
 * private_* wrapper. Catch those too: i2c_add_driver is a kernel macro,
 * so #undef it and call i2c_register_driver() with the driver's module.
 */
static inline int __sinfo_i2c_add_driver_plain(struct i2c_driver *drv,
					       int def_i2c_addr,
					       struct module *owner)
{
	int ret = i2c_register_driver(owner, drv);
	if (!ret)
		tx_isp_sinfo_driver_add(drv, def_i2c_addr, owner);
	return ret;
}

static inline void __sinfo_i2c_del_driver_plain(struct i2c_driver *drv)
{
	tx_isp_sinfo_driver_del(drv);
	(i2c_del_driver)(drv);
}

#undef i2c_add_driver
#define i2c_add_driver(drv) \
	__sinfo_i2c_add_driver_plain((drv), SENSOR_I2C_ADDRESS, THIS_MODULE)
#define i2c_del_driver(drv) \
	__sinfo_i2c_del_driver_plain((drv))

#endif /* TX_ISP_SINFO_NO_HOOK */

#endif// __TX_SENSOR_COMMON_H__
