// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc.c - C replacements for the firmware functions that read kernel
 * objects through 3.10 offsets the compiler inlined
 *
 * Each function here takes over from its firmware twin (renamed t31fw_* by the
 * Kbuild). Kernel-owned objects (file, sock, the current task) are reached
 * through 7.x types; firmware-owned objects keep the firmware's own offsets.
 * The firmware reaches these functions three ways and each is redirected
 * without touching its code: a slot in one of its 3.10 file_operations tables
 * (switched in t31_fwc_install(), before tx_isp_init() registers anything), a
 * global symbol (weakened by the Kbuild, so this strong definition wins), or
 * a pointer the firmware stores at probe time (switched after probe).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <net/sock.h>

#include "tx-isp-abi.h"

#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

/* ---- firmware symbols ------------------------------------------------------ */

extern int isp_printf(int level, const char *fmt, ...);
extern int private_copy_from_user(void *to, const void __user *from, unsigned long n);
extern int private_copy_to_user(void __user *to, const void *from, unsigned long n);
extern void private_mutex_lock(struct mutex *m);
extern void private_mutex_unlock(struct mutex *m);
extern void private_init_completion(struct completion *c);
extern void private_sock_release(struct socket *sock);
extern int isp_free_buffer(u32 addr);

int fwc_apical_isp_core_ops_g_ctrl(void *t, void *ctrl);
extern int apical_isp_core_ops_s_ctrl(void *core, void *ctrl);
extern int __frame_channel_vb2_streamoff(void *chan, int type);
extern int __vb2_queue_free(void *q, int n);
extern int tx_isp_video_link_destroy(void *ispdev) asm("tx_isp_video_link_destroy.isra.5");

extern u64 frame_done_cnt;
extern volatile int frame_done_cond;
extern wait_queue_head_t frame_done_wq;
extern struct sock *nlsk;

int isp_frame_done_wait(long timeout, u64 *cnt);
void tisp_netlink_exit(void);
ssize_t isp_tunning_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);

extern char tispPollValue;

/* the firmware's own versions, for the slot checks */
extern char t31fw_tx_isp_open[], t31fw_tx_isp_release[];
extern char t31fw_frame_channel_open[], t31fw_frame_channel_release[];
extern char t31fw_isp_core_tunning_open[], t31fw_isp_core_tunning_release[];
extern char t31fw_isp_core_tunning_unlocked_ioctl[];
extern char t31fw_tx_isp_unlocked_ioctl[];
extern char t31fw_tisp_code_tuning_ioctl[];

/* 3.10 file_operations tables in the firmware's .data */
extern void *tx_isp_fops[], *fs_channel_ops[], *isp_core_tunning_fops[], *tisp_fops[];
long fwc_tisp_code_tuning_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

enum { F310_UNLOCKED_IOCTL = 8, F310_OPEN = 11, F310_RELEASE = 13 };

/*
 * The firmware returns -ENOIOCTLCMD (-515) from subdev ops it does not
 * implement; callers treat it as success.
 */
#define FW_ENOIOCTLCMD	(-515)

/* ---- /dev/tx-isp ----------------------------------------------------------
 *
 * private_data is the firmware's ispdev miscdevice (t31_misc_open puts it
 * there): +0x2c..+0x6c the 16 subdev pointers, +0x108 the open count,
 * +0x10c the link state. A subdev's ops sit at +0xc4, its internal ops at
 * ops+0x10: activate_module then slake_module.
 */
#define ISPDEV_SUBDEV_FIRST	0x2c
#define ISPDEV_SUBDEV_END	0x6c
#define ISPDEV_OPEN_CNT		0x108
#define ISPDEV_LINK		0x10c

typedef int (*fw_sd_op)(void *sd);

static fw_sd_op fw_sd_internal(void *sd, int which)
{
	void **internal = FW(FW(sd, 0xc4, u8 *), 0x10, void **);

	return internal ? internal[which] : NULL;
}

static int fwc_tx_isp_open(struct inode *inode, struct file *file)
{
	u8 *dev = file->private_data;
	int off, ret = 0;

	if (FW(dev, ISPDEV_OPEN_CNT, int)) {
		FW(dev, ISPDEV_OPEN_CNT, int)++;
		return 0;
	}
	FW(dev, ISPDEV_LINK, int) = -1;
	for (off = ISPDEV_SUBDEV_FIRST; off < ISPDEV_SUBDEV_END; off += 4) {
		void *sd = FW(dev, off, void *);
		fw_sd_op activate;

		if (!sd)
			continue;
		activate = fw_sd_internal(sd, 0);
		ret = activate ? activate(sd) : FW_ENOIOCTLCMD;
		if (ret && ret != FW_ENOIOCTLCMD)
			return ret;
	}
	return ret == FW_ENOIOCTLCMD ? 0 : ret;
}

