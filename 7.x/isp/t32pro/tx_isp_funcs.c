#include "lib_base.h"
#include "linux/spinlock_types.h"
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mempolicy.h>
#include <linux/mfd/core.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/pwm.h>
#include <linux/resource.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
#include <linux/i2c-gpio.h>
#endif

/* #include <gpio.h> */
#include <linux/gpio.h>

#include <soc/base.h>
#include <soc/gpio.h>

#ifdef CONFIG_KERNEL_3_10
#include <mach/platform.h>
#include <soc/irq.h>
#endif

#ifdef CONFIG_KERNEL_4_4_94
#include <dt-bindings/interrupt-controller/PRJ007-irq.h>
#endif

/* #include <mach/platform.h> */
/* #include <mach/jzdma.h> */
/* #include <mach/jzsnd.h> */
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/mipsregs.h>
#include <jz_proc.h>
#include <linux/spi/spi.h>
/* #include <linux/mfd/jz_tcu.h> */

#ifdef CONFIG_ZERATUL
#include "include/txx-funcs.h"
#else
#include <txx-funcs.h>
#endif /* CONFIG_ZERATUL */

#include "lib_base_sensor.h"
#include "tx_isp_fs.h"

extern struct platform_device tx_isp_platform_device;
extern void kernel_mxu_begin_partial(unsigned int num_regs);
extern void kernel_mxu_end(void);

/* -------------------debugfs interface------------------- */
static int print_level = ISP_WARNING_LEVEL;
module_param(print_level, int, S_IRUGO);
MODULE_PARM_DESC(print_level, "isp print level");

int isp_printf(unsigned int level, unsigned char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int ret = 0;

	if (level >= print_level) {
		va_start(args, fmt);
		vaf.fmt = fmt;
		vaf.va = &args;
		ret = printk("%pV", &vaf);
		va_end(args);
		if (level >= ISP_ERROR_LEVEL)
			dump_stack();
	}

	return ret;
}
EXPORT_SYMBOL(isp_printf);

static char *clk_name = "sclka";
module_param(clk_name, charp, S_IRUGO);
MODULE_PARM_DESC(clk_name, "chose parent clk");

static char *clks_name = "mpll";
module_param(clks_name, charp, S_IRUGO);
MODULE_PARM_DESC(clks_name, "chose scaler parent clk");

static char *clka_name = "sclka";
module_param(clka_name, charp, S_IRUGO);
MODULE_PARM_DESC(clka_name, "chose axi parent clk");

static char *clkv_name = "sclka";
module_param(clkv_name, charp, S_IRUGO);
MODULE_PARM_DESC(clkv_name, "chose axi parent clk");

static char *clkl_name = "mpll";
module_param(clkl_name, charp, S_IRUGO);
MODULE_PARM_DESC(clkl_name, "chose ldc parent clk");

static int isp_clk = 520000000;
module_param(isp_clk, int, S_IRUGO);
MODULE_PARM_DESC(isp_clk, "isp clock freq");

static int isp_clks = 476000000;
module_param(isp_clks, int, S_IRUGO);
MODULE_PARM_DESC(isp_clks, "scaler clock freq");

static int isp_clka = 600000000;
module_param(isp_clka, int, S_IRUGO);
MODULE_PARM_DESC(isp_clka, "axi clock freq");

static int isp_clkv = 500000000;
module_param(isp_clkv, int, S_IRUGO);
MODULE_PARM_DESC(isp_clkv, "axi clock freq");

static int isp_clkl = 520000000;
module_param(isp_clkl, int, S_IRUGO);
MODULE_PARM_DESC(isp_clkl, "ldc clock freq");

static int isp_memopt = 5;
module_param(isp_memopt, int, S_IRUGO);
MODULE_PARM_DESC(isp_memopt, "isp memory optimize");

unsigned int direct_mode = 0;
module_param(direct_mode, int, S_IRUGO);
MODULE_PARM_DESC(direct_mode, "isp direct mode");

unsigned int ivdc_mem_line = 0;
module_param(ivdc_mem_line, int, S_IRUGO);
MODULE_PARM_DESC(ivdc_mem_line, "ivdc mem line");

unsigned int ivdc_mem_stride = 0;
module_param(ivdc_mem_stride, int, S_IRUGO);
MODULE_PARM_DESC(ivdc_mem_stride, "ivdc mem stride");

