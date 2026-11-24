#ifndef __TX_SENSOR_COMMON_H__
#define __TX_SENSOR_COMMON_H__
#include <soc/gpio.h>

#include <txx-funcs.h>

#define SENSOR_R_BLACK_LEVEL	0
#define SENSOR_GR_BLACK_LEVEL	1
#define SENSOR_GB_BLACK_LEVEL	2
#define SENSOR_B_BLACK_LEVEL	3

/* External v4l2 format info. */
#define V4L2_I2C_REG_MAX		(150)
#define V4L2_I2C_ADDR_16BIT		(0x0002)
#define V4L2_I2C_DATA_16BIT		(0x0004)
#define V4L2_SBUS_MASK_SAMPLE_8BITS	0x01
#define V4L2_SBUS_MASK_SAMPLE_16BITS	0x02
#define V4L2_SBUS_MASK_SAMPLE_32BITS	0x04
#define V4L2_SBUS_MASK_ADDR_8BITS	0x08
#define V4L2_SBUS_MASK_ADDR_16BITS	0x10
#define V4L2_SBUS_MASK_ADDR_32BITS	0x20
#define V4L2_SBUS_MASK_ADDR_STEP_16BITS 0x40
#define V4L2_SBUS_MASK_ADDR_STEP_32BITS 0x80
#define V4L2_SBUS_MASK_SAMPLE_SWAP_BYTES 0x100
#define V4L2_SBUS_MASK_SAMPLE_SWAP_WORDS 0x200
#define V4L2_SBUS_MASK_ADDR_SWAP_BYTES	0x400
#define V4L2_SBUS_MASK_ADDR_SWAP_WORDS	0x800
#define V4L2_SBUS_MASK_ADDR_SKIP	0x1000
#define V4L2_SBUS_MASK_SPI_READ_MSB_SET 0x2000
#define V4L2_SBUS_MASK_SPI_INVERSE_DATA 0x4000
#define V4L2_SBUS_MASK_SPI_HALF_ADDR	0x8000
#define V4L2_SBUS_MASK_SPI_LSB		0x10000

static inline int set_sensor_gpio_function(int func_set)
{
	int ret = 0;
	/* VDD select 1.8V */
//	*(volatile unsigned int*)(0xB0010104) = 0x1;
	*(volatile unsigned int*)(0xB0010130) = 0x2aaa000;
	switch (func_set) {
	case DVP_PA_LOW_8BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x000340ff);
		pr_info("set sensor gpio as PA-low-8bit\n");
		break;
	case DVP_PA_HIGH_8BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x00034ff0);
		pr_info("set sensor gpio as PA-high-8bit\n");
		break;
	case DVP_PA_LOW_10BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x000343ff);
		pr_info("set sensor gpio as PA-low-10bit\n");
		break;
	case DVP_PA_HIGH_10BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x00034ffc);
		pr_info("set sensor gpio as PA-high-10bit\n");
		break;
	case DVP_PA_12BIT:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_1, 0x00034fff);
		pr_info("set sensor gpio as PA-12bit\n");
		break;
	default:
		pr_err("set sensor gpio error: unknow function %d\n", func_set);
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

static inline int __sinfo_subdev_init(struct tx_isp_pdev44 *pdev,
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
