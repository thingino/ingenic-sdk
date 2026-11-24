// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc.c - C for the firmware code that reads kernel objects through
 * the 3.10 offsets the compiler inlined
 *
 * Kernel-owned objects the firmware reads by offset: struct file
 * (private_data at 112, in every fops handler; f_pos of the files it
 * opens), struct sock (sk_socket at 304), struct inode / dentry (i_size,
 * f_pos, via an inlined vfs_read of the IQ file) and the current
 * thread_info through $gp (addr_limit, preempt_count, flags). Nothing in
 * the firmware is patched: each function that touches one is C instead, and
 * the firmware's own copy is renamed t21fw_* by the Kbuild so the slot
 * checks can still name it.
 *
 * Here: the small file handlers (open and release of the three nodes, the
 * tuning ioctl), tisp_netlink_exit, tiziano_load_parameters and the streamon
 * case of the core's pad handler. tx-isp-fwc-txioctl.c, tx-isp-fwc-fch.c
 * and tx-isp-fwc-vfs.c hold the two ioctl dispatchers and the two /proc
 * command writers.
 *
 * The firmware reaches these entry points through the slots of its own
 * file_operations tables (switched in t21_fwc_install() before tx_isp_init()
 * registers anything, each slot checked against the firmware's address), the
 * weakened global tisp_netlink_exit and tiziano_load_parameters, and the
 * pad handler pointers the core's probe stores (switched after probe).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
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
extern void __private_spin_lock_irqsave(void *lock, unsigned long *flags);
extern void private_spin_unlock_irqrestore(void *lock, unsigned long flags);
extern int isp_free_buffer(u32 addr);
extern int tisp_channel_start(int ch);

extern int apical_isp_core_ops_s_ctrl(void *tuning, void *ctrl);
extern int apical_isp_core_ops_g_ctrl(void *tuning, void *ctrl);
extern int __frame_channel_vb2_streamoff(void *chan, int type);
extern int __vb2_queue_free(void *q, int n);
extern int tx_isp_video_link_destroy(void *ispdev) asm("tx_isp_video_link_destroy.isra.1");

extern struct sock *nlsk;
extern void *ispcore_sd;
extern char sensor_info[];
extern u8 tparams_day[], tparams_night[];

void tisp_netlink_exit(void);
int tiziano_load_parameters(void);

/* the firmware's own versions, for the slot checks and the pad handler's forwarding */
extern char t21fw_tx_isp_open[], t21fw_tx_isp_release[];
extern char t21fw_frame_channel_open[], t21fw_frame_channel_release[];
extern char t21fw_isp_core_tunning_open[], t21fw_isp_core_tunning_release[];
extern char t21fw_isp_core_tunning_unlocked_ioctl[];
extern char t21fw_tx_isp_unlocked_ioctl[], t21fw_frame_channel_unlocked_ioctl[];
extern char t21fw_isp_vic_cmd_set[], t21fw_video_input_cmd_set[];
extern char t21fw_ispcore_pad_event_handle[];

/* 3.10 file_operations tables in the firmware's .data */
extern void *tx_isp_fops[], *fs_channel_ops[], *isp_core_tunning_fops[];
extern void *isp_vic_frd_fops[], *video_input_cmd_fops[];

enum { F310_WRITE = 3, F310_UNLOCKED_IOCTL = 8, F310_OPEN = 11, F310_RELEASE = 13 };

/*
 * The firmware returns -ENOIOCTLCMD (-515) from subdev ops it does not
 * implement; callers treat it as success.
 */
#define FW_ENOIOCTLCMD	(-515)

/* 3.10's GFP_KERNEL is not needed: the buffers below are native */

/* ---- /dev/tx-isp ----------------------------------------------------------
 *
 * private_data is the firmware's ispdev miscdevice (t21_misc_open puts it
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
 * +0x24 the vb2 queue (its type first), +0x20c the buffer count, +0x234 76
 * bytes of per-open state, +0x294 the channel index, +0x298 the state mutex,
 * +0x2a4 the state (2 slake, 3 open, 4 streaming), +0x2a8 the frame
 * completion, +0x2b4 and +0x2b8 two counters.
 */
#define FCH_VBQ		0x24
#define FCH_NBUF	0x20c
#define FCH_OPEN_STATE	0x234
#define FCH_OPEN_SIZE	0x4c
#define FCH_INDEX	0x294
#define FCH_MUTEX	0x298
#define FCH_STATE	0x2a4
#define FCH_DONE	0x2a8
#define FCH_CNT0	0x2b4
#define FCH_CNT1	0x2b8

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
	memset(ch + FCH_OPEN_STATE, 0, FCH_OPEN_SIZE);
	private_init_completion(&FW(ch, FCH_DONE, struct completion));
	FW(ch, FCH_CNT0, int) = 0;
	FW(ch, FCH_CNT1, int) = 0;
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
 * private_data -> +200 the core subdev -> +0x19c the tuning state:
 * +0x40ac buffers allocated, +0x40c4 node state (2 closed, 3 open).
 */
#define TUN_BUFFERS	0x40ac
#define TUN_STATE	0x40c4

#define TUNING_G_CTRL	0xc008561b
#define TUNING_S_CTRL	0xc008561c

static u8 *fwc_tuning(struct file *file)
{
	return FW(FW(file->private_data, 200, u8 *), 0x19c, u8 *);
}

static int fwc_isp_core_tunning_open(struct inode *inode, struct file *file)
{
	u8 *t = fwc_tuning(file);

	if (FW(t, TUN_STATE, int) != 2)
		return -1;
	FW(t, TUN_BUFFERS, int) = 0;
	FW(t, TUN_STATE, int) = 3;
	return 0;
}

static int fwc_isp_core_tunning_release(struct inode *inode, struct file *file)
{
	u8 *t = fwc_tuning(file);

	isp_printf(0, "##### %s %d #####\n", "isp_core_tunning_release", 2876);
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
		ret = apical_isp_core_ops_g_ctrl(t, buf);
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
			ret = apical_isp_core_ops_g_ctrl(t, &buf[1]);
			if ((!ret || ret == FW_ENOIOCTLCMD) &&
			    private_copy_to_user(uarg, buf, 12))
				ret = -EFAULT;
		}
	}
	return ret == FW_ENOIOCTLCMD ? 0 : ret;
}