unsigned int ivdc_threshold_line = 0;
module_param(ivdc_threshold_line, int, S_IRUGO);
MODULE_PARM_DESC(ivdc_threshold_line, "ivdc threshold line");

unsigned int ivdc_mem_paddr = 0;

#ifdef CONFIG_ZERATUL
int zeratul_open = 1;
int kernel_encode = 1;
#else
int zeratul_open = 0;
#endif /* CONFIG_ZERATUL */

module_param(zeratul_open, int, S_IRUGO);
MODULE_PARM_DESC(zeratul_open, "zeratul open");

#ifdef CONFIG_PM
int atlas_open = 1;
#else
int atlas_open = 0;
#endif

module_param(atlas_open, int, S_IRUGO);
MODULE_PARM_DESC(atlas_open, "atlas open");

int isp_cache_line = 0;
module_param(isp_cache_line, int, S_IRUGO);
MODULE_PARM_DESC(isp_cache_line, "isp cache line");

static int streamoff_mode = 0;
module_param(streamoff_mode, int, S_IRUGO);
MODULE_PARM_DESC(streamoff_mode, "sensor streamoff mode");

#ifdef CONFIG_ZERATUL
struct module *isp_module = NULL;
#else
struct module *isp_module = THIS_MODULE;
#endif /* CONFIG_ZERATUL */

#ifdef CONFIG_SOC_PRJ009
char *sclk_name[2] = {"sclka", "mpll"};
#elif defined(CONFIG_SOC_PRJ007)
char *sclk_name[3] = {"sclka", "mpll", "vpll"};
#endif

char *get_clk_name(void)
{
	return clk_name;
}

char *get_clks_name(void)
{
	return clks_name;
}

char *get_clka_name(void)
{
	return clka_name;
}

char *get_clkv_name(void)
{
	return clkv_name;
}

char *get_clkl_name(void)
{
	return clkl_name;
}

int get_isp_clk(void)
{
	return isp_clk;
}

int get_isp_clks(void)
{
	return isp_clks;
}

int get_isp_clka(void)
{
	return isp_clka;
}

int get_isp_clkv(void)
{
	return isp_clkv;
}

int get_isp_clkl(void)
{
	return isp_clkl;
}

int get_isp_memopt(void)
{
	return isp_memopt;
}

int get_isp_cache_line(void)
{
	return isp_cache_line;
}

int tx_isp_kernel_get_time(int vinum, tisp_bin_info_t *info)
{
	return *(volatile uint32_t *)0xb0002078 * 1024 / 24000;
}

unsigned int tx_isp_get_ivdc_mem_line(void)
{
	return (unsigned int)ivdc_mem_line;
}
EXPORT_SYMBOL(tx_isp_get_ivdc_mem_line);

unsigned int tx_isp_get_ivdc_mem_stride(void)
{
	return (unsigned int)ivdc_mem_stride;
}
EXPORT_SYMBOL(tx_isp_get_ivdc_mem_stride);

unsigned int tx_isp_get_ivdc_threshold_line(void)
{
	return (unsigned int)ivdc_threshold_line;
}
EXPORT_SYMBOL(tx_isp_get_ivdc_threshold_line);

unsigned int tx_isp_get_ivdc_mem_paddr(void)
{
	return (unsigned int)ivdc_mem_paddr;
}
EXPORT_SYMBOL(tx_isp_get_ivdc_mem_paddr);

int tx_isp_get_isp_memopt(void)
{
	return isp_memopt;
}
EXPORT_SYMBOL(tx_isp_get_isp_memopt);

int tx_isp_get_isp_cache_line(void)
{
	return isp_cache_line;
}
EXPORT_SYMBOL(tx_isp_get_isp_cache_line);

int tx_isp_get_direct_mode(void)
{
	return direct_mode;
}
EXPORT_SYMBOL(tx_isp_get_direct_mode);

int get_isp_streamoff_mode(void)
{
	return streamoff_mode;
}

static DECLARE_WORK(tisp_sample_work, NULL);

