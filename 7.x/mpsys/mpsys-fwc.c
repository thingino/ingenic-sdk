// SPDX-License-Identifier: GPL-2.0
/*
 * C versions of the firmware functions that read the current
 * task (its tgid) and the struct file by the offsets of the kernel the
 * object was built for: the mpsys_data ioctl and release and, on the 4.4.94
 * builds, the mpsys_utils ioctl and release and nmem_dealloc().
 *
 * They keep the firmware's own data (process_infos, the node lists,
 * main_process_tgid, the nmem bookkeeping) and take its locks through the
 * same private_* imports the firmware uses. Unlike the firmware they drop
 * the locks on their error paths, recheck the release list after waking
 * and copy no more than a queued message holds.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include "mpsys-abi.h"

#define PI(i)		(process_infos + (i) * MPSYS_PI_SIZE)
#define PI_TGID(i)	FW(PI(i), MPSYS_PI_TGID, s32)
#define PI_KEY(i)	FW(PI(i), MPSYS_PI_KEY, s32)
#define PI_DONE(i)	(PI(i) + MPSYS_PI_DONE)
#define PI_FLAG(i)	FW(PI(i), MPSYS_PI_FLAG, s32)
#define PI_NODE(i)	FW(PI(i), MPSYS_PI_NODE, struct mpsys_node *)

/* a process slot whose consumer has not asked for a key yet */
#define MPSYS_KEY_NONE	(-1)

static_assert(sizeof(struct mpsys_node) == 0x14);
static_assert(sizeof(struct mpsys_msg) == 0x10);
static_assert(offsetof(struct mpsys_nmem, tgid) == 0x14);

static s32 cur_tgid(void)
{
	return current->tgid;
}

static void pi_claim(int i, s32 key)
{
	PI_KEY(i) = key;
	PI_FLAG(i) = 0;
	PI_TGID(i) = cur_tgid();
	private_init_completion(PI_DONE(i));
}

/* release_node_mutex held: queue a consumed message for the main process */
static void release_node_add(struct mpsys_node *node)
{
	bool was_empty = list_empty(&release_node_list);

	list_add(&node->list, &release_node_list);
	if (was_empty)
		private_wake_up(wait_release);
}

static int release_state(void)
{
	return !list_empty_careful(&release_node_list);
}

static long data_register(void)
{
	int i;

	private_mutex_lock(process_info_mutex);
	for (i = 0; i < MPSYS_NR_PROC; i++)
		if (!PI_TGID(i)) {
			pi_claim(i, MPSYS_KEY_NONE);
			private_mutex_unlock(process_info_mutex);
			return 0;
		}
	private_mutex_unlock(process_info_mutex);
	pr_err("mpsys: set process info failed\n");
	return -EINVAL;
}

/* a consumer waits for the next message of a key */
static long data_get(struct mpsys_msg __user *umsg)
{
	struct mpsys_msg msg;
	struct mpsys_node *node;
	s32 tgid = cur_tgid();
	long ret;
	int i;

	if (copy_from_user(&msg, umsg, sizeof(msg)))
		return -EFAULT;

	private_mutex_lock(process_info_mutex);
	for (i = 0; i < MPSYS_NR_PROC; i++)
		if (PI_TGID(i) == tgid && PI_KEY(i) == msg.key)
			goto found;
	for (i = 0; i < MPSYS_NR_PROC; i++)
		if (PI_TGID(i) == tgid && PI_KEY(i) == MPSYS_KEY_NONE) {
			PI_KEY(i) = msg.key;
			goto found;
		}
	for (i = 0; i < MPSYS_NR_PROC; i++)
		if (!PI_TGID(i)) {
			pi_claim(i, msg.key);
			goto found;
		}
	private_mutex_unlock(process_info_mutex);
	pr_err("mpsys: MPSYS_GET_DATA: no free process slot\n");
	return -EINVAL;

found:
	PI_FLAG(i) = 1;
	private_mutex_unlock(process_info_mutex);

	ret = private_wait_for_completion_interruptible(PI_DONE(i));
	if (ret)
		return -ERESTARTSYS;

	msg.tgid = main_process_tgid;
	if (copy_to_user(umsg, &msg, sizeof(msg)))
		return -EFAULT;

	private_mutex_lock(process_info_mutex);
	node = PI_NODE(i);
	if (!node || msg.size < 0)
		ret = -EINVAL;
	else if (copy_to_user(msg.buf, node->data, min(msg.size, node->size)))
		ret = -EFAULT;
	private_mutex_unlock(process_info_mutex);
	return ret;
}