static int fwc_tx_isp_release(struct inode *inode, struct file *file)
{
	u8 *dev = file->private_data;
	int off, ret = 0;

	if (FW(dev, ISPDEV_OPEN_CNT, int)) {
		FW(dev, ISPDEV_OPEN_CNT, int)--;
		return 0;
	}
	for (off = ISPDEV_SUBDEV_FIRST; off < ISPDEV_SUBDEV_END; off += 4) {
		void *sd = FW(dev, off, void *);
		fw_sd_op slake;

		if (!sd)
			continue;
		slake = fw_sd_internal(sd, 1);
		ret = slake ? slake(sd) : FW_ENOIOCTLCMD;
		if (ret && ret != FW_ENOIOCTLCMD)
			return ret;
	}
	if (FW(dev, ISPDEV_LINK, int) >= 0)
		tx_isp_video_link_destroy(dev - 12);
	return 0;
}

/* ---- /dev/framechanN -------------------------------------------------------
 *
 * private_data is the channel's miscdevice, at the start of the channel:
 * +0x24 the vb2 queue (its type first), +0x20c the buffer count, +0x23c 112
 * bytes of per-open state, +0x2c4 the state mutex, +0x2d0 the state
 * (2 slake, 3 open, 4 streaming), +0x2d4 the frame completion.
 */
#define FCH_VBQ		0x24
#define FCH_NBUF	0x20c
#define FCH_OPEN_STATE	0x23c
#define FCH_INDEX	0x2c0
#define FCH_MUTEX	0x2c4
#define FCH_STATE	0x2d0
#define FCH_DONE	0x2d4
#define FCH_CNT0	0x2e0
#define FCH_CNT1	0x2e4

static int fwc_frame_channel_open(struct inode *inode, struct file *file)
{
	u8 *ch = file->private_data;

	if (IS_ERR_OR_NULL(ch))
		return -EINVAL;
	private_mutex_lock(&FW(ch, FCH_MUTEX, struct mutex));
	if (FW(ch, FCH_STATE, int) < 2) {
		private_mutex_unlock(&FW(ch, FCH_MUTEX, struct mutex));
		isp_printf(2, "Frame channel%d is slake now, Please activate it firstly!\n",
			   FW(ch, FCH_INDEX, int));
		return -1;
	}
	private_mutex_unlock(&FW(ch, FCH_MUTEX, struct mutex));
	memset(ch + FCH_OPEN_STATE, 0, 0x70);
	FW(ch, FCH_CNT0, int) = 0;
	FW(ch, FCH_CNT1, int) = 0;
	private_init_completion(&FW(ch, FCH_DONE, struct completion));
	__vb2_queue_free(ch + FCH_VBQ, FW(ch, FCH_NBUF, int));
	FW(ch, FCH_STATE, int) = 3;
	return 0;
}

static int fwc_frame_channel_release(struct inode *inode, struct file *file)
{
	u8 *ch = file->private_data;

	if (IS_ERR_OR_NULL(ch))
		return -EINVAL;
	if (FW(ch, FCH_STATE, int) == 4) {
		__frame_channel_vb2_streamoff(ch, FW(ch, FCH_VBQ, int));
		__vb2_queue_free(ch + FCH_VBQ, FW(ch, FCH_NBUF, int));
		FW(ch, FCH_STATE, int) = 2;
	}
	return 0;
}

/* ---- /dev/isp-m0 (tuning) --------------------------------------------------
 *
 * private_data -> +200 the core subdev -> +0x1bc the tuning state:
 * +0x40ac buffers allocated, +0x40c4 node state (2 closed, 3 open).
 */
#define TUN_BUFFERS	0x40ac
#define TUN_STATE	0x40c4

#define TUNING_G_CTRL	0xc008561b
#define TUNING_S_CTRL	0xc008561c

static u8 *fwc_tuning(struct file *file)
{
	return FW(FW(file->private_data, 200, u8 *), 0x1bc, u8 *);
}

static int fwc_isp_core_tunning_open(struct inode *inode, struct file *file)
{
	u8 *t = fwc_tuning(file);

	if (FW(t, TUN_STATE, int) != 2)
		return -1;
	frame_done_cnt = 0;
	FW(t, TUN_STATE, int) = 3;
	FW(t, TUN_BUFFERS, int) = 0;
	return 0;
}

