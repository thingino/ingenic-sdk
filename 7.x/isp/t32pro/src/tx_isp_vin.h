/*
 * Header: tx_isp_vin.h
 * Role  : VIN 私有定义。
 * Scope : sensor 链表、active sensor 状态、锁、multisensor 状态和 sensor 注册入口。
 * Notes : 仅供 tx-isp driver 内部 VIN 路径使用。
 */

#ifndef __TX_ISP_VIN_H__
#define __TX_ISP_VIN_H__

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "lib_base_sensor.h"
#include "tx-isp-device.h"
#include "txx-funcs.h"

struct tx_isp_vin_device {
	struct tx_isp_subdev sd;
	struct list_head sensors;
	struct tx_isp_sensor *active[SENSORNUM]; /* the sensor instance */

	struct mutex mlock;
	struct mutex sen_mlock[SENSORNUM];
	int state[SENSORNUM];
	unsigned int refcnt;
	multisensor_mode_t multi_mode; //0:disable, 1: enable
	/*struct tx_isp_subdev_platform_data *pdata;*/
	tisp_oneshot_state_t oneshot_state;
};

#define sd_to_vin_device(sd) (container_of(sd, struct tx_isp_vin_device, sd))

/* Weak default implementation, can be overridden by another TU. */
struct tx_isp_subdev *subdev_sensor_register_i2c_device(struct tx_isp_sensor_register_info *info);

#endif