void tx_isp_fw_workqueue_init(tx_isp_fw_workqueue_t *work, void (*work_func)(void *work))
{
	struct work_struct *kwork = NULL;

	if (work == NULL) {
		ISP_ERROR("[ %s:%d ] invalid spinlock handle\n", __func__, __LINE__);
		return;
	}

	kwork = (struct work_struct *)work->data;
	if (kwork == NULL) {
		kwork = (struct work_struct *)private_vmalloc(sizeof(*kwork));
		if (kwork == NULL) {
			ISP_ERROR("[ %s:%d ] kmalloc spinlock failed\n", __func__, __LINE__);
			return;
		}
		work->data = kwork;
	}
	memcpy(kwork, &tisp_sample_work, sizeof(tisp_sample_work));
	kwork->func = (work_func_t)work_func;
	kwork->entry.next = &kwork->entry;
	kwork->entry.prev = &kwork->entry;
}

void tx_isp_fw_workqueue_deinit(tx_isp_fw_workqueue_t *work)
{
	struct work_struct *kwork = NULL;

	if (!work) {
		return;
	}

	kwork = (struct work_struct *)work->data;
	work->data = NULL;
	if (kwork != NULL) {
		private_vfree(kwork);
	}
}

int tx_isp_fw_schedule_workqueue(tx_isp_fw_workqueue_t *work)
{
	if (work && work->data)
		return schedule_work(work->data) ? 1 : 0;
	return 1;
}

int tx_isp_fw_flush_workqueue(tx_isp_fw_workqueue_t *work)
{
	if (work && work->data)
		return flush_work(work->data) ? 1 : 0;
	return 1;
}

void tx_isp_fw_spin_lock_init(tx_isp_fw_spinlock_t *lock)
{
	spinlock_t *slock = NULL;

	if (lock == NULL) {
		ISP_ERROR("[ %s:%d ] invalid spinlock handle\n", __func__, __LINE__);
		return;
	}

	if (sizeof(*slock)) {
		slock = (spinlock_t *)lock->data;
		if (slock == NULL) {
			// isp_printk("slock size is %d \n", sizeof(*slock));
			slock = (spinlock_t *)private_vmalloc(sizeof(*slock));
			if (slock == NULL) {
				ISP_ERROR("[ %s:%d ] kmalloc spinlock failed\n", __func__, __LINE__);
				return;
			}
			lock->data = slock;
		}
	} else {
		slock = (spinlock_t *)lock;
		lock->data = slock;
	}

	spin_lock_init(slock);
}

void tx_isp_fw_spin_lock_deinit(tx_isp_fw_spinlock_t *lock)
{
	spinlock_t *slock = NULL;

	if (!lock) {
		return;
	}

	if (sizeof(*slock)) {
		slock = (spinlock_t *)lock->data;
		lock->data = NULL;
		if (slock != NULL) {
			private_vfree(slock);
		}
	} else {
		lock->data = NULL;
	}
}

void tx_isp_fw_spin_lock_irqsave(tx_isp_fw_spinlock_t *lock, unsigned long *flags)
{
	spinlock_t *slock = NULL;

	unsigned long flags_tmp = 0;

	if (lock != NULL) {
		slock = (spinlock_t *)lock->data;
	}

	if (slock == NULL || flags == NULL) {
		ISP_ERROR("[ %s:%d ] invalid lock handle\n", __func__, __LINE__);
		return;
	}

	spin_lock_irqsave(slock, flags_tmp);
	*flags = flags_tmp;
}

void tx_isp_fw_spin_unlock_irqrestore(tx_isp_fw_spinlock_t *lock, unsigned long flags)
{
	spinlock_t *slock = NULL;

	if (lock != NULL) {
		slock = (spinlock_t *)lock->data;
	}

	if (slock == NULL) {
		ISP_ERROR("[ %s:%d ] invalid lock handle\n", __func__, __LINE__);
		return;
	}

	spin_unlock_irqrestore(slock, flags);
}

void tx_isp_fw_mutex_init(tx_isp_fw_mutex_t *lock)
{
	struct mutex *mlock = NULL;

	if (!lock) {
		return;
	}

	mlock = (struct mutex *)lock->data;
	if (mlock == NULL) {
		mlock = (struct mutex *)private_vmalloc(sizeof(*mlock));
		if (mlock == NULL) {
			ISP_ERROR("[ %s:%d ] kmalloc mutex failed\n", __func__, __LINE__);
			return;
		}
		lock->data = mlock;
	}

	mutex_init(mlock);
}

