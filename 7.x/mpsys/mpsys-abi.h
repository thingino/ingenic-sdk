/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MPSYS_ABI_H__
#define __MPSYS_ABI_H__

#include <linux/types.h>
#include <linux/fs.h>

#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

/*
 * Layout of the firmware's own objects. The T32/T33 object is the 5.15
 * build, the T40/T41 ones are 4.4.94 builds; they differ in the size of
 * the kernel objects embedded in a process_infos entry and in the
 * file_operations slots.
 */
#define MPSYS_NR_PROC		10
#define MPSYS_PI_TGID		0x00
#define MPSYS_PI_KEY		0x04
#define MPSYS_PI_DONE		0x08	/* struct completion, firmware layout */
#ifdef MPSYS_FW_515
#define MPSYS_PI_FLAG		0x14
#define MPSYS_PI_NODE		0x18
#define MPSYS_PI_SIZE		0x1c
#define MPSYS_FOPS_IOCTL	0x28
#define MPSYS_FOPS_RELEASE	0x40
#else
#define MPSYS_PI_FLAG		0x18
#define MPSYS_PI_NODE		0x1c
#define MPSYS_PI_SIZE		0x20
#define MPSYS_FOPS_IOCTL	0x20
#define MPSYS_FOPS_RELEASE	0x34
#endif

/* miscdevice, both builds: minor, name, fops */
#define MPSYS_MISC_MINOR	0x00
#define MPSYS_MISC_NAME		0x04
#define MPSYS_MISC_FOPS		0x08

/* a message queued between processes; the payload follows the header */
struct mpsys_node {
	struct list_head list;
	s32 tgid;
	s32 key;
	s32 size;
	u8 data[];
};

/* a block handed out of the nmem= region (4.4 builds) */
struct mpsys_nmem {
	struct list_head list;
	s32 size;
	u32 addr;
	s32 free;
	s32 tgid;
};

/* the 16-byte argument of every mpsys_data ioctl */
struct mpsys_msg {
	s32 key;
	void __user *buf;
	s32 size;
	s32 tgid;
};

#define MPSYS_SET_MAIN		0xc0044401
#define MPSYS_REGISTER		0xc0044402
#define MPSYS_GET_DATA		0xc0044403
#define MPSYS_PUSH_DATA		0xc0044404
#define MPSYS_RELEASE_DATA	0xc0044405
#define MPSYS_WAIT_RELEASE	0xc0044406

#define MPSYS_NMEM_ALLOC	0xc0046307
#define MPSYS_SEM_LOCK		0xc0046308
#define MPSYS_SEM_UNLOCK	0xc0046309
#define MPSYS_NMEM_FREE		0xc004630a

/* firmware objects (made global by the Kbuild) */
extern u8 process_infos[];
extern s32 main_process_tgid;
extern struct list_head data_node_list;
extern struct list_head release_node_list;
extern u8 release_node_mutex[], data_node_mutex[], push_data_mutex[];
extern u8 process_info_mutex[], wait_release[];
extern u8 mpsys_data_miscdev[], mpsys_data_fops[];
extern char mpsysfw_mpsys_data_ioctl[], mpsysfw_mpsys_data_release[];
#ifndef MPSYS_FW_515
extern struct list_head nmem_list;
extern s32 remain_size;
extern u32 remain_start;
extern u8 mutex_nmem[], sema_lock[];
extern u8 mpsys_utils_miscdev[], mpsys_utils_fops[];
extern char mpsysfw_mpsys_utils_ioctl[], mpsysfw_mpsys_utils_release[];
#endif

int mpsys_data_init(void);
void mpsys_data_exit(void);
int mpsys_utils_init(void);
void mpsys_utils_exit(void);

/* the firmware's import table (mpsys-abi.c), keyed by firmware addresses */
void private_mutex_lock(void *lock);
void private_mutex_unlock(void *lock);
int private_mutex_lock_interruptible(void *lock);
int private_down_interruptible(void *sem);
void private_up(void *sem);
void private_init_completion(void *x);
void private_complete(void *x);
int private_wait_for_completion_interruptible(void *x);
int private_wait_event_interruptible(void *wq, int (*state)(void));
void private_wake_up(void *wq);
void *private_kzalloc(size_t size, unsigned int gfp);
void private_kfree(void *p);
long private_copy_from_user(void *to, const void __user *from, long size);
long private_copy_to_user(void __user *to, const void *from, long size);
struct file *private_filp_open(const char *filename, int flags, umode_t mode);
int private_filp_close(struct file *filp, fl_owner_t id);
int private_kernel_read(struct file *file, loff_t offset, char *addr,
			unsigned long count);
char *private_strstr(const char *s1, const char *s2);
size_t private_strlen(const char *s);
int private_kstrtoint(const char *s, unsigned int base, int *res);
void private_msleep(unsigned int msecs);
int private_misc_register(void *fw_misc);
void private_misc_deregister(void *fw_misc);

/* targets of the Kbuild's objcopy redirections */
void *mpsys_kmem_cache_alloc_44(void *cache, unsigned int gfp);
extern void *mpsys_kmalloc_caches_44[16];
extern unsigned long mpsys_stack_chk_guard;
void mpsys_stack_chk_fail(void);

int mpsys_abi_init(void);
void mpsys_abi_exit(void);
const struct file_operations *mpsys_fwc_fops(const void *fw_fops);

#endif
