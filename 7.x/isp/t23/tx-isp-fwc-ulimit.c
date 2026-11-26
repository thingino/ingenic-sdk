// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-ulimit.c - the address limit the multi-sensor firmware checks
 *
 * The 4.4.94 builds inline access_ok() as a load of addr_limit at 24($gp),
 * a slot 7.x uses for the syscall number. In the single-sensor firmware the
 * five functions that do it are C (tx-isp-fwc.c, tx-isp-fwc-tun.c). The
 * multi-sensor build has the same five, but compiled from different code
 * (per-sensor tuning state), so here the firmware's own functions run with
 * the slot holding 4.4's USER_DS for the length of the call. The slot is the
 * calling task's own and nothing reads it before the syscall returns.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/thread_info.h>

#include "tx-isp-abi.h"
#include "tx-isp-twin.h"

/* 4.4's USER_DS.seg on 32-bit MIPS: the access_ok() mask of user space */
#define T23_UA_LIMIT_44		0x80000000UL

typedef long (*fw_ioctl_t)(void *twin, unsigned int cmd, unsigned long arg);
typedef ssize_t (*fw_read_t)(void *twin, char __user *buf, size_t count,
			     loff_t *ppos);
typedef ssize_t (*fw_write_t)(void *twin, const char __user *buf, size_t count,
			      loff_t *ppos);

extern char t23fw_tisp_vic_ctrl_ioctl[], t23fw_video_input_cmd_set[];
extern char t23fw_isp_tunning_read[], t23fw_tisp_code_tuning_ioctl[];
extern char t23fw_isp_core_tunning_unlocked_ioctl[];

extern void *isp_vic_frd_fops[], *video_input_cmd_fops[];
extern void *isp_core_tunning_fops[], *tisp_fops[];

enum { F44_READ = 2, F44_WRITE = 3, F44_UNLOCKED_IOCTL = 8 };

static inline long ulimit_enter(void)
{
	long saved = current_thread_info()->syscall;

	current_thread_info()->syscall = T23_UA_LIMIT_44;
	return saved;
}

static inline void ulimit_leave(long saved)
{
	current_thread_info()->syscall = saved;
}

#define UL_IOCTL(fn)							\
static long ul_##fn(void *twin, unsigned int cmd, unsigned long arg)	\
{									\
	long saved = ulimit_enter();					\
	long ret = ((fw_ioctl_t)t23fw_##fn)(twin, cmd, arg);		\
									\
	ulimit_leave(saved);						\
	return ret;							\
}

UL_IOCTL(tisp_vic_ctrl_ioctl)
UL_IOCTL(tisp_code_tuning_ioctl)
UL_IOCTL(isp_core_tunning_unlocked_ioctl)

static ssize_t ul_isp_tunning_read(void *twin, char __user *buf, size_t count,
				   loff_t *ppos)
{
	long saved = ulimit_enter();
	ssize_t ret = ((fw_read_t)t23fw_isp_tunning_read)(twin, buf, count, ppos);

	ulimit_leave(saved);
	return ret;
}

static ssize_t ul_video_input_cmd_set(void *twin, const char __user *buf,
				      size_t count, loff_t *ppos)
{
	long saved = ulimit_enter();
	ssize_t ret = ((fw_write_t)t23fw_video_input_cmd_set)(twin, buf, count, ppos);

	ulimit_leave(saved);
	return ret;
}

struct ul_slot {
	void **table;
	int index;
	const void *fw;
	void *wrap;
	const char *name;
};

#define SLOT(t, i, f)	{ t, i, t23fw_##f, ul_##f, #f }

static const struct ul_slot ul_slots[] = {
	SLOT(isp_vic_frd_fops, F44_UNLOCKED_IOCTL, tisp_vic_ctrl_ioctl),
	SLOT(video_input_cmd_fops, F44_WRITE, video_input_cmd_set),
	SLOT(tisp_fops, F44_READ, isp_tunning_read),
	SLOT(tisp_fops, F44_UNLOCKED_IOCTL, tisp_code_tuning_ioctl),
	SLOT(isp_core_tunning_fops, F44_UNLOCKED_IOCTL, isp_core_tunning_unlocked_ioctl),
};

int t23_fwc_install(void)
{
	int i;

	BUILD_BUG_ON(offsetof(struct thread_info, syscall) != 24);

	for (i = 0; i < ARRAY_SIZE(ul_slots); i++) {
		if (ul_slots[i].table[ul_slots[i].index] != ul_slots[i].fw) {
			pr_err("t23-isp: %s: unexpected firmware table entry\n",
			       ul_slots[i].name);
			return -ENOEXEC;
		}
	}
	for (i = 0; i < ARRAY_SIZE(ul_slots); i++)
		ul_slots[i].table[ul_slots[i].index] = ul_slots[i].wrap;
	return 0;
}
