// SPDX-License-Identifier: GPL-2.0
/*
 * The import table of the vendor mpsys object on Linux 7.x
 *
 * The object calls the kernel through private_* functions (weak stubs in the
 * object, the vendor shim supplied the strong ones). The locks, semaphore,
 * wait queue and completions it passes are its own static objects in the
 * layout of the kernel it was built for, so each one is backed here by a
 * native object found by its address. The miscdevices it registers carry
 * file_operations in the old layout; a native table is registered instead.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "mpsys-abi.h"

enum mpsys_shadow_type {
	SH_MUTEX,
	SH_SEM,
	SH_WAITQ,
	SH_COMPLETION,
};

struct mpsys_shadow {
	const void *key;
	enum mpsys_shadow_type type;
	union {
		struct mutex mutex;	/* stands in for a firmware mutex */
		struct semaphore sem;
		wait_queue_head_t wq;
		struct completion done;
	};
};

/* 5 mutexes, the wait queue, a completion per process slot; 4.4: + nmem mutex + semaphore */
#define MPSYS_NR_SHADOWS	(8 + MPSYS_NR_PROC)

static struct mpsys_shadow shadows[MPSYS_NR_SHADOWS];
static int nr_shadows;

static struct mpsys_shadow *shadow_add(const void *key, enum mpsys_shadow_type type)
{
	struct mpsys_shadow *s = &shadows[nr_shadows++];

	s->key = key;
	s->type = type;
	switch (type) {
	case SH_MUTEX:
		mutex_init(&s->mutex);
		break;
	case SH_SEM:
		sema_init(&s->sem, 1);
		break;
	case SH_WAITQ:
		init_waitqueue_head(&s->wq);
		break;
	case SH_COMPLETION:
		init_completion(&s->done);
		break;
	}
	return s;
}

static struct mpsys_shadow *shadow(const void *key, enum mpsys_shadow_type type)
{
	int i;

	for (i = 0; i < nr_shadows; i++)
		if (shadows[i].key == key && shadows[i].type == type)
			return &shadows[i];
	WARN_ONCE(1, "mpsys: unknown firmware object %p (type %d)\n", key, type);
	return NULL;
}

int mpsys_abi_init(void)
{
	int i;

	shadow_add(release_node_mutex, SH_MUTEX);
	shadow_add(data_node_mutex, SH_MUTEX);
	shadow_add(push_data_mutex, SH_MUTEX);
	shadow_add(process_info_mutex, SH_MUTEX);
	shadow_add(wait_release, SH_WAITQ);
#ifndef MPSYS_FW_515
	/* both statically initialised unlocked, the semaphore with count 1 */
	shadow_add(mutex_nmem, SH_MUTEX);
	shadow_add(sema_lock, SH_SEM);
#endif
	for (i = 0; i < MPSYS_NR_PROC; i++)
		shadow_add(process_infos + i * MPSYS_PI_SIZE + MPSYS_PI_DONE, SH_COMPLETION);
	return 0;
}

void mpsys_abi_exit(void)
{
	nr_shadows = 0;
}

/* ---- locks and waits ------------------------------------------------------- */

void private_mutex_lock(void *lock)
{
	struct mpsys_shadow *s = shadow(lock, SH_MUTEX);

	if (s)
		mutex_lock(&s->mutex);
}

void private_mutex_unlock(void *lock)
{
	struct mpsys_shadow *s = shadow(lock, SH_MUTEX);

	if (s)
		mutex_unlock(&s->mutex);
}

int private_mutex_lock_interruptible(void *lock)
{
	struct mpsys_shadow *s = shadow(lock, SH_MUTEX);

	return s ? mutex_lock_interruptible(&s->mutex) : -EINVAL;
}

int private_down_interruptible(void *sem)
{
	struct mpsys_shadow *s = shadow(sem, SH_SEM);

	return s ? down_interruptible(&s->sem) : -EINVAL;
}

void private_up(void *sem)
{
	struct mpsys_shadow *s = shadow(sem, SH_SEM);

	if (s)
		up(&s->sem);
}

void private_init_completion(void *x)
{
	struct mpsys_shadow *s = shadow(x, SH_COMPLETION);

	if (s)
		init_completion(&s->done);
}

void private_complete(void *x)
{
	struct mpsys_shadow *s = shadow(x, SH_COMPLETION);

	if (s)
		complete(&s->done);
}