/* ---- tisp_netlink_exit (global, weakened) ---------------------------------- */

void tisp_netlink_exit(void)
{
	if (nlsk && nlsk->sk_socket)
		private_sock_release(nlsk->sk_socket);
}

/* ---- ispcore pad events -----------------------------------------------------
 *
 * The core's pad handler runs the event through a jump table; its streamon
 * case unlocks the channel with an inlined spin_unlock_irqrestore() whose
 * preempt_enable() works on the current thread_info through $gp. That case
 * is C here; every other event goes to the firmware's own handler, whose
 * code touches no kernel object.
 *
 * Pad (36 bytes, array at core+0xcc, count at core+0x150): +4 the channel
 * index, +5 non-zero when wired, +7 the pad state, +0x1c the handler, +0x20
 * the channel. Channel (160 bytes): +0x50 its state, +0x78 its lock.
 */
#define PAD_INDEX	0x04
#define PAD_WIRED	0x05
#define PAD_STATE	0x07
#define PAD_EVENT	0x1c
#define PAD_CHAN	0x20
#define PAD_SIZE	0x24
#define CHAN_STATE	0x50
#define CHAN_LOCK	0x78
#define CORE_PADS	0xcc
#define CORE_NPADS	0x150
#define EV_STREAMON	0x3000003

typedef int (*fw_pad_fn)(void *pad, unsigned int ev, void *arg);

