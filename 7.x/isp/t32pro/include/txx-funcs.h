/*
 * Header: txx-funcs.h
 * Role  : driver 辅助接口声明。
 * Scope : log、clock、GPIO、I2C、IRQ、proc、memory、cache、thread、user-copy 和 register access wrapper。
 * Notes : 接口实现位于 tx_isp_funcs.c。
 */

#ifndef __TXX_DRV_FUNCS_H__
#define __TXX_DRV_FUNCS_H__

#include <linux/clk.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pwm.h>
/*#include <linux/list.h>*/
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/time.h>
/*#include <linux/delay.h>*/
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/mempolicy.h>
#include <linux/mfd/core.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#include "lib_base.h"
#include "lib_base.h"

#ifdef CONFIG_KERNEL_3_10
#include <mach/platform.h>
#include <soc/irq.h>
#endif

#ifdef CONFIG_KERNEL_4_4_94
#include <dt-bindings/interrupt-controller/PRJ007-irq.h>
#endif

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <soc/base.h>
#include <soc/gpio.h>
/*#include <linux/seq_file.h>*/
#include <jz_proc.h>
#include <linux/hrtimer.h>
#ifdef CONFIG_KERNEL_5_15
#include <linux/proc_fs.h>
#endif

#ifndef U16_MAX
#define U16_MAX 0xFFFF
#endif

#define ISP_INFO_LEVEL 0x0
#define ISP_WARNING_LEVEL 0x1
#define ISP_ERROR_LEVEL 0x2
#define ISP_PRINT(level, format, ...) isp_printf(level, "[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define ISP_INFO(format, ...) ISP_PRINT(ISP_INFO_LEVEL, format, ##__VA_ARGS__)
#define ISP_WARNING(format, ...) ISP_PRINT(ISP_WARNING_LEVEL, format, ##__VA_ARGS__)
#define ISP_ERROR(format, ...) ISP_PRINT(ISP_ERROR_LEVEL, format, ##__VA_ARGS__)
#define ISP_PRINTK(format, ...) ISP_WARNING(format, ##__VA_ARGS__)

// extern unsigned int isp_print_level;
/*int isp_debug_init(void);*/
/*int isp_debug_deinit(void);*/
#ifdef CONFIG_SOC_PRJ008
extern char *sclk_name[2];
#elif defined(CONFIG_SOC_PRJ007)
extern char *sclk_name[3];
#endif
extern struct module *isp_module;
int isp_printf(unsigned int level, unsigned char *fmt, ...);
char *get_clk_name(void);
char *get_clks_name(void);
char *get_clka_name(void);
char *get_clkv_name(void);
char *get_clkl_name(void);
int get_isp_clk(void);
int get_isp_clks(void);
int get_isp_clka(void);
int get_isp_clkv(void);
int get_isp_clkl(void);
int get_isp_memopt(void);
int get_isp_cache_line(void);
int get_isp_streamoff_mode(void);
int get_isp_rmem_info(unsigned long *rmem_addr, unsigned long *size);
unsigned int tx_isp_get_ivdc_mem_line(void);
unsigned int tx_isp_get_ivdc_mem_stride(void);
unsigned int tx_isp_get_ivdc_mem_paddr(void);
unsigned int tx_isp_get_ivdc_threshold_line(void);
int tx_isp_get_isp_memopt(void);
int tx_isp_get_isp_cache_line(void);
int tx_isp_get_direct_mode(void);

#define paddr2vaddr(phyaddr) ((void *)((phyaddr) + PAGE_OFFSET - PHYS_OFFSET))

struct resource *private_platform_get_resource(struct platform_device *dev, unsigned int type, unsigned int num);
void private_dev_set_drvdata(struct device *dev, void *data);
void *private_dev_get_drvdata(const struct device *dev);
int private_platform_get_irq(struct platform_device *dev, unsigned int num);
struct resource *private_request_mem_region(resource_size_t start, resource_size_t n, const char *name);
void private_release_mem_region(resource_size_t start, resource_size_t n);

void __iomem *private_ioremap(phys_addr_t offset, unsigned long size);
void private_iounmap(const volatile void __iomem *addr);
/* interrupt interfaces */
int private_request_threaded_irq(unsigned int irq, irq_handler_t handler, irq_handler_t thread_fn, unsigned long irqflags, const char *devname, void *dev_id);
int private_request_irq(unsigned int irq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id);
void private_enable_irq(unsigned int irq);
void private_disable_irq(unsigned int irq);
void private_free_irq(unsigned int irq, void *dev_id);