void tx_isp_fw_mutex_deinit(tx_isp_fw_mutex_t *lock)
{
	struct mutex *mlock = NULL;

	if (!lock) {
		return;
	}

	mlock = (struct mutex *)lock->data;
	lock->data = NULL;
	if (mlock != NULL) {
		private_vfree(mlock);
	}
}

void tx_isp_fw_mutex_lock(tx_isp_fw_mutex_t *lock)
{
	struct mutex *mlock = NULL;

	if (lock != NULL) {
		mlock = (struct mutex *)lock->data;
	}

	if (mlock == NULL) {
		ISP_ERROR("[ %s:%d ] invalid mutex handle\n", __func__, __LINE__);
		return;
	}

	mutex_lock(mlock);
}

void tx_isp_fw_mutex_unlock(tx_isp_fw_mutex_t *lock)
{
	struct mutex *mlock = NULL;

	if (lock != NULL) {
		mlock = (struct mutex *)lock->data;
	}

	if (mlock == NULL) {
		ISP_ERROR("[ %s:%d ] invalid mutex handle\n", __func__, __LINE__);
		return;
	}

	mutex_unlock(mlock);
}

void tx_isp_fw_completion_init(tx_isp_fw_completion_t *completion)
{
	struct completion *done = NULL;

	if (completion == NULL) {
		ISP_ERROR("[ %s:%d ] invalid completion handle\n", __func__, __LINE__);
		return;
	}

	done = (struct completion *)completion->data;
	if (done == NULL) {
		done = (struct completion *)private_vmalloc(sizeof(*done));
		if (done == NULL) {
			ISP_ERROR("[ %s:%d ] kmalloc completion failed\n", __func__, __LINE__);
			return;
		}
		completion->data = done;
	}

	init_completion(done);
}

unsigned int private_get_completion_donecnt(tx_isp_fw_completion_t *comp)
{
	struct completion *x = NULL;
	if (comp) {
		x = (void *)comp->data;
		return x ? x->done : 0;
	}
	return 0;
}

void tx_isp_fw_completion_deinit(tx_isp_fw_completion_t *completion)
{
	struct completion *done = NULL;

	if (completion == NULL) {
		return;
	}

	done = (struct completion *)completion->data;
	completion->data = NULL;
	if (done != NULL) {
		private_vfree(done);
	}
}

void tx_isp_fw_completion_complete(tx_isp_fw_completion_t *completion)
{
	struct completion *done = NULL;

	if (completion != NULL) {
		done = (struct completion *)completion->data;
	}

	if (done == NULL) {
		ISP_ERROR("[ %s:%d ] invalid completion handle\n", __func__, __LINE__);
		return;
	}

	complete(done);
}

int tx_isp_fw_completion_wait_interruptible(tx_isp_fw_completion_t *completion)
{
	struct completion *done = NULL;

	if (completion != NULL) {
		done = (struct completion *)completion->data;
	}

	if (done == NULL) {
		ISP_WARNING("invalid completion handle\n");
		return -EINVAL;
	}

	return wait_for_completion_interruptible(done);
}

int tx_isp_fw_completion_wait_interruptible_timeout(
        tx_isp_fw_completion_t *completion,
        unsigned int timeout_ms)
{
        struct completion *done = NULL;
        long ret;

        if (completion != NULL) {
                done = (struct completion *)completion->data;
        }

        if (done == NULL) {
                ISP_WARNING("invalid completion handle\n");
                return -EINVAL;
        }

        ret = wait_for_completion_interruptible_timeout(
                        done,
                        msecs_to_jiffies(timeout_ms));

        if (ret == 0) {
                /*ISP_WARNING("wait completion timeout\n");*/
                return -ETIMEDOUT;
        }

        if (ret < 0) {
                /*ISP_WARNING("wait completion interrupted\n");*/
                return ret;
        }

        return ret;
}

unsigned long tx_isp_fw_completion_wait_timeout(tx_isp_fw_completion_t *completion, unsigned long timeout)
{
	struct completion *done = NULL;

	if (completion != NULL) {
		done = (struct completion *)completion->data;
	}

	if (done == NULL) {
		ISP_ERROR("[ %s:%d ] invalid completion handle\n", __func__, __LINE__);
		return 0;
	}

	return wait_for_completion_timeout(done, timeout);
}