static int fwc_ispcore_pad_event_handle(void *pad, unsigned int ev, void *arg)
{
	u8 *p = pad;
	u8 *chan;
	unsigned long flags = 0;

	if (ev != EV_STREAMON || !FW(p, PAD_WIRED, u8))
		return ((fw_pad_fn)t21fw_ispcore_pad_event_handle)(pad, ev, arg);

	if (FW(p, PAD_STATE, u8) != 3)
		return 0;
	chan = FW(p, PAD_CHAN, u8 *);
	__private_spin_lock_irqsave(chan + CHAN_LOCK, &flags);
	if (FW(chan, CHAN_STATE, int) != 4) {
		tisp_channel_start(FW(p, PAD_INDEX, u8));
		FW(chan, CHAN_STATE, int) = 4;
		FW(p, PAD_STATE, u8) = 4;
	}
	private_spin_unlock_irqrestore(chan + CHAN_LOCK, flags);
	return 0;
}

/*
 * tx_isp_core_probe() stores the handler into pad +0x1c of every wired pad
 * of the core subdev, then publishes the core in ispcore_sd. Call this after
 * the probe has succeeded; it replaces those pointers, all or nothing.
 */
int fwc_pad_install(void)
{
	u8 *core = ispcore_sd;
	u8 *pads;
	u32 i, n;

	if (IS_ERR_OR_NULL(core))
		return -ENODEV;
	pads = FW(core, CORE_PADS, u8 *);
	n = FW(core, CORE_NPADS, u32);
	if (IS_ERR_OR_NULL(pads))
		return -ENODEV;

	for (i = 0; i < n; i++) {
		u8 *pad = pads + i * PAD_SIZE;

		if (FW(pad, PAD_WIRED, u8) &&
		    FW(pad, PAD_EVENT, void *) != (void *)t21fw_ispcore_pad_event_handle) {
			pr_err("t21-isp: ispcore_pad_event_handle: unexpected pad %u handler\n", i);
			return -ENOEXEC;
		}
	}
	for (i = 0; i < n; i++) {
		u8 *pad = pads + i * PAD_SIZE;

		if (FW(pad, PAD_WIRED, u8))
			FW(pad, PAD_EVENT, void *) = fwc_ispcore_pad_event_handle;
	}
	return 0;
}

/* ---- the IQ file ------------------------------------------------------------
 *
 * tiziano_load_parameters() reads /etc/sensor/<sensor>-t21.bin into a buffer
 * under set_fs(KERNEL_DS), taking the size from the inode and the file
 * position from the struct file by their 3.10 offsets, and through the
 * $gp-relative addr_limit. The file: 8 bytes version ("1.03"), 8 bytes
 * "header0", u32 size (+16), u32 checksum (+20), then the day and the night
 * parameter blocks, 0xed40 bytes each, at +24.
 */
#define TP_SENSOR_NAME	12
#define TP_BLOCK	0xed40
#define TP_DST_OFF	26176
#define TP_HDR_SIZE	24

static const u32 tp_crc_table[8] = {
	0x0, 0x77073096, 0xee0e612c, 0x990951ba,
	0x76dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
};

static u32 tp_crc(const u32 *w, u32 n)
{
	u32 crc = 0;

	while (n--) {
		crc ^= *w++;
		crc ^= tp_crc_table[crc & 7];
	}
	return crc;
}