struct clk *private_devm_clk_get(struct device *dev, const char *id);
int private_clk_prepare_enable(struct clk *clk);
void private_clk_disable_unprepare(struct clk *clk);
void private_devm_clk_put(struct device *dev, struct clk *clk);

/* i2c interfaces */
struct i2c_adapter *private_i2c_get_adapter(int nr);
void private_i2c_put_adapter(struct i2c_adapter *adap);
int private_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
int private_i2c_register_driver(struct module *, struct i2c_driver *);
void private_i2c_del_driver(struct i2c_driver *drv);
struct i2c_client *private_i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info);
void private_i2c_set_clientdata(struct i2c_client *dev, void *data);
void *private_i2c_get_clientdata(const struct i2c_client *dev);
int private_i2c_add_driver(struct i2c_driver *drv);
void private_i2c_unregister_device(struct i2c_client *client);

/* gpio interfaces */
int private_gpio_request(unsigned gpio, const char *label);
void private_gpio_free(unsigned gpio);
int private_gpio_direction_output(unsigned gpio, int value);
int private_gpio_direction_input(unsigned gpio);
int private_gpio_set_debounce(unsigned gpio, unsigned debounce);
int private_jzgpio_set_func(enum gpio_port port, enum gpio_function func, unsigned long pins);
// int private_jzgpio_ctrl_pull(enum gpio_port port, int enable_pull,unsigned long pins);

/* system interfaces */
void private_msleep(unsigned int msecs);
void private_mdelay(unsigned int msecs);
void private_mxu_begin_partial(unsigned int num_regs);
void private_mxu_end(void);
bool private_capable(int cap);
unsigned long long private_sched_clock(void);
bool private_try_module_get(struct module *module);
int private_request_module(bool wait, const char *fmt, ...);
void private_module_put(struct module *module);
void system_reg_write(unsigned int reg, unsigned int value);
unsigned int system_reg_read(unsigned int reg);
void system_reg_ivdc_write(unsigned int reg, unsigned int value);
unsigned int system_reg_ivdc_read(unsigned int reg);

int private_strcmp(const char *s1, const char *s2);
int private_strncmp(const char *s1, const char *s2, size_t n);
char *private_strncpy(char *dest, const char *src, size_t n);

/* wait interfaces */
void private_init_completion(struct completion *x);
void private_complete(struct completion *x);
int private_wait_for_completion_interruptible(struct completion *x);
unsigned long private_wait_for_completion_timeout(struct completion *x, unsigned long timeover);
int private_wait_event_interruptible(tx_isp_fw_wait_queue_head_t *q, int (*state)(void *), void *data);
void private_wake_up_all(tx_isp_fw_wait_queue_head_t *q);
void private_wake_up(tx_isp_fw_wait_queue_head_t *q);
void private_init_waitqueue_head(tx_isp_fw_wait_queue_head_t *q);
int private_write_c0_watchlo0(unsigned int watch_addr);
int private_write_c0_watchhi0(unsigned int watch_addr);
/* misc driver interfaces */
int private_misc_register(struct miscdevice *mdev);
void private_misc_deregister(struct miscdevice *mdev);

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
struct proc_dir_entry *private_proc_create(const char *name, umode_t mode, struct proc_dir_entry *parent, const struct file_operations *proc_fops);
#endif
#ifdef CONFIG_KERNEL_5_15
struct proc_dir_entry *private_proc_create(const char *name, umode_t mode, struct proc_dir_entry *parent, const struct proc_ops *proc_fops);
#endif

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode, struct proc_dir_entry *parent, const struct file_operations *proc_fops, void *data);
#endif
#ifdef CONFIG_KERNEL_5_15
struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode, struct proc_dir_entry *parent, const struct proc_ops *proc_fops, void *data);
#endif
/* proc file interfaces */
ssize_t private_seq_read(struct file *file, char __user *buf, size_t size, loff_t *ppos);
loff_t private_seq_lseek(struct file *file, loff_t offset, int whence);
int private_single_release(struct inode *inode, struct file *file);
int private_single_open_size(struct file *file, int (*show)(struct seq_file *, void *), void *data, size_t size);
struct proc_dir_entry *private_jz_proc_mkdir(char *s);
void private_proc_remove(struct proc_dir_entry *de);
void private_seq_printf(struct seq_file *m, const char *f, ...);
unsigned long long private_simple_strtoull(const char *cp, char **endp, unsigned int base);

