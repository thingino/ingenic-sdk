// SPDX-License-Identifier: GPL-2.0
/*
 * The vendor kernel hands tx-isp a table of kernel entry points
 * (get_driver_common_interfaces()); on 7.x the table is built here from the
 * real functions, with the ones whose signatures changed wrapped.
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/gpio/consumer.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kmod.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>

#include <txx-funcs.h>
#include <jz_proc.h>
#include "tx_isp_glue.h"

extern unsigned long ispmem_base;
extern unsigned long ispmem_size;

static int i_platform_driver_register(struct platform_driver *drv)
{
	return platform_driver_register(drv);
}

static void i_platform_set_drvdata(struct platform_device *pdev, void *data)
{
	platform_set_drvdata(pdev, data);
}

static void *i_platform_get_drvdata(const struct platform_device *pdev)
{
	return platform_get_drvdata(pdev);
}

static struct resource *i_platform_get_resource(struct platform_device *dev,
						unsigned int type,
						unsigned int num)
{
	return platform_get_resource(dev, type, num);
}

static int i_dev_set_drvdata(struct device *dev, void *data)
{
	dev_set_drvdata(dev, data);
	return 0;
}

static void *i_dev_get_drvdata(const struct device *dev)
{
	return dev_get_drvdata(dev);
}

static int i_platform_get_irq(struct platform_device *dev, unsigned int num)
{
	return platform_get_irq_optional(dev, num);
}

static void i_iounmap(const volatile void __iomem *addr)
{
	iounmap(addr);
}

static int i_request_threaded_irq(unsigned int irq, irq_handler_t handler,
				  irq_handler_t thread_fn, unsigned long flags,
				  const char *name, void *dev_id)
{
	return request_threaded_irq(irq, handler, thread_fn, flags, name,
				    dev_id);
}

static void i_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
	spin_unlock_irqrestore(lock, flags);
}

static void i_mutex_lock(struct mutex *lock)
{
	mutex_lock(lock);
}

static void i_mutex_unlock(struct mutex *lock)
{
	mutex_unlock(lock);
}

static struct clk *i_clk_get(struct device *dev, const char *id)
{
	return tx_isp_clk_get(dev, id);
}

/* the vendor clk_enable() also prepared */
static int i_clk_enable(struct clk *clk)
{
	return clk_prepare_enable(clk);
}

static int i_clk_is_enabled(struct clk *clk)
{
	return __clk_is_enabled(clk);
}

static void i_clk_disable(struct clk *clk)
{
	clk_disable_unprepare(clk);
}

static unsigned long i_clk_get_rate(struct clk *clk)
{
	return clk_get_rate(clk);
}

static void i_clk_put(struct clk *clk)
{
	clk_put(clk);
}

static int i_clk_set_rate(struct clk *clk, unsigned long rate)
{
	return tx_isp_clk_set_rate(clk, rate);
}

static struct i2c_adapter *i_i2c_get_adapter(int nr)
{
	return i2c_get_adapter(nr);
}

static void i_i2c_put_adapter(struct i2c_adapter *adap)
{
	i2c_put_adapter(adap);
}

static int i_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			  int num)
{
	return i2c_transfer(adap, msgs, num);
}

static int i_i2c_register_driver(struct module *mod, struct i2c_driver *drv)
{
	return i2c_register_driver(mod, drv);
}

static void i_i2c_del_driver(struct i2c_driver *drv)
{
	i2c_del_driver(drv);
}

static struct i2c_client *i_i2c_new_device(struct i2c_adapter *adap,
					   struct i2c_board_info const *info)
{
	struct i2c_client *c = i2c_new_client_device(adap, info);

	return IS_ERR(c) ? NULL : c;
}

static void *i_i2c_get_clientdata(const struct i2c_client *c)
{
	return i2c_get_clientdata(c);
}

static void i_i2c_set_clientdata(struct i2c_client *c, void *data)
{
	i2c_set_clientdata(c, data);
}

static void i_i2c_unregister_device(struct i2c_client *c)
{
	i2c_unregister_device(c);
}