int tiziano_load_parameters(void)
{
	char path[64];
	struct file *filp;
	loff_t size, pos = 0;
	u8 *buf;
	u32 crc, want;
	ssize_t got;

	snprintf(path, sizeof(path), "/etc/sensor/%s-t21.bin", sensor_info + TP_SENSOR_NAME);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		isp_printf(2, "ISP: open %s file for isp calibrate read failed\n", path);
		return -1;
	}
	size = i_size_read(file_inode(filp));
	if (size < TP_HDR_SIZE + 2 * TP_BLOCK || size > S32_MAX) {
		isp_printf(2, "The size of file has been modify!\n");
		filp_close(filp, NULL);
		return -1;
	}
	buf = kvmalloc(size, GFP_KERNEL);
	if (!buf) {
		isp_printf(2, "%s[%d]: Failed to alloc %lld KB buffer!\n",
			   "tiziano_load_parameters", 696, size >> 10);
		filp_close(filp, NULL);
		return -1;
	}
	got = kernel_read(filp, buf, size, &pos);
	filp_close(filp, NULL);
	if (got != size) {
		isp_printf(2, "The size of file has been modify!\n");
		goto fail;
	}

	if (memcmp(buf, "1.03", 5)) {
		isp_printf(2, "####################################################################\n");
		isp_printf(2, "#### The version of bin doesn't match with driver! ####\n");
		isp_printf(2, "#### The version of bin is %s, the driver is %s ####\n",
			   "1.03", buf);
		isp_printf(2, "####################################################################\n");
		goto fail;
	}
	if (memcmp(buf + 8, "header0", 8)) {
		isp_printf(2, "####################################################################\n");
		isp_printf(2, "#### The first flag of bin doesn't match with driver! ####\n");
		isp_printf(2, "####################################################################\n");
		goto fail;
	}
	/* the checksum covers size/4 words from +24, as the firmware computes it */
	if (FW(buf, 16, u32) > size - TP_HDR_SIZE) {
		isp_printf(2, "The size of file has been modify!\n");
		goto fail;
	}
	want = FW(buf, 20, u32);
	crc = tp_crc((const u32 *)(buf + TP_HDR_SIZE), FW(buf, 16, u32) >> 2);
	if (crc != want) {
		isp_printf(2, "%s[%d]: Failed to CRC sensor setting!\n",
			   "tiziano_load_parameters", 736);
		goto fail;
	}
	memcpy(tparams_day + TP_DST_OFF, buf + TP_HDR_SIZE, TP_BLOCK);
	memcpy(tparams_night + TP_DST_OFF, buf + TP_HDR_SIZE + TP_BLOCK, TP_BLOCK);
	kvfree(buf);
	isp_printf(0, "ISP: loaded %s\n", path);
	return 0;
fail:
	kvfree(buf);
	return -1;
}

/* ---- install --------------------------------------------------------------- */

struct fwc_slot {
	void **table;
	int index;
	const void *fw;
	void *c;
	const char *name;
};

#define SLOT(t, i, f)	{ t, i, t21fw_##f, fwc_##f, #f }

static const struct fwc_slot fwc_slots[] = {
	SLOT(tx_isp_fops, F310_OPEN, tx_isp_open),
	SLOT(tx_isp_fops, F310_RELEASE, tx_isp_release),
	SLOT(tx_isp_fops, F310_UNLOCKED_IOCTL, tx_isp_unlocked_ioctl),
	SLOT(fs_channel_ops, F310_OPEN, frame_channel_open),
	SLOT(fs_channel_ops, F310_RELEASE, frame_channel_release),
	SLOT(fs_channel_ops, F310_UNLOCKED_IOCTL, frame_channel_unlocked_ioctl),
	SLOT(isp_core_tunning_fops, F310_OPEN, isp_core_tunning_open),
	SLOT(isp_core_tunning_fops, F310_RELEASE, isp_core_tunning_release),
	SLOT(isp_core_tunning_fops, F310_UNLOCKED_IOCTL, isp_core_tunning_unlocked_ioctl),
	SLOT(isp_vic_frd_fops, F310_WRITE, isp_vic_cmd_set),
	SLOT(video_input_cmd_fops, F310_WRITE, video_input_cmd_set),
};

int t21_fwc_install(void)
{
	int i;

	/* all or nothing: a different firmware build fails here */
	for (i = 0; i < ARRAY_SIZE(fwc_slots); i++) {
		if (fwc_slots[i].table[fwc_slots[i].index] != fwc_slots[i].fw) {
			pr_err("t21-isp: %s: unexpected firmware table entry\n",
			       fwc_slots[i].name);
			return -ENOEXEC;
		}
	}
	for (i = 0; i < ARRAY_SIZE(fwc_slots); i++)
		fwc_slots[i].table[fwc_slots[i].index] = fwc_slots[i].c;
	return 0;
}