/* the producer hands a message to every consumer waiting on its key */
static long data_push(struct mpsys_msg __user *umsg)
{
	struct mpsys_msg msg;
	struct mpsys_node *node;
	long n = 0;
	int i;

	if (private_mutex_lock_interruptible(push_data_mutex))
		return -ERESTARTSYS;
	if (copy_from_user(&msg, umsg, sizeof(msg))) {
		n = -EFAULT;
		goto out;
	}
	if (msg.size < 0) {
		n = -EINVAL;
		goto out;
	}

	for (i = 0; i < MPSYS_NR_PROC; i++) {
		if (!PI_TGID(i) || PI_KEY(i) != msg.key || PI_FLAG(i) != 1)
			continue;

		node = kzalloc(sizeof(*node) + msg.size, GFP_KERNEL);
		if (!node) {
			n = -ENOMEM;
			goto out;
		}
		node->tgid = PI_TGID(i);
		node->key = msg.key;
		node->size = msg.size;
		if (copy_from_user(node->data, msg.buf, msg.size)) {
			kfree(node);
			n = -EFAULT;
			goto out;
		}
		if (private_mutex_lock_interruptible(data_node_mutex)) {
			kfree(node);
			n = -ERESTARTSYS;
			goto out;
		}
		list_add_tail(&node->list, &data_node_list);
		private_mutex_unlock(data_node_mutex);

		PI_NODE(i) = node;
		PI_FLAG(i) = 0;
		private_complete(PI_DONE(i));
		n++;
	}
out:
	private_mutex_unlock(push_data_mutex);
	return n;
}

/* a consumer is done with a message: it goes back to the main process */
static long data_release_msg(struct mpsys_msg __user *umsg)
{
	struct mpsys_msg msg;
	struct mpsys_node *node, *found = NULL;
	s32 tgid = cur_tgid();

	if (copy_from_user(&msg, umsg, sizeof(msg)))
		return -EFAULT;
	if (msg.tgid != main_process_tgid)
		return 0;

	private_mutex_lock(data_node_mutex);
	list_for_each_entry(node, &data_node_list, list)
		if (node->tgid == tgid && node->key == msg.key &&
		    node->size == msg.size && msg.size >= 0) {
			found = node;
			break;
		}
	if (!found) {
		private_mutex_unlock(data_node_mutex);
		pr_err("mpsys: release node not found\n");
		return 0;
	}
	list_del(&found->list);
	private_mutex_unlock(data_node_mutex);

	private_mutex_lock(release_node_mutex);
	release_node_add(found);
	private_mutex_unlock(release_node_mutex);
	return 0;
}

/* the main process takes back the next released message */
static long data_wait_release(struct mpsys_msg __user *umsg)
{
	struct mpsys_msg msg;
	struct mpsys_node *node;
	long ret = 0;

	if (copy_from_user(&msg, umsg, sizeof(msg)))
		return -EFAULT;

	for (;;) {
		if (private_wait_event_interruptible(wait_release, release_state))
			return -1;
		private_mutex_lock(release_node_mutex);
		if (!list_empty(&release_node_list))
			break;
		private_mutex_unlock(release_node_mutex);
	}
	node = list_first_entry(&release_node_list, struct mpsys_node, list);
	if (msg.size < 0)
		ret = -EINVAL;
	else if (copy_to_user(msg.buf, node->data, min(msg.size, node->size)))
		ret = -EFAULT;
	list_del(&node->list);
	kfree(node);
	private_mutex_unlock(release_node_mutex);
	return ret;
}