/* clock interfaces */
struct clk *private_clk_get(struct device *dev, const char *id)
{
	return clk_get(dev, id);
}
EXPORT_SYMBOL(private_clk_get);

struct clk *private_devm_clk_get(struct device *dev, const char *id)
{
	return devm_clk_get(dev, id);
}
EXPORT_SYMBOL(private_devm_clk_get);

int private_clk_enable(struct clk *clk)
{
	return clk_enable(clk);
}
EXPORT_SYMBOL(private_clk_enable);

int private_clk_prepare_enable(struct clk *clk)
{
	return clk_prepare_enable(clk);
}
EXPORT_SYMBOL(private_clk_prepare_enable);

#if 0
int private_clk_is_enabled(struct clk *clk)
{
	return clk_is_enabled(clk);
}
#endif

void private_clk_disable(struct clk *clk)
{
	clk_disable(clk);
}
EXPORT_SYMBOL(private_clk_disable);

void private_clk_disable_unprepare(struct clk *clk)
{
	clk_disable_unprepare(clk);
}
EXPORT_SYMBOL(private_clk_disable_unprepare);

unsigned long private_clk_get_rate(struct clk *clk)
{
	return clk_get_rate(clk);
}
EXPORT_SYMBOL(private_clk_get_rate);

void private_clk_put(struct clk *clk)
{
	return clk_put(clk);
}
EXPORT_SYMBOL(private_clk_put);

void private_devm_clk_put(struct device *dev, struct clk *clk)
{
	return devm_clk_put(dev, clk);
}
EXPORT_SYMBOL(private_devm_clk_put);

int private_clk_set_rate(struct clk *clk, unsigned long rate)
{
	return clk_set_rate(clk, rate);
}
EXPORT_SYMBOL(private_clk_set_rate);

/* i2c interfaces */
struct i2c_adapter *private_i2c_get_adapter(int nr)
{
	return i2c_get_adapter(nr);
}

void private_i2c_put_adapter(struct i2c_adapter *adap)
{
	i2c_put_adapter(adap);
}

int private_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	return i2c_transfer(adap, msgs, num);
}
EXPORT_SYMBOL(private_i2c_transfer);

int private_i2c_register_driver(struct module *owner, struct i2c_driver *driver)
{
#ifdef CONFIG_ZERATUL
	return i2c_register_driver(NULL, driver);
#else
	return i2c_register_driver(owner, driver);
#endif /* CONFIG_ZERATUL */
}

void private_i2c_del_driver(struct i2c_driver *drv)
{
	i2c_del_driver(drv);
}
EXPORT_SYMBOL(private_i2c_del_driver);

struct i2c_client *private_i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
{
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
	return i2c_new_device(adap, info);
#endif
#ifdef CONFIG_KERNEL_5_15
	return i2c_new_client_device(adap, info);
#endif
}

void private_i2c_set_clientdata(struct i2c_client *dev, void *data)
{
	i2c_set_clientdata(dev, data);
}
EXPORT_SYMBOL(private_i2c_set_clientdata);

void *private_i2c_get_clientdata(const struct i2c_client *dev)
{
	return i2c_get_clientdata(dev);
}
EXPORT_SYMBOL(private_i2c_get_clientdata);

int private_i2c_add_driver(struct i2c_driver *drv)
{
	return i2c_add_driver(drv);
}
EXPORT_SYMBOL(private_i2c_add_driver);

void private_i2c_unregister_device(struct i2c_client *client)
{
	i2c_unregister_device(client);
}

/* gpio interfaces */
int private_gpio_request(unsigned gpio, const char *label)
{
	return gpio_request(gpio, label);
}
EXPORT_SYMBOL(private_gpio_request);

void private_gpio_free(unsigned gpio)
{
	gpio_free(gpio);
}
EXPORT_SYMBOL(private_gpio_free);

int private_gpio_direction_output(unsigned gpio, int value)
{
	return gpio_direction_output(gpio, value);
}
EXPORT_SYMBOL(private_gpio_direction_output);

int private_gpio_direction_input(unsigned gpio)
{
	return gpio_direction_input(gpio);
}

