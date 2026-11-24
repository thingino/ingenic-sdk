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
struct seq_file;
struct inode;
struct class;
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

int t31_fwc_install(void);
int t31_fwc_vfs_install(void);
long fwc_tx_isp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
void t31_cdev_init_310(struct cdev *cdev, const void *fops310);
void t31_cdev_del_310(struct cdev *cdev);

/* the targets of the Kbuild's objcopy redirections */
void *t31_kmem_cache_alloc_310(void *cache, gfp_t gfp);
unsigned long t31_irq_save_310(void);
void t31_irq_restore_310(unsigned long flags);
struct class *t31_class_create_310(struct module *owner, const char *name,
				   struct lock_class_key *key);
void t31_class_destroy_310(struct class *c);
void t31_device_destroy_310(struct class *c, dev_t devt);
void *t31_dev_get_drvdata_310(const struct device *dev);
void t31_kfree_skb_310(struct sk_buff *skb);
void *t31_pde_data_310(const struct inode *inode);
void t31_might_sleep_310(const char *file, int line, int preempt_offset);
void t31_preempt_schedule_310(void);
signed long t31_schedule_timeout_310(signed long timeout);

int tx_isp_abi_init(void);
void tx_isp_abi_exit(void);
void *get_driver_common_interfaces(void);

void t31_fops_from_310(struct file_operations *dst, const void *f310);
void *t31_blob_dev_of(const struct device *dev);
void *t31_dev_get_drvdata(const struct device *dev);
struct device *t31_any_dev(void);

/* 3.10 jiffies (HZ=100) to this kernel's */
unsigned long t31_jiffies_from_310(unsigned long j);

int t31_seq_printf_310(struct seq_file *m, const char *fmt, ...);
void *t31_priv_kmalloc(size_t s, gfp_t gfp);
struct sk_buff *t31_priv_nlmsg_new(size_t payload, gfp_t flags);
struct nlmsghdr *t31_priv_nlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
				    int type, int payload, int flags);
struct sock *t31_priv_netlink_kernel_create(struct net *net, int unit,
					    struct netlink_kernel_cfg *cfg310);
void t31_priv_sock_release(struct socket *sock);
ssize_t t31_priv_vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
ssize_t t31_priv_vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
unsigned long t31_priv_get_fs(void);
void t31_priv_set_fs(unsigned long val);
void t31_priv_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
			     enum dma_data_direction direction);
struct timespec;
void t31_priv_getrawmonotonic(struct timespec *ts);
struct net *t31_priv_get_init_net(void);

#endif
