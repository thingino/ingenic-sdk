#ifndef __TX_SENSOR_COMMON_H__
#define __TX_SENSOR_COMMON_H__
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <linux/i2c.h>
#include <soc/gpio.h>

#include "tx-isp-common.h"

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

struct tx_isp_sensor_win_setting {
	int	width;
	int	height;
	int fps;
	u32 mbus_code;
	enum v4l2_colorspace colorspace;
	void *regs;	/* Regs to tweak; the default fps is fast */
};

/*
 * The DVP pad mux comes from the "ingenic,t20-isp" node's pinctrl state
 * (PA0-PA9 data, PCLK, MCLK, sync), applied when tx-isp binds; the vendor
 * kernel muxed the pads here with jzgpio_set_func().
 */
static inline int set_sensor_gpio_function(int func_set)
{
	return 0;
}

/*
 * Sensor info registry hooks (tx-isp-sinfo.c, /proc/jz/sensor). Only sensor
 * translation units include this header, so the wraps are sensor-only.
 * SENSOR_I2C_ADDRESS expands at the driver's call site.
 */
#include <tx-isp-common.h>
#include <media/v4l2-device.h>

int tx_isp_sinfo_driver_add(struct i2c_driver *drv, int def_i2c_addr,
			    struct module *owner);
void tx_isp_sinfo_driver_del(struct i2c_driver *drv);
int tx_isp_sinfo_sensor_bind(struct v4l2_subdev *sd, struct module *owner);
void tx_isp_sinfo_sensor_unbind(struct v4l2_subdev *sd, struct module *owner);
int tx_isp_sinfo_init(void);
void tx_isp_sinfo_exit(void);

#ifndef TX_ISP_SINFO_NO_HOOK

static inline void __sinfo_v4l2_subdev_init(struct v4l2_subdev *sd,
					    struct i2c_client *client,
					    const struct v4l2_subdev_ops *ops,
					    struct module *owner)
{
	(v4l2_i2c_subdev_init)(sd, client, ops);
	tx_isp_sinfo_sensor_bind(sd, owner);
}

static inline void __sinfo_v4l2_unreg_subdev(struct v4l2_subdev *sd,
					     struct module *owner)
{
	tx_isp_sinfo_sensor_unbind(sd, owner);
	(v4l2_device_unregister_subdev)(sd);
}

static inline int __sinfo_i2c_add_driver(struct i2c_driver *drv,
					 int def_i2c_addr, struct module *owner)
{
	int ret = i2c_register_driver(owner, drv);

	if (!ret)
		tx_isp_sinfo_driver_add(drv, def_i2c_addr, owner);
	return ret;
}

static inline void __sinfo_i2c_del_driver(struct i2c_driver *drv)
{
	tx_isp_sinfo_driver_del(drv);
	(i2c_del_driver)(drv);
}

#undef i2c_add_driver
#define i2c_add_driver(drv) \
	__sinfo_i2c_add_driver((drv), SENSOR_I2C_ADDRESS, THIS_MODULE)
#define i2c_del_driver(drv) \
	__sinfo_i2c_del_driver((drv))
#define v4l2_i2c_subdev_init(sd, client, ops) \
	__sinfo_v4l2_subdev_init((sd), (client), (ops), THIS_MODULE)
#define v4l2_device_unregister_subdev(sd) \
	__sinfo_v4l2_unreg_subdev((sd), THIS_MODULE)

#endif /* TX_ISP_SINFO_NO_HOOK */

#endif// __TX_SENSOR_COMMON_H__