/* kthread interfaces */
bool private_kthread_should_stop(void);
struct task_struct *private_kthread_run(int (*threadfn)(void *data), void *data, const char namefmt[]);
int private_kthread_stop(struct task_struct *k);
int private_pthread_create(unsigned long *thread, const void *attr, void *(*start_routine)(void *), void *arg);
int private_pthread_join(unsigned long thread, void **retval);

/* #define MALLOC_CHECK */
#ifndef MALLOC_CHECK
void *private_vmalloc(unsigned long size);
void private_vfree(const void *addr);
void *private_kmalloc(size_t s);
void *private_dma_alloc_noncoherent(size_t size, unsigned int *dma_handle);
void private_dma_free_noncoherent(size_t size, void *cpu_addr, unsigned int dma_handle);
void private_kfree(void *p);
#else
#include <linux/vmalloc.h>
extern void *test_malloc_addr;
#define private_vmalloc(size) (test_malloc_addr = vmalloc(size), ISP_WARNING("[%s %d] vmalloc %p %d\n", __func__, __LINE__, test_malloc_addr, size), test_malloc_addr)
#define private_vfree(addr)                                          \
	{                                                                \
		vfree(addr);                                                 \
		ISP_WARNING("[%s %d] vfree %p\n", __func__, __LINE__, addr); \
	}
#define private_kmalloc(s, gfp) (test_malloc_addr = kmalloc(s, gfp), ISP_WARNING("[%s %d] kmalloc %p %d\n", __func__, __LINE__, test_malloc_addr, s), test_malloc_addr)
#define private_kfree(addr) (kfree(addr), ISP_WARNING("[%s %d] kfree %p\n", __func__, __LINE__, addr))
#endif /* MALLOC_CHECK */
long private_copy_from_user(void *to, const void __user *from, long size);
long private_copy_to_user(void __user *to, const void *from, long size);

/* file ops */
struct file *private_filp_open(const char *filename, int flags, umode_t mode);
int private_filp_close(struct file *filp, fl_owner_t id);

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
ssize_t private_vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
ssize_t private_vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
#endif
#ifdef CONFIG_KERNEL_5_15
ssize_t private_vfs_read(struct file *file, void *buf, size_t count, loff_t *pos);
ssize_t private_vfs_write(struct file *file, const void *buf, size_t count, loff_t *pos);
#endif
loff_t private_vfs_llseek(struct file *file, loff_t offset, int whence);
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
mm_segment_t private_get_fs(void);
#endif
void private_dma_cache_sync(struct device *dev, void *vaddr, size_t size, enum dma_data_direction direction);
void private_getrawmonotonic(tx_isp_fw_timespec_t *ts);
struct net *private_get_init_net(void);

ktime_t private_ktime_set(const long secs, const unsigned long nsecs);
void private_set_current_state(unsigned int state);
int private_schedule_hrtimeout(ktime_t *ex, const enum hrtimer_mode mode);
bool private_schedule_work(struct work_struct *work);
bool private_flush_work(struct work_struct *work);

/* isp driver interface */
void private_get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size);

/* int private_driver_get_interface(void); */

/*   new  */
void tx_isp_fw_completion_init(tx_isp_fw_completion_t *completion);
void tx_isp_fw_completion_deinit(tx_isp_fw_completion_t *completion);
tx_isp_ops_type_t tx_isp_get_device_opstype(void);

/* sensor 驱动使用*/
int private_clk_set_rate(struct clk *clk, unsigned long rate);
unsigned long private_clk_get_rate(struct clk *clk);

int tx_isp_kernel_cd_int_handler(int chn, int buf_index, long long time);
int tx_isp_kernel_get_flag_sum(void);
int tx_isp_kernel_get_channel_nrvbs(int chn);
char* tx_isp_kernel_get_fast_sensor_name(int vinum);
void tx_isp_kernel_ttff_debug(char *func, unsigned int line, char *buf);

#endif /*__TXX_DRV_FUNCS_H__*/
