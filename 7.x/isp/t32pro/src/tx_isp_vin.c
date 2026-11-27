#include "tx_isp_vin.h"

struct tx_isp_subdev *isp_i2c_new_subdev_board(struct i2c_adapter *adapter, struct i2c_board_info *info, const unsigned short *probe_addrs)
{
	struct tx_isp_subdev *sd = NULL;
	struct i2c_client *client;
#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
	struct i2c_driver *driver;
#endif

#if 0
	request_module(I2C_MODULE_PREFIX "%s", info->type);
#else
	request_module(I2C_MODULE_PREFIX "%s", info->type);
#endif

	/* Create the i2c client */
	if (info->addr == 0) {
		ISP_WARNING("[ %s:%d ]\n", __func__, __LINE__);
		return NULL;
	} else {
		client = private_i2c_new_device(adapter, info); // sensor probe
	}

	/* Note: by loading the module first we are certain that c->driver
	   will be set if the driver was found. If the module was not loaded
	   first, then the i2c core tries to delay-load the module for us,
	   and then c->driver is still NULL until the module is finally
	   loaded. This delay-load mechanism doesn't work if other drivers
	   want to use the i2c device, so explicitly loading the module
	   is the best alternative. */
#ifdef CONFIG_KERNEL_3_10
	if (client == NULL || client->driver == NULL)
		goto error;

	/* Lock the module so we can safely get the tx_isp_subdev pointer */
	if (!try_module_get(client->driver->driver.owner))
		goto error;
	sd = private_i2c_get_clientdata(client);

	/* Decrease the module use count to match the first try_module_get. */
	module_put(client->driver->driver.owner);
#endif

#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
	driver = to_i2c_driver(client->dev.driver);
	if (client == NULL || driver == NULL)
		goto error;

	/* Lock the module so we can safely get the tx_isp_subdev pointer */
	if (!try_module_get(driver->driver.owner))
		goto error;
	sd = private_i2c_get_clientdata(client);

	/* Decrease the module use count to match the first try_module_get. */
	module_put(driver->driver.owner);
#endif

error:
	/* If we have a client but no subdev, then something went wrong and
	   we must unregister the client. */
	if (client && sd == NULL)
		private_i2c_unregister_device(client);

	return sd;
}

/**
 * 注册传感器I2C设备
 * @param info 传感器注册信息结构体指针，包含I2C配置和传感器ID等信息
 * @return 成功返回传感器子设备指针，失败返回NULL
 */
struct tx_isp_subdev *private_subdev_sensor_register_i2c_device(struct tx_isp_sensor_register_info *info)
{
	struct i2c_adapter *adapter; // I2C适配器指针
	struct i2c_board_info board_info;
	struct tx_isp_subdev *sensor_sd = NULL;
	struct tx_isp_sensor *sensor = NULL;

	printk("private_i2c_get_adapter \n");
	adapter = private_i2c_get_adapter(info->i2c.i2c_adapter_id);
	if (!adapter) {
		ISP_ERROR("[ %s:%d ] Failed to get I2C adapter %d, deferring probe!!!\n", __func__, __LINE__, info->i2c.i2c_adapter_id);
		return NULL;
	}
	memset(&board_info, 0, sizeof(board_info));
	memcpy(&board_info.type, &info->i2c.type, I2C_NAME_SIZE);
	board_info.addr = info->i2c.addr;
	ISP_INFO("[ %s:%d ] sensor id is %d, i2c name size is %d, type is %s, addr is 0x%02x, adapter id is %d\n", __func__, __LINE__, info->sensor_id, I2C_NAME_SIZE, board_info.type, info->i2c.addr, info->i2c.i2c_adapter_id);
	sensor_sd = isp_i2c_new_subdev_board(adapter, &board_info, NULL);
	if (IS_ERR_OR_NULL(sensor_sd)) {
		private_i2c_put_adapter(adapter);
		ISP_ERROR("[ %s:%d ] Failed to acquire subdev %s, deferring probe\n", __func__, __LINE__, info->i2c.type);
		return NULL;
	}

	sensor = tx_isp_get_subdev_hostdata(sensor_sd);
	sensor->info = *info;
	sensor->video.info = &sensor->info;
	sensor->fsync_flag = 0;

	return sensor_sd;
}

void private_subdev_sensor_unregister_i2c_device(struct tx_isp_subdev *sensor_sd)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sensor_sd);
	struct i2c_adapter *adapter = client->adapter;
	if (adapter)
		private_i2c_put_adapter(adapter);
	private_i2c_unregister_device(client);
}

void *private_get_sensor_list(struct tx_isp_subdev *sensor_sd)
{
	struct tx_isp_sensor *sensor = NULL;
	sensor = tx_isp_get_subdev_hostdata(sensor_sd);

	return &sensor->list;
}
