/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __TX_ISP_ABI_H__
#define __TX_ISP_ABI_H__

#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/time64.h>
#include <linux/dma-direction.h>

struct cdev;
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

/* tx-isp-twin.c: file_operations 27 slots, wrapped; blob is the firmware's miscdevice or NULL */
struct fw_fops {
	struct file_operations fops;
	const void *orig[27];
	void *blob;
};
void fw_fops_init(struct fw_fops *t, const void *f44, void *blob);
struct proc_dir_entry *t23_proc_create_data(const char *name, umode_t mode,
					    struct proc_dir_entry *parent,
					    const void *f44, void *data);
void t23_proc_remove(struct proc_dir_entry *de);
void *t23_proc_data(const struct inode *inode);
int t23_single_open_size(struct file *twin, int (*show)(struct seq_file *, void *),
			 void *data, size_t size);
int t23_single_release(struct inode *inode, struct file *twin);
ssize_t t23_seq_read(struct file *twin, char __user *buf, size_t size, loff_t *ppos);
loff_t t23_seq_lseek(struct file *twin, loff_t offset, int whence);
struct seq_file *t23_seq_real(void *twin);
struct file *t23_filp_open(const char *path, int flags, umode_t mode);
int t23_filp_close(struct file *handle, void *id);
ssize_t t23_vfs_read(struct file *handle, char __user *buf, size_t count, loff_t *pos);
ssize_t t23_vfs_write(struct file *handle, const char __user *buf, size_t count,
		      loff_t *pos);
loff_t t23_vfs_llseek(struct file *handle, loff_t offset, int whence);
int t23_platform_driver_register_44(struct platform_driver *drv, struct module *owner);
void t23_cdev_init_310(struct cdev *cdev, const void *fops310);
void t23_cdev_del_310(struct cdev *cdev);

/* the targets of the Kbuild's objcopy redirections */
void *t23_kmem_cache_alloc_310(void *cache, gfp_t gfp);
unsigned long t23_irq_save_310(void);
void t23_irq_restore_310(unsigned long flags);
struct class *t23_class_create_310(struct module *owner, const char *name,
				   struct lock_class_key *key);
void t23_class_destroy_310(struct class *c);
struct clk *t23_clk_get_44(struct device *dev, const char *id);
int t23_clk_set_parent_44(struct clk *clk, struct clk *parent);
void t23_warn_slowpath_null_44(const char *file, int line);
int t23_dma_alloc_from_coherent_44(struct device *dev, ssize_t size,
				   dma_addr_t *handle, void **ret);
int t23_dma_release_from_coherent_44(struct device *dev, int order, void *vaddr);
void t23_device_destroy_310(struct class *c, dev_t devt);
void *t23_dev_get_drvdata_310(const struct device *dev);
void t23_kfree_skb_310(struct sk_buff *skb);
void *t23_pde_data_310(const struct inode *inode);
void t23_might_sleep_310(const char *file, int line, int preempt_offset);
void t23_preempt_schedule_310(void);
signed long t23_schedule_timeout_310(signed long timeout);

int tx_isp_abi_init(void);
void tx_isp_abi_exit(void);
void *get_driver_common_interfaces(void);

void *t23_blob_dev_of(const struct device *dev);
void *t23_dev_get_drvdata(const struct device *dev);
struct device *t23_any_dev(void);

/* 3.10 jiffies (HZ=100) to this kernel's */
unsigned long t23_jiffies_from_310(unsigned long j);

int t23_seq_printf_310(void *seq_twin, const char *fmt, ...);
void *t23_priv_kmalloc(size_t s, gfp_t gfp);
struct sk_buff *t23_priv_nlmsg_new(size_t payload, gfp_t flags);
struct nlmsghdr *t23_priv_nlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
				    int type, int payload, int flags);
struct sock *t23_priv_netlink_kernel_create(struct net *net, int unit,
					    struct netlink_kernel_cfg *cfg310);
void t23_priv_sock_release(struct socket *sock);
unsigned long t23_priv_get_fs(void);
void t23_priv_set_fs(unsigned long val);
void t23_priv_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
			     enum dma_data_direction direction);
struct timespec;
void t23_priv_getrawmonotonic(struct timespec *ts);
struct net *t23_priv_get_init_net(void);

#endif
