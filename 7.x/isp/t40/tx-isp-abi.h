/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __TX_ISP_ABI_H__
#define __TX_ISP_ABI_H__

#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/time64.h>
#include <linux/dma-direction.h>

struct cdev;
struct miscdevice;
struct task_struct;
struct sched_param;
struct device;
struct file_operations;
struct net;
struct netlink_kernel_cfg;
struct nlmsghdr;
struct sk_buff;
struct sock;
struct socket;
struct file;
struct platform_driver;
struct proc_dir_entry;
#include <linux/fs.h>
struct seq_file;
struct inode;
struct class;
struct clk;
struct module;
struct lock_class_key;
struct i2c_driver;
struct tx_isp_subdev;

int tx_isp_sinfo_init(void);
void tx_isp_sinfo_exit(void);
int tx_isp_sinfo_driver_add(struct i2c_driver *drv, int def_i2c_addr,
			    struct module *owner);
void tx_isp_sinfo_driver_del(struct i2c_driver *drv);
int tx_isp_sinfo_sensor_bind(struct tx_isp_subdev *sd, struct module *owner);
void tx_isp_sinfo_sensor_unbind(struct tx_isp_subdev *sd, struct module *owner);

/* tx-isp-twin.c: file_operations 33 slots, wrapped; blob is the firmware's miscdevice or NULL */
struct fw_fops {
	struct file_operations fops;
	const void *orig[33];
	void *blob;
};
void fw_fops_init(struct fw_fops *t, const void *f44, void *blob);
struct proc_dir_entry *t40_proc_create_data(const char *name, umode_t mode,
					    struct proc_dir_entry *parent,
					    const void *f44, void *data);
void t40_proc_remove(struct proc_dir_entry *de);
void *t40_proc_data(const struct inode *inode);
int t40_single_open_size(struct file *twin, int (*show)(struct seq_file *, void *),
			 void *data, size_t size);
int t40_single_release(struct inode *inode, struct file *twin);
ssize_t t40_seq_read(struct file *twin, char __user *buf, size_t size, loff_t *ppos);
loff_t t40_seq_lseek(struct file *twin, loff_t offset, int whence);
struct seq_file *t40_seq_real(void *twin);
struct file *t40_filp_open(const char *path, int flags, umode_t mode);
int t40_filp_close(struct file *handle, void *id);
ssize_t t40_vfs_read(struct file *handle, char __user *buf, size_t count, loff_t *pos);
ssize_t t40_vfs_write(struct file *handle, const char __user *buf, size_t count,
		      loff_t *pos);
loff_t t40_vfs_llseek(struct file *handle, loff_t offset, int whence);
int t40_platform_driver_register_44(struct platform_driver *drv, struct module *owner);
void t40_cdev_init_44(struct cdev *cdev, const void *fops44);
void t40_cdev_del_44(struct cdev *cdev);

/* the targets of the Kbuild's objcopy redirections */
void *t40_kmem_cache_alloc_44(void *cache, gfp_t gfp);
struct class *t40_class_create_44(struct module *owner, const char *name,
				   struct lock_class_key *key);
void t40_class_destroy_44(struct class *c);
struct clk *t40_clk_get_44(struct device *dev, const char *id);
int t40_clk_set_parent_44(struct clk *clk, struct clk *parent);
void t40_warn_slowpath_null_44(const char *file, int line);
int t40_dma_alloc_from_coherent_44(struct device *dev, ssize_t size,
				   dma_addr_t *handle, void **ret);
int t40_dma_release_from_coherent_44(struct device *dev, int order, void *vaddr);
void t40_device_destroy_44(struct class *c, dev_t devt);
void *t40_dev_get_drvdata_44(const struct device *dev);
void *t40_pde_data_44(const struct inode *inode);
void t40_might_sleep_44(const char *file, int line, int preempt_offset);
void t40_preempt_schedule_44(void);
signed long t40_schedule_timeout_44(signed long timeout);
long t40_prepare_to_wait_event_44(struct wait_queue_head *wq_head,
				 struct wait_queue_entry *wq_entry, int state);

char *t40_strncpy(char *dst, const char *src, size_t n);
void t40_arch_sync_dma_for_device(phys_addr_t paddr, size_t size, enum dma_data_direction dir);
void t40_dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr, size_t size,
				 enum dma_data_direction dir);
void t40_dma_sync_single_for_device(struct device *dev, dma_addr_t addr, size_t size,
				    enum dma_data_direction dir);
void private_dma_sync_single_for_device(struct device *dev, dma_addr_t addr, size_t size,
					enum dma_data_direction dir);

int VPU_Encoder_SendFrame(int chn, void *frame);
int tx_isp_abi_init(void);
void tx_isp_abi_exit(void);
void *get_driver_common_interfaces(void);

void *t40_blob_dev_of(const struct device *dev);
void *t40_dev_get_drvdata(const struct device *dev);
struct device *t40_any_dev(void);

/* 3.10 jiffies (HZ=100) to this kernel's */
unsigned long t40_jiffies_from_44(unsigned long j);

int t40_seq_printf_44(void *seq_twin, const char *fmt, ...);
void __iomem *t40___ioremap_44(phys_addr_t phys, unsigned long size, unsigned long flags);
void t40___iounmap_44(const volatile void __iomem *addr);
extern void *t40_dma_map_ops;
void t40_kfree_44(const void *p);
bool t40_alloc_untrack(const void *p);
void t40_alloc_free_all(void);
void *t40_alloc_track_ext(void *p);
u32 t40___div64_32(u64 *n, u32 base);
extern unsigned long t40_mips_io_port_base;
void t40_misc_deregister_44(struct miscdevice *misc44);
int t40_sched_setscheduler_44(struct task_struct *p, int policy, const struct sched_param *param);
int t40_single_open_44(struct file *twin, int (*show)(struct seq_file *, void *), void *data);
bool t40_queue_work_44(struct work_struct *work);
bool t40_queue_work_on_44(int cpu, struct workqueue_struct *wq, struct work_struct *work);
extern unsigned long t40_rmem_base, t40_rmem_size;
void *t40_priv_kmalloc(size_t s, gfp_t gfp);
void t40_priv_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
			     enum dma_data_direction direction);
struct net *t40_priv_get_init_net(void);

#endif