static bool trace;
module_param(trace, bool, 0644);
MODULE_PARM_DESC(trace, "log every mpsys_data and nna_lock ioctl");

static long mpsys_fwc_data_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	void __user *uarg = (void __user *)arg;

	if (trace) {
		struct mpsys_msg msg = { };

		if (cmd != MPSYS_SET_MAIN && cmd != MPSYS_REGISTER &&
		    copy_from_user(&msg, uarg, sizeof(msg)))
			msg.key = -1;
		pr_info("mpsys: %d/%d data ioctl %u key %d size %d tgid %d main %d\n",
			current->tgid, current->pid, _IOC_NR(cmd), msg.key,
			msg.size, msg.tgid, main_process_tgid);
	}

	switch (cmd) {
	case MPSYS_SET_MAIN:
		main_process_tgid = cur_tgid();
		return 0;
	case MPSYS_REGISTER:
		return data_register();
	case MPSYS_GET_DATA:
		return data_get(uarg);
	case MPSYS_PUSH_DATA:
		return data_push(uarg);
	case MPSYS_RELEASE_DATA:
		return data_release_msg(uarg);
	case MPSYS_WAIT_RELEASE:
		return data_wait_release(uarg);
	}
	return -EINVAL;
}

static void node_list_free(struct list_head *head, void *lock)
{
	struct mpsys_node *node, *tmp;

	private_mutex_lock(lock);
	list_for_each_entry_safe(node, tmp, head, list) {
		list_del(&node->list);
		kfree(node);
	}
	private_mutex_unlock(lock);
}

/*
 * Last close of a process: a consumer's slots are cleared and its unread
 * messages go back to the main process; the main process frees them all.
 */
static int mpsys_fwc_data_release(struct inode *inode, struct file *file)
{
	struct mpsys_node *node, *tmp;
	s32 tgid = cur_tgid();
	int i, n = 0;

	private_mutex_lock(process_info_mutex);
	for (i = 0; i < MPSYS_NR_PROC; i++) {
		if (PI_TGID(i) != tgid)
			continue;
		memset(PI(i), 0, MPSYS_PI_SIZE);
		private_mutex_lock(data_node_mutex);
		list_for_each_entry_safe(node, tmp, &data_node_list, list) {
			if (node->tgid != tgid)
				continue;
			list_del(&node->list);
			private_mutex_lock(release_node_mutex);
			release_node_add(node);
			private_mutex_unlock(release_node_mutex);
		}
		private_mutex_unlock(data_node_mutex);
		n++;
	}
	private_mutex_unlock(process_info_mutex);
	if (n)
		return 0;

	if (tgid != main_process_tgid)
		return 0;
	main_process_tgid = 0;
	node_list_free(&data_node_list, data_node_mutex);
	node_list_free(&release_node_list, release_node_mutex);
	return 0;
}

static const struct file_operations mpsys_data_fops_7x = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= mpsys_fwc_data_ioctl,
	.release	= mpsys_fwc_data_release,
};

#ifndef MPSYS_FW_515
/* mutex_nmem held: the caller's block (one) goes back to the free list */
static void nmem_dealloc_cur(void)
{
	struct mpsys_nmem *b;
#ifdef MPSYS_NMEM_MERGE
	struct mpsys_nmem *tmp;
#endif
	s32 tgid = cur_tgid();

	list_for_each_entry(b, &nmem_list, list)
		if (b->tgid == tgid && !b->free) {
			b->free = 1;
			b->tgid = 0;
			break;
		}

#ifdef MPSYS_NMEM_MERGE
	/* T41: free blocks next to the unallocated tail join it again */
	list_for_each_entry_safe(b, tmp, &nmem_list, list) {
		if (!b->free)
			continue;
		if (b->addr + b->size == remain_start) {
			remain_start = b->addr;
			remain_size += b->size;
		} else if (b->addr == remain_start + remain_size) {
			remain_size += b->size;
		} else {
			continue;
		}
		list_del(&b->list);
		kfree(b);
	}
#endif
}