static int i_gpio_request(unsigned int gpio, const char *label)
{
	return gpio_request(gpio, label);
}

static void i_gpio_free(unsigned int gpio)
{
	gpio_free(gpio);
}

static int i_gpio_direction_output(unsigned int gpio, int value)
{
	return gpio_direction_output(gpio, value);
}

static int i_gpio_direction_input(unsigned int gpio)
{
	return gpio_direction_input(gpio);
}

static int i_gpio_set_debounce(unsigned int gpio, unsigned int debounce)
{
	return gpiod_set_debounce(gpio_to_desc(gpio), debounce);
}

/* pad muxing is the DT pin states' job; the sensors' DVP pads are set there */
static int i_jzgpio_set_func(enum gpio_port port, enum gpio_function func,
			     unsigned long pins)
{
	return tx_isp_dvp_pins(port, func, pins);
}

static int i_jzgpio_ctrl_pull(enum gpio_port port, int enable_pull,
			      unsigned long pins)
{
	return 0;
}

static void i_msleep(unsigned int msecs)
{
	msleep(msecs);
}

static bool i_capable(int cap)
{
	return capable(cap);
}

static unsigned long long i_sched_clock(void)
{
	return sched_clock();
}

static bool i_try_module_get(struct module *module)
{
	return try_module_get(module);
}

static void i_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

static int i_request_module(bool wait, const char *fmt, char *s)
{
	return wait ? request_module(fmt, s) : -ENOSYS;
}

static void i_module_put(struct module *module)
{
	module_put(module);
}

static void i_init_completion(struct completion *x)
{
	init_completion(x);
}

static void i_complete(struct completion *x)
{
	complete(x);
}

static int i_wait_for_completion_interruptible(struct completion *x)
{
	return wait_for_completion_interruptible(x);
}

static int i_misc_register(struct miscdevice *mdev)
{
	return misc_register(mdev);
}

static int i_misc_deregister(struct miscdevice *mdev)
{
	misc_deregister(mdev);
	return 0;
}

/*
 * 7.x procfs takes a proc_ops; the vendor debug nodes pass a
 * file_operations of the same open/read/llseek/release shape. One proc_ops
 * per distinct file_operations, kept for the life of the module.
 */
#define TX_ISP_PROC_OPS_MAX 16
static struct {
	const struct file_operations *fops;
	struct proc_ops ops;
} tx_isp_proc_ops[TX_ISP_PROC_OPS_MAX];
static DEFINE_MUTEX(tx_isp_proc_ops_lock);

static struct proc_dir_entry *i_proc_create_data(const char *name, umode_t mode,
		struct proc_dir_entry *parent,
		const struct file_operations *fops, void *data)
{
	struct proc_dir_entry *pde = NULL;
	int i;

	mutex_lock(&tx_isp_proc_ops_lock);
	for (i = 0; i < TX_ISP_PROC_OPS_MAX; i++) {
		if (tx_isp_proc_ops[i].fops == fops || !tx_isp_proc_ops[i].fops)
			break;
	}
	if (i < TX_ISP_PROC_OPS_MAX) {
		if (!tx_isp_proc_ops[i].fops) {
			tx_isp_proc_ops[i].fops = fops;
			tx_isp_proc_ops[i].ops.proc_open = fops->open;
			tx_isp_proc_ops[i].ops.proc_read = fops->read;
			tx_isp_proc_ops[i].ops.proc_write = fops->write;
			tx_isp_proc_ops[i].ops.proc_lseek = fops->llseek;
			tx_isp_proc_ops[i].ops.proc_release = fops->release;
		}
		pde = proc_create_data(name, mode, parent,
				       &tx_isp_proc_ops[i].ops, data);
	}
	mutex_unlock(&tx_isp_proc_ops_lock);
	return pde;
}

static ssize_t i_seq_read(struct file *file, char __user *buf, size_t size,
			  loff_t *ppos)
{
	return seq_read(file, buf, size, ppos);
}