static int fwc_isp_core_tunning_release(struct inode *inode, struct file *file)
{
	u8 *t = fwc_tuning(file);

	isp_printf(0, "##### %s %d #####\n", "isp_core_tunning_release", 0xacc);
	if (FW(t, TUN_STATE, int) == 2)
		return 0;
	if (FW(t, TUN_BUFFERS, int))
		isp_free_buffer(FW(t, TUN_BUFFERS, u32));
	FW(t, TUN_STATE, int) = 2;
	return 0;
}

static long fwc_isp_core_tunning_unlocked_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	u8 *t = fwc_tuning(file);
	int buf[3];
	int ret;

	if (FW(t, TUN_STATE, int) != 3)
		return -1;

	if (cmd == TUNING_S_CTRL) {
		if (private_copy_from_user(buf, uarg, 8))
			return -EFAULT;
		ret = apical_isp_core_ops_s_ctrl(t, buf);
	} else if (cmd == TUNING_G_CTRL) {
		if (private_copy_from_user(buf, uarg, 8))
			return -EFAULT;
		ret = fwc_apical_isp_core_ops_g_ctrl(t, buf);
		if (ret && ret != FW_ENOIOCTLCMD)
			return ret;
		if (private_copy_to_user(uarg, buf, 8))
			return -EFAULT;
	} else {
		/* {0 = set / else get, id, value} */
		if (private_copy_from_user(buf, uarg, 12))
			return -EFAULT;
		if (!buf[0]) {
			ret = apical_isp_core_ops_s_ctrl(t, &buf[1]);
		} else {
			ret = fwc_apical_isp_core_ops_g_ctrl(t, &buf[1]);
			if ((!ret || ret == FW_ENOIOCTLCMD) &&
			    private_copy_to_user(uarg, buf, 12))
				ret = -EFAULT;
		}
	}
	return ret == FW_ENOIOCTLCMD ? 0 : ret;
}

/* ---- global entry points (the firmware's copies are weak) ----------------- */

int isp_frame_done_wait(long timeout, u64 *cnt)
{
	long ret;

	frame_done_cond = 0;
	ret = wait_event_interruptible_timeout(frame_done_wq, frame_done_cond == 1, timeout);
	*cnt = frame_done_cnt;
	if (ret == -ERESTARTSYS)
		return ret;
	return ret ? 0 : -ETIMEDOUT;
}

void tisp_netlink_exit(void)
{
	if (nlsk && nlsk->sk_socket)
		private_sock_release(nlsk->sk_socket);
}

/*
 * The tuning event poll: one byte, consumed by the read. The firmware copied
 * count bytes starting at that byte; only the byte itself is real, the rest
 * of the user buffer is zeroed.
 */
ssize_t isp_tunning_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	if ((file->f_flags & O_NONBLOCK) || !tispPollValue)
		return -EAGAIN;
	if (count && (copy_to_user(buf, &tispPollValue, 1) ||
		      clear_user(buf + 1, count - 1)))
		return -EFAULT;
	tispPollValue = 0;
	return count;
}

/* ---- install --------------------------------------------------------------- */

struct fwc_slot {
	void **table;
	int index;
	const void *fw;
	void *c;
	const char *name;
};

#define SLOT(t, i, f)	{ t, i, t31fw_##f, fwc_##f, #f }

static const struct fwc_slot fwc_slots[] = {
	SLOT(tx_isp_fops, F310_OPEN, tx_isp_open),
	SLOT(tx_isp_fops, F310_RELEASE, tx_isp_release),
	SLOT(tx_isp_fops, F310_UNLOCKED_IOCTL, tx_isp_unlocked_ioctl),
	SLOT(tisp_fops, F310_UNLOCKED_IOCTL, tisp_code_tuning_ioctl),
	SLOT(fs_channel_ops, F310_OPEN, frame_channel_open),
	SLOT(fs_channel_ops, F310_RELEASE, frame_channel_release),
	SLOT(isp_core_tunning_fops, F310_OPEN, isp_core_tunning_open),
	SLOT(isp_core_tunning_fops, F310_RELEASE, isp_core_tunning_release),
	SLOT(isp_core_tunning_fops, F310_UNLOCKED_IOCTL, isp_core_tunning_unlocked_ioctl),
};

int t31_fwc_install(void)
{
	int i;

	/* all or nothing: a different firmware build fails here */
	for (i = 0; i < ARRAY_SIZE(fwc_slots); i++) {
		if (fwc_slots[i].table[fwc_slots[i].index] != fwc_slots[i].fw) {
			pr_err("t31-isp: %s: unexpected firmware table entry\n",
			       fwc_slots[i].name);
			return -ENOEXEC;
		}
	}
	for (i = 0; i < ARRAY_SIZE(fwc_slots); i++)
		fwc_slots[i].table[fwc_slots[i].index] = fwc_slots[i].c;
	return t31_fwc_vfs_install();
}
