/*
 * Header: tx_isp_tuning.h
 * Role  : ISP tuning 私有设备定义。
 * Scope : image_tuning_vdrv_t、tuning 控制状态、锁、completion 和 event 回调。
 * Notes : 仅供 tx-isp driver 内部 tuning 路径使用。
 */

#ifndef __TX_ISP_TUNING_H__
#define __TX_ISP_TUNING_H__

#include "lib_base.h"
#include "tx-isp-device.h"

typedef struct isp_core_tuning_driver {
	struct tx_isp_subdev *parent; // which is that the driver belongs to.
	struct image_tuning_ctrls ctrls[SENSORNUM];

	spinlock_t slock;
	struct mutex mlock;
	int state;
	struct file_operations *fops;
	struct completion dncomp;
	int (*event)(struct isp_core_tuning_driver *tuning, unsigned int event, void *data);
} image_tuning_vdrv_t;

struct isp_core_tuning_driver *isp_core_tuning_init(struct tx_isp_subdev *parent);

#endif /*　__TX_ISP_TUNING_H__　*/