static long utils_nmem_alloc(s32 __user *uarg)
{
	struct mpsys_nmem *b;
	s32 size;
	u32 addr = 0;
	long ret;

	if (get_user(size, uarg))
		return -EFAULT;

	private_mutex_lock(mutex_nmem);
	pr_info("mpsys: NMEM_INIT size: 0x%x\n", size);
	list_for_each_entry(b, &nmem_list, list)
		if (b->free && size <= b->size) {
			b->free = 0;
			b->tgid = cur_tgid();
			addr = b->addr;
			break;
		}
	if (!addr) {
		if (size <= 0 || remain_size < size) {
			pr_err("mpsys: nmem is not enough (0x%x left, 0x%x asked)\n",
			       remain_size, size);
		} else {
			b = kmalloc_obj(*b, GFP_KERNEL);
			if (!b) {
				private_mutex_unlock(mutex_nmem);
				return -ENOMEM;
			}
			addr = remain_start;
			remain_size -= size;
			remain_start += size;
			b->addr = addr;
			b->size = size;
			b->free = 0;
			b->tgid = cur_tgid();
			list_add_tail(&b->list, &nmem_list);
		}
	}
	ret = put_user(addr, (u32 __user *)(uarg + 1)) ? -EFAULT : (long)addr;
	private_mutex_unlock(mutex_nmem);
	return ret;
}

/* file->private_data: the tgid that holds the global semaphore through this file */
static long mpsys_fwc_utils_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	int ret;

	if (trace)
		pr_info("mpsys: %d/%d nna_lock ioctl %u\n", current->tgid,
			current->pid, _IOC_NR(cmd));

	switch (cmd) {
	case MPSYS_NMEM_ALLOC:
		return utils_nmem_alloc((s32 __user *)arg);
	case MPSYS_SEM_LOCK:
		ret = private_down_interruptible(sema_lock);
		if (ret)
			return ret;
		file->private_data = (void *)(long)cur_tgid();
		return 0;
	case MPSYS_SEM_UNLOCK:
		file->private_data = NULL;
		private_up(sema_lock);
		return 0;
	case MPSYS_NMEM_FREE:
		private_mutex_lock(mutex_nmem);
		nmem_dealloc_cur();
		private_mutex_unlock(mutex_nmem);
		return 0;
	}
	return 0;
}

static int mpsys_fwc_utils_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int mpsys_fwc_utils_release(struct inode *inode, struct file *file)
{
	if (file->private_data == (void *)(long)cur_tgid())
		private_up(sema_lock);
	private_mutex_lock(mutex_nmem);
	nmem_dealloc_cur();
	private_mutex_unlock(mutex_nmem);
	/* as the firmware: give a restarting process's peers time to let go */
	msleep(500);
	return 0;
}

static const struct file_operations mpsys_utils_fops_7x = {
	.owner		= THIS_MODULE,
	.open		= mpsys_fwc_utils_open,
	.unlocked_ioctl	= mpsys_fwc_utils_ioctl,
	.release	= mpsys_fwc_utils_release,
};
#endif

/*
 * The native table for one of the firmware's file_operations, after
 * checking that its slots hold the firmware functions replaced here.
 */
const struct file_operations *mpsys_fwc_fops(const void *fw_fops)
{
	const struct file_operations *fops = NULL;
	const void *ioctl = NULL, *release = NULL;

	if (fw_fops == mpsys_data_fops) {
		fops = &mpsys_data_fops_7x;
		ioctl = mpsysfw_mpsys_data_ioctl;
		release = mpsysfw_mpsys_data_release;
	}
#ifndef MPSYS_FW_515
	if (fw_fops == mpsys_utils_fops) {
		fops = &mpsys_utils_fops_7x;
		ioctl = mpsysfw_mpsys_utils_ioctl;
		release = mpsysfw_mpsys_utils_release;
	}
#endif
	if (!fops || FW(fw_fops, MPSYS_FOPS_IOCTL, const void *) != ioctl ||
	    FW(fw_fops, MPSYS_FOPS_RELEASE, const void *) != release) {
		pr_err("mpsys: firmware file_operations %p not recognised\n", fw_fops);
		return NULL;
	}
	return fops;
}