int private_gpio_set_debounce(unsigned gpio, unsigned debounce)
{
	return gpiod_set_debounce(gpio_to_desc(gpio), debounce);
}

int private_jzgpio_set_func(enum gpio_port port, enum gpio_function func, unsigned long pins)
{
	return jzgpio_set_func(port, func, pins);
}
EXPORT_SYMBOL(private_jzgpio_set_func);

int set_sensor_gpio_function(int func_set)
{
	int ret = 0;

	*(volatile unsigned int *)(0xB0010130) = 0x2aaa000;
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
EXPORT_SYMBOL(set_sensor_gpio_function);

int set_sensor_mclk_function(int mclk_set)
{
	int ret = 0;

	switch (mclk_set) {
	case 0:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_0, 0x00010000);
		pr_info("set sensor mclk(%d) gpio\n", mclk_set);
		break;
	case 1:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_0, 0x00020000);
		pr_info("set sensor mclk(%d) gpio\n", mclk_set);
		break;
	case 2:
		ret = private_jzgpio_set_func(GPIO_PORT_A, GPIO_FUNC_0, 0x00040000);
		pr_info("set sensor mclk(%d) gpio\n", mclk_set);
		break;
	default:
		pr_err("set sensor mclk error: unknow function %d\n", mclk_set);
		ret = -1;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(set_sensor_mclk_function);

#if 0
int private_jzgpio_ctrl_pull(enum gpio_port port, int enable_pull,unsigned long pins)
{
	return jzgpio_ctrl_pull(port, enable_pull,pins);
}
#endif

/* system interfaces */
void private_msleep(unsigned int msecs)
{
	msleep(msecs);
}
EXPORT_SYMBOL(private_msleep);
void private_mdelay(unsigned int msecs)
{
	mdelay(msecs);
}
EXPORT_SYMBOL(private_mdelay);

void private_mxu_begin_partial(unsigned int num_regs)
{
	kernel_mxu_begin_partial(num_regs);
}
EXPORT_SYMBOL(private_mxu_begin_partial);

void private_mxu_end(void)
{
	kernel_mxu_end();
}
EXPORT_SYMBOL(private_mxu_end);

int private_strcmp(const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}

int private_strncmp(const char *s1, const char *s2, size_t n)
{
	return strncmp(s1, s2, n);
}

char *private_strncpy(char *dest, const char *src, size_t n)
{
	size_t len = strnlen(src, n);

	/* strncpy() is gone on 7.x: copy and zero-pad as it did */
	memcpy(dest, src, len);
	memset(dest + len, 0, n - len);
	return dest;
}

int private_wait_event_interruptible(tx_isp_fw_wait_queue_head_t *q, int (*state)(void *), void *data)
{
	wait_queue_head_t *wq = NULL;
	int condition = 0;
	int ret = 0;

	if (q == NULL || q->data || state == NULL)
		return -EINVAL;
	condition = state(data);
	if (!condition) {
		DEFINE_WAIT(__wait);
		for (;;) {
			prepare_to_wait(wq, &__wait, TASK_INTERRUPTIBLE);
			if (condition)
				break;
			if (!signal_pending(current)) {
				schedule();
				continue;
			}
			ret = -ERESTARTSYS;
			break;
		}
		finish_wait(wq, &__wait);
	}
	return ret;
}

void private_wake_up_all(tx_isp_fw_wait_queue_head_t *q)
{
	wait_queue_head_t *wq = NULL;

	if (q != NULL && q->data != NULL) {
		wq = q->data;
		wake_up_all(wq);
	}
}

void private_wake_up(tx_isp_fw_wait_queue_head_t *q)
{
	wait_queue_head_t *wq = NULL;

	if (q != NULL && q->data != NULL) {
		wq = q->data;
		wake_up(wq);
	}
}

void private_init_waitqueue_head(tx_isp_fw_wait_queue_head_t *q)
{
	wait_queue_head_t *wq = NULL;

	if (q == NULL) {
		ISP_ERROR("[ %s:%d ] invalid waitqueue handle\n", __func__, __LINE__);
		return;
	}

	wq = (void *)q->data;
	if (!wq) {
		wq = (void *)private_vmalloc(sizeof(*wq));
		if (wq == NULL) {
			ISP_ERROR("[ %s:%d ] malloc waitqueue failed\n", __func__, __LINE__);
			return;
		}
		q->data = wq;
	}

	init_waitqueue_head(wq);
}

void private_deinit_waitqueue_head(tx_isp_fw_wait_queue_head_t *q)
{
	if (q == NULL || q->data == NULL) {
		return;
	}

	private_vfree(q->data);
	q->data = NULL;
	return;
}

// malloc
#ifndef MALLOC_CHECK
void *private_vmalloc(unsigned long size)
{
	void *addr = NULL;
	if (size >= 4 * 1024) {
		addr = vmalloc(size);
	} else {
		addr = kmalloc(size, GFP_KERNEL);
	}
	return addr;
}

void private_vfree(const void *addr)
{
	if (is_vmalloc_addr(addr)) {
		vfree(addr);
	} else {
		kfree(addr);
	}
}

void *private_kmalloc(size_t s)
{
	void *addr = kmalloc(s, GFP_KERNEL);
	return addr;
}

void *private_dma_alloc_noncoherent(size_t size, unsigned int *dma_handle)
{
	dma_addr_t dma_addr = 0;
#ifdef CONFIG_KERNEL_5_15
	void *cpu_addr = dma_alloc_noncoherent(&tx_isp_platform_device.dev, size, &dma_addr, DMA_BIDIRECTIONAL, GFP_KERNEL);
#else
	void *cpu_addr = dma_alloc_noncoherent(&tx_isp_platform_device.dev, size, &dma_addr, GFP_KERNEL);
#endif

	if (cpu_addr == NULL)
		return NULL;

	if (dma_handle)
		*dma_handle = (unsigned int)dma_addr;

	return cpu_addr;
}

void private_dma_free_noncoherent(size_t size, void *cpu_addr, unsigned int dma_handle)
{
#ifdef CONFIG_KERNEL_5_15
	dma_free_noncoherent(&tx_isp_platform_device.dev, size, cpu_addr, (dma_addr_t)dma_handle, DMA_BIDIRECTIONAL);
#else
	dma_free_noncoherent(&tx_isp_platform_device.dev, size, cpu_addr, (dma_addr_t)dma_handle);
#endif
}

void private_kfree(void *p)
{
	kfree(p);
}
#else
void *test_malloc_addr = 0;
#endif /* MALLOC_CHECK */

// copy user
long private_copy_from_user(void *to, const void __user *from, long size)
{
	if (tx_isp_get_device_opstype() == TISP_OPS_KERNEL) {
		memcpy(to, (const void *)(unsigned long)from, size);
		return 0;
	}
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
	if (!access_ok(VERIFY_READ, from, size)) {
		memcpy(to, from, size);
		return size;
	}
#endif
#if defined(CONFIG_KERNEL_5_15)
	if (!access_ok(from, size)) {
		memcpy(to, from, size);
		return size;
	}
#endif
	return copy_from_user(to, from, size);
}

long private_copy_to_user(void __user *to, const void *from, long size)
{
	if (tx_isp_get_device_opstype() == TISP_OPS_KERNEL) {
		memcpy((void *)(unsigned long)to, from, size);
		return 0;
	}
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
	if (!access_ok(VERIFY_WRITE, to, size)) {
		memcpy(to, from, size);
		return size;
	}
#endif
#if defined(CONFIG_KERNEL_5_15)
	if (!access_ok(to, size)) {
		memcpy(to, from, size);
		return size;
	}
#endif
	return copy_to_user(to, from, size);
}

static void tx_isp_sync_phys(phys_addr_t paddr, size_t size, enum dma_data_direction dir);

void private_dma_phy_cache_sync(void *paddr, size_t size, enum dma_data_direction direction)
{
	tx_isp_sync_phys((phys_addr_t)(unsigned long)paddr, size, direction);
}

void private_getrawmonotonic(tx_isp_fw_timespec_t *ts)
{
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
	struct timespec k_ts;
	if(!ts)
		return;
	getrawmonotonic(&k_ts);
	ts->ts_sec = k_ts.tv_sec;
	ts->ts_nsec = k_ts.tv_nsec;
#endif
#ifdef CONFIG_KERNEL_5_15
	struct timespec64 k_ts;
	if(!ts)
		return;
	ktime_get_real_ts64(&k_ts);
	ts->ts_sec = k_ts.tv_sec;
	ts->ts_nsec = k_ts.tv_nsec;
#endif
}

/* kthread interfaces */

bool private_kthread_should_stop(void)
{
	return kthread_should_stop();
}

struct task_struct *private_kthread_run(int (*threadfn)(void *data), void *data, const char namefmt[])
{
	return kthread_run(threadfn, data, namefmt);
}

int private_kthread_stop(struct task_struct *k)
{
	return kthread_stop(k);
}

int private_pthread_create(unsigned long *thread, const void *attr, void *(*start_routine)(void *), void *arg)
{
	struct task_struct *task;

	(void)attr;
	if (!thread || !start_routine)
		return -EINVAL;

	/*
	 * Firmware side uses a pthread-like signature; map it onto kthread_run.
	 * On this platform pointer/int return width is identical, so this cast is safe.
	 */
	task = private_kthread_run((int (*)(void *))(void *)start_routine, arg, "txisp_pthread");
	if (IS_ERR(task))
		return PTR_ERR(task);

	*thread = (unsigned long)task;
	return 0;
}

int private_pthread_join(unsigned long thread, void **retval)
{
	int ret;

	if (!thread)
		return -EINVAL;

	ret = private_kthread_stop((struct task_struct *)thread);
	if (retval)
		*retval = (void *)(unsigned long)ret;

	return 0;
}

/* the vendor kernels' ispmem=<size>@<base> carve-out, a module parameter here */
static unsigned long ispmem_base;
static unsigned long ispmem_size;
module_param(ispmem_base, ulong, 0444);
MODULE_PARM_DESC(ispmem_base, "ISP private memory physical base");
module_param(ispmem_size, ulong, 0444);
MODULE_PARM_DESC(ispmem_size, "ISP private memory size in bytes");

static void get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size)
{
	*phyaddr = ispmem_base;
	*size = ispmem_size;
}