int private_wait_for_completion_interruptible(void *x)
{
	struct mpsys_shadow *s = shadow(x, SH_COMPLETION);

	return s ? wait_for_completion_interruptible(&s->done) : -EINVAL;
}

int private_wait_event_interruptible(void *wq, int (*state)(void))
{
	struct mpsys_shadow *s = shadow(wq, SH_WAITQ);

	return s ? wait_event_interruptible(s->wq, state()) : -EINVAL;
}

void private_wake_up(void *wq)
{
	struct mpsys_shadow *s = shadow(wq, SH_WAITQ);

	if (s)
		wake_up(&s->wq);
}

/* ---- memory, strings, files ------------------------------------------------ */

/* the firmware passes its own kernel's GFP_KERNEL value; every caller may sleep */
void *private_kzalloc(size_t size, unsigned int gfp)
{
	return kzalloc(size, GFP_KERNEL);
}

void private_kfree(void *p)
{
	kfree(p);
}

long private_copy_from_user(void *to, const void __user *from, long size)
{
	return copy_from_user(to, from, size);
}

long private_copy_to_user(void __user *to, const void *from, long size)
{
	return copy_to_user(to, from, size);
}

struct file *private_filp_open(const char *filename, int flags, umode_t mode)
{
	return filp_open(filename, flags, mode);
}

int private_filp_close(struct file *filp, fl_owner_t id)
{
	return filp_close(filp, id);
}

#ifndef MPSYS_FW_515
/*
 * 4.4.94 kernel_read(). The only caller reads /proc/cmdline into a zeroed
 * buffer of count bytes and then scans the nmem= value up to the next
 * space: keep the buffer terminated and end the text with a space.
 */
int private_kernel_read(struct file *file, loff_t offset, char *addr,
			unsigned long count)
{
	loff_t pos = offset;
	ssize_t n;

	if (count < 2)
		return -EINVAL;
	n = kernel_read(file, addr, count - 2, &pos);
	if (n <= 0)
		return n;
	if (addr[n - 1] == '\n')
		addr[n - 1] = ' ';
	else
		addr[n++] = ' ';
	addr[n] = '\0';
	return n;
}
#endif

char *private_strstr(const char *s1, const char *s2)
{
	return strstr(s1, s2);
}

size_t private_strlen(const char *s)
{
	return strlen(s);
}

int private_kstrtoint(const char *s, unsigned int base, int *res)
{
	return kstrtoint(s, base, res);
}

void private_msleep(unsigned int msecs)
{
	msleep(msecs);
}

/* ---- miscdevices ------------------------------------------------------------ */

struct mpsys_misc {
	const void *fw;
	struct miscdevice misc;
};

static struct mpsys_misc miscs[2];

int private_misc_register(void *fw_misc)
{
	const struct file_operations *fops;
	struct mpsys_misc *m = NULL;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(miscs); i++)
		if (!miscs[i].fw) {
			m = &miscs[i];
			break;
		}
	if (!m)
		return -EBUSY;

	fops = mpsys_fwc_fops(FW(fw_misc, MPSYS_MISC_FOPS, void *));
	if (!fops)
		return -EINVAL;

	m->misc.minor = FW(fw_misc, MPSYS_MISC_MINOR, int);
	m->misc.name = FW(fw_misc, MPSYS_MISC_NAME, const char *);
	m->misc.fops = fops;
	ret = misc_register(&m->misc);
	if (ret)
		return ret;
	m->fw = fw_misc;
	return 0;
}

void private_misc_deregister(void *fw_misc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(miscs); i++)
		if (miscs[i].fw == fw_misc) {
			misc_deregister(&miscs[i].misc);
			memset(&miscs[i], 0, sizeof(miscs[i]));
		}
}

/* ---- targets of the Kbuild's objcopy redirections ----------------------- */

/* only the replaced NMEM ioctl allocated through these */
void *mpsys_kmem_cache_alloc_44(void *cache, unsigned int gfp)
{
	WARN_ONCE(1, "mpsys: replaced firmware path reached\n");
	return NULL;
}

void *mpsys_kmalloc_caches_44[16];

/* the replaced data ioctl was the only user of the stack protector */
unsigned long mpsys_stack_chk_guard;

void mpsys_stack_chk_fail(void)
{
	panic("mpsys: firmware stack corrupted\n");
}
