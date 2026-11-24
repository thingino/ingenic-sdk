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

int t21_fwc_install(void);

/* tx-isp-fwc-fch.c */
long fwc_frame_channel_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/* tx-isp-fwc-txioctl.c */
long fwc_tx_isp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/* tx-isp-fwc-vfs.c: the /proc command writers */
ssize_t fwc_isp_vic_cmd_set(struct file *file, const char __user *buf, size_t count,
			    loff_t *ppos);
ssize_t fwc_video_input_cmd_set(struct file *file, const char __user *buf, size_t count,
				loff_t *ppos);
int fwc_pad_install(void);

/* the targets of the Kbuild's objcopy redirections */
unsigned long t21_irq_save_310(void);
void t21_irq_restore_310(unsigned long flags);
void t21_kfree_skb_310(struct sk_buff *skb);
void *t21_pde_data_310(const struct inode *inode);
void t21_preempt_schedule_310(void);
void *t21_kmalloc_310(size_t size, gfp_t gfp);
void t21_dma_cache_sync_310(struct device *dev, void *vaddr, size_t size,
			    enum dma_data_direction dir);
int t21_seq_printf_310(struct seq_file *m, const char *fmt, ...);

int tx_isp_abi_init(void);
void tx_isp_abi_exit(void);
void *get_driver_common_interfaces(void);

void t21_fops_from_310(struct file_operations *dst, const void *f310);
void *t21_blob_dev_of(const struct device *dev);
void *t21_dev_get_drvdata(const struct device *dev);
struct device *t21_any_dev(void);

/* 3.10 jiffies (HZ=100) to this kernel's */
unsigned long t21_jiffies_from_310(unsigned long j);

#endif