static loff_t i_seq_lseek(struct file *file, loff_t offset, int whence)
{
	return seq_lseek(file, offset, whence);
}

static int i_single_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static int i_single_open_size(struct file *file,
			      int (*show)(struct seq_file *, void *),
			      void *data, size_t size)
{
	return single_open_size(file, show, data, size);
}

static struct proc_dir_entry *i_jz_proc_mkdir(char *s)
{
	return jz_proc_mkdir(s);
}

static void i_get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size)
{
	*phyaddr = ispmem_base;
	*size = ispmem_size;
}

static struct jz_driver_common_interfaces tx_isp_ifaces = {
	.platform_driver_register = i_platform_driver_register,
	.platform_driver_unregister = platform_driver_unregister,
	.platform_set_drvdata = i_platform_set_drvdata,
	.platform_get_drvdata = i_platform_get_drvdata,
	.platform_device_register = platform_device_register,
	.platform_device_unregister = platform_device_unregister,
	.platform_get_resource = i_platform_get_resource,
	.dev_set_drvdata = i_dev_set_drvdata,
	.dev_get_drvdata = i_dev_get_drvdata,
	.platform_get_irq = i_platform_get_irq,
	.iounmap = i_iounmap,
	.request_threaded_irq = i_request_threaded_irq,
	.enable_irq = enable_irq,
	.disable_irq = disable_irq,
	.free_irq = i_free_irq,
	.spin_unlock_irqrestore = i_spin_unlock_irqrestore,
	.mutex_lock = i_mutex_lock,
	.mutex_unlock = i_mutex_unlock,
	.clk_get = i_clk_get,
	.clk_enable = i_clk_enable,
	.clk_is_enabled = i_clk_is_enabled,
	.clk_disable = i_clk_disable,
	.clk_get_rate = i_clk_get_rate,
	.clk_put = i_clk_put,
	.clk_set_rate = i_clk_set_rate,
	.i2c_get_adapter = i_i2c_get_adapter,
	.i2c_put_adapter = i_i2c_put_adapter,
	.i2c_transfer = i_i2c_transfer,
	.i2c_register_driver = i_i2c_register_driver,
	.i2c_del_driver = i_i2c_del_driver,
	.i2c_new_device = i_i2c_new_device,
	.i2c_get_clientdata = i_i2c_get_clientdata,
	.i2c_set_clientdata = i_i2c_set_clientdata,
	.i2c_unregister_device = i_i2c_unregister_device,
	.gpio_request = i_gpio_request,
	.gpio_free = i_gpio_free,
	.gpio_direction_output = i_gpio_direction_output,
	.gpio_direction_input = i_gpio_direction_input,
	.gpio_set_debounce = i_gpio_set_debounce,
	.jzgpio_set_func = i_jzgpio_set_func,
	.jzgpio_ctrl_pull = i_jzgpio_ctrl_pull,
	.msleep = i_msleep,
	.capable = i_capable,
	.sched_clock = i_sched_clock,
	.try_module_get = i_try_module_get,
	.tx_request_module = i_request_module,
	.module_put = i_module_put,
	.init_completion = i_init_completion,
	.complete = i_complete,
	.wait_for_completion_interruptible = i_wait_for_completion_interruptible,
	.misc_register = i_misc_register,
	.misc_deregister = i_misc_deregister,
	.proc_create_data = i_proc_create_data,
	.seq_read = i_seq_read,
	.seq_lseek = i_seq_lseek,
	.single_release = i_single_release,
	.single_open_size = i_single_open_size,
	.jz_proc_mkdir = i_jz_proc_mkdir,
	.get_isp_priv_mem = i_get_isp_priv_mem,
};

struct jz_driver_common_interfaces *get_driver_common_interfaces(void)
{
	/* the vendor check compares both words with printk's address */
	tx_isp_ifaces.flags_0 = (unsigned int)(unsigned long)_printk;
	tx_isp_ifaces.flags_1 = tx_isp_ifaces.flags_0;
	return &tx_isp_ifaces;
}