void private_get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size)
{
	get_isp_priv_mem(phyaddr, size);
}

ktime_t private_ktime_set(const long secs, const unsigned long nsecs)
{
	return ktime_set(secs, nsecs);
}

void *private_phys_to_virt(unsigned int phys_addr)
{
	return phys_to_virt((phys_addr_t)phys_addr);
}
unsigned int private_virt_to_phys(const void *virt_addr)
{
	return virt_to_phys(virt_addr);
}

int private_frame_channel_misc_register(int chn, char *name, void *data)
{
	return frame_channel_misc_register(chn, name, data);
}

void private_frame_channel_misc_deregister(int chn, char *name)
{
	frame_channel_misc_deregister(chn, name);
	return;
}

/*
 * dma_cache_sync() is gone on 7.x. Sync what lies in the kernel's RAM through
 * the DMA API, page by page for vmalloc buffers; the reserved media memory has
 * no struct page and is reached uncached.
 */
static void tx_isp_sync_phys(phys_addr_t paddr, size_t size, enum dma_data_direction dir)
{
	struct device *d = &tx_isp_platform_device.dev;

	if (!pfn_valid(PHYS_PFN(paddr)))
		return;
	if (dir == DMA_FROM_DEVICE)
		dma_sync_single_for_cpu(d, paddr, size, dir);
	else
		dma_sync_single_for_device(d, paddr, size, dir);
}

void private_dma_cache_sync(struct device *dev, void *vaddr, size_t size, enum dma_data_direction direction)
{
	unsigned long va = (unsigned long)vaddr;
	size_t off, chunk;

	if (is_vmalloc_addr(vaddr)) {
		for (off = 0; off < size; off += chunk) {
			struct page *pg = vmalloc_to_page(vaddr + off);

			chunk = min_t(size_t, size - off, PAGE_SIZE - offset_in_page(va + off));
			if (pg)
				tx_isp_sync_phys(page_to_phys(pg) + offset_in_page(va + off), chunk, direction);
		}
		return;
	}
	if (va >= CKSEG0 && va < CKSEG1)
		tx_isp_sync_phys(CPHYSADDR(va), size, direction);
}

int private_write_c0_watchlo0(unsigned int watch_addr)
{
       write_c0_watchlo0(watch_addr);
       return 0;
}

int private_write_c0_watchhi0(unsigned int watch_addr)
{
       write_c0_watchhi0(watch_addr);
       return 0;
}
