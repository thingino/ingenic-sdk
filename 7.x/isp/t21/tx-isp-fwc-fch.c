// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-fch.c - C for the firmware's /dev/framechanN ioctl
 *
 * frame_channel_unlocked_ioctl is the unlocked_ioctl slot of the firmware's
 * 3.10 fs_channel_ops. The firmware version reads file->private_data at its
 * 3.10 offset; this one goes through the 7.x struct file. Everything else it
 * touches is the firmware's own (the channel, its vb2-style queue and
 * buffers), reached by the firmware's offsets.
 *
 * private_data is the channel's miscdevice, at the start of the channel; the
 * queue starts at +0x24 (its type first). The firmware's spinlocks are empty
 * on its uniprocessor build, so a lock shares its address with the field
 * that follows it (the done lock at +0x224 with the done count).
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/dma-direction.h>

#include "tx-isp-abi.h"

#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

/* ---- firmware symbols ------------------------------------------------------ */

extern int isp_printf(int level, const char *fmt, ...);
extern int private_copy_from_user(void *to, const void __user *from, unsigned long n);
extern int private_copy_to_user(void __user *to, const void *from, unsigned long n);
extern void private_mutex_lock(void *m);
extern void private_mutex_unlock(void *m);
extern void *private_kmalloc(size_t size, int flags);
extern int private_wait_for_completion_interruptible(void *c);
extern int private_wait_event_interruptible(void *wq, int (*cond)(void *), void *arg);
extern void __private_spin_lock_irqsave(void *lock, unsigned long *flags);
extern void private_spin_unlock_irqrestore(void *lock, unsigned long flags);
extern void private_dma_sync_single_for_device(struct device *dev, dma_addr_t addr,
					       size_t size, enum dma_data_direction dir);
extern int tx_isp_send_event_to_remote(void *pad, unsigned int cmd, void *data);

/* the mscaler channel the /proc commands and the vic snapshot look at */
extern int msca_ch_nrvbs;
extern u32 msca_ch_buf_addr;
extern void *main_channel_pad, *second_channel_pad;
extern void *main_channel, *second_channel;

/* frame channel helpers and data, local to the firmware */
extern int __frame_channel_vb2_streamoff(void *chan, int type);
extern int __vb2_queue_free(void *q, int n);
extern int __vb2_queue_cancel(void *q);
extern void __enqueue_in_driver(void *vb);
extern int __fill_v4l2_buffer(void *vb, void *buf);
extern int check_state(void *q);
extern int open_count asm("count.35161");

/*
 * The firmware returns -ENOIOCTLCMD (-515) from subdev events it does not
 * implement; the ioctl treats it as success, and answers it for commands it
 * does not know.
 */
#define FW_ENOIOCTLCMD		(-515)

/* ---- channel and buffer layout --------------------------------------------- */

#define CH_VBQ			0x24	/* the queue: its type first */
#define CH_QLOCK		0x28
#define CH_BUFSZ		0x34
#define CH_MEMTYPE		0x3c
#define CH_SIZEIMAGE		0x58
#define CH_VBS			0x10c	/* the buffer pointers */
#define CH_NBUF			0x20c
#define CH_QUEUED		0x210	/* list head, then its prev, then the count */
#define CH_QUEUED_PREV		0x214
#define CH_QUEUED_CNT		0x218
#define CH_DONE			0x21c
#define CH_DONE_LOCK		0x224
#define CH_DONE_CNT		0x224
#define CH_WQ			0x228
#define CH_FLAGS		0x230
#define CH_FMT			0x234	/* the last format set, 76 bytes */
#define CH_PAD			0x290
#define CH_INDEX		0x294
#define CH_LIST_LOCK		0x298
#define CH_STATE		0x2a4
#define CH_DONE_CMPL		0x2a8

#define CH_STREAMING		0x1
#define CH_STATE_OPEN		3
#define CH_STATE_STREAMING	4

#define VB_INDEX		0x00
#define VB_TYPE			0x04
#define VB_FLAGS		0x0c
#define VB_F10			0x10
#define VB_F14			0x14
#define VB_F18			0x18
#define VB_MEMORY		0x30
#define VB_USERPTR		0x34
#define VB_LENGTH		0x38
#define VB_QUEUE		0x44
#define VB_STATE		0x48
#define VB_STATE8		0x4c
#define VB_QENTRY		0x58
#define VB_DENTRY		0x60
#define VB_LIST			0x68
#define VB_SYNC			0x70

#define FCH_IOC_SET_BANKS	0x800456c5
#define FCH_IOC_WAIT		0x400456bf
#define FCH_IOC_GET_FMT		0x404c56c4
#define FCH_IOC_SET_FMT		0xc04c56c3
#define FCH_IOC_SET_PEER_FMT	0xc04c56d6
#define FCH_IOC_STREAMON	0x80045612
#define FCH_IOC_STREAMOFF	0x80045613
#define FCH_IOC_REQBUFS		0xc0145608
#define FCH_IOC_QUERYBUF	0xc0445609
#define FCH_IOC_QBUF		0xc044560f
#define FCH_IOC_DQBUF		0xc0445611

#define EV_G_FMT		0x3000001
#define EV_S_FMT		0x3000002
#define EV_STREAM_START		0x3000003
#define EV_CHAN_ENABLE		0x3000008
#define EV_PEER_FMT		0x3000009

/* 3.10's GFP_KERNEL, as the firmware passes it to private_kmalloc */
#define FW_GFP_KERNEL		0xd0

#define FCH_MAX_BUFS		64

/* struct v4l2_buffer is 68 bytes, struct v4l2_format 76 (as the firmware lays it out) */
#define V4L2_BUF_SIZE		68
#define V4L2_FMT_SIZE		76

static bool fch_uarg_ok(unsigned long arg)
{
	if (!arg || IS_ERR_VALUE(arg)) {
		isp_printf(2, "The parameter from user is invalid!\n");
		return false;
	}
	return true;
}

/* 0 or -ENOMEM, the firmware's code for every user copy failure */
static int fch_get_user(void *to, void __user *from, unsigned long n)
{
	if (private_copy_from_user(to, from, n)) {
		isp_printf(2, "Failed to copy from user\n");
		return -ENOMEM;
	}
	return 0;
}

static int fch_put_user(void __user *to, const void *from, unsigned long n)
{
	if (private_copy_to_user(to, from, n)) {
		isp_printf(2, "Failed to copy to user\n");
		return -ENOMEM;
	}
	return 0;
}

static long fch_set_banks(u8 *ch, unsigned long arg)
{
	u32 v = 0;
	int ret;

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(&v, (void __user *)arg, 4))
		return -ENOMEM;
	ret = tx_isp_send_event_to_remote(FW(ch, CH_PAD, void *), EV_CHAN_ENABLE, &v);
	if (ret && ret != FW_ENOIOCTLCMD) {
		isp_printf(2, "streamon: driver refused to start streaming\n");
		return ret;
	}
	return 0;
}

static long fch_wait(u8 *ch, unsigned long arg)
{
	u32 v;
	int ret;

	ret = private_wait_for_completion_interruptible(ch + CH_DONE_CMPL);
	v = ret < 0 ? ret : FW(ch, CH_DONE_CMPL, u32) + 1;
	if (fch_put_user((void __user *)arg, &v, 4))
		return -ENOMEM;
	return ret;
}

static long fch_streamon(u8 *ch, unsigned long arg)
{
	u32 type;
	u8 *vb;
	int ret;

	if (FW(ch, CH_STATE, int) != CH_STATE_OPEN) {
		isp_printf(2, "The state of frame channel%d is invalid(%d)!\n",
			   FW(ch, CH_INDEX, int), FW(ch, CH_STATE, int));
		return -1;
	}
	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(&type, (void __user *)arg, 4))
		return -ENOMEM;
	if (FW(ch, CH_VBQ, u32) != type) {
		isp_printf(2, "streamon: invalid stream type\n");
		return -EINVAL;
	}
	if (FW(ch, CH_FLAGS, u32) & CH_STREAMING) {
		isp_printf(2, "streamon: already streaming\n");
		return -EBUSY;
	}
	/* the queued list's entries sit at VB_QENTRY in each buffer */
	for (vb = FW(ch, CH_QUEUED, u8 *) - VB_QENTRY; vb + VB_QENTRY != ch + CH_QUEUED;
	     vb = FW(vb, VB_QENTRY, u8 *) - VB_QENTRY)
		__enqueue_in_driver(vb);
	ret = tx_isp_send_event_to_remote(FW(ch, CH_PAD, void *), EV_STREAM_START, NULL);
	if (ret && ret != FW_ENOIOCTLCMD) {
		isp_printf(2, "streamon: driver refused to start streaming\n");
		__vb2_queue_cancel(ch + CH_VBQ);
		return ret;
	}
	FW(ch, CH_FLAGS, u8) |= CH_STREAMING;
	FW(ch, CH_STATE, int) = CH_STATE_STREAMING;
	isp_printf(0, "Streamon successful\n");
	return 0;
}

static long fch_streamoff(u8 *ch, unsigned long arg)
{
	u32 type;

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(&type, (void __user *)arg, 4))
		return -ENOMEM;
	return __frame_channel_vb2_streamoff(ch, type);
}

static long fch_reqbufs(u8 *ch, unsigned long arg)
{
	u8 *q = ch + CH_VBQ;
	u32 b[5], ev;
	u32 n, i;
	int ret;

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(b, (void __user *)arg, 20))
		return -ENOMEM;
	if (FW(ch, CH_FLAGS, u32) & CH_STREAMING) {
		isp_printf(2, "reqbufs: streaming active\n");
		return -EBUSY;
	}
	if (!b[0] || FW(ch, CH_NBUF, u32) || FW(ch, CH_MEMTYPE, u32) != b[2]) {
		__vb2_queue_free(q, FW(ch, CH_NBUF, int));
		if (!b[0])
			return 0;
	}
	if (FW(ch, CH_MEMTYPE, u32) != b[2]) {
		isp_printf(2, "reqbufs: the memory type isn't userptr!\n");
		return -EINVAL;
	}
	n = min_t(u32, b[0], FCH_MAX_BUFS);

	for (i = 0; i != n; i++) {
		u8 *vb = private_kmalloc(FW(ch, CH_BUFSZ, u32), FW_GFP_KERNEL);
		u32 idx;

		if (!vb) {
			isp_printf(2, "Memory alloc for buffer struct failed\n");
			break;
		}
		memset(vb, 0, FW(ch, CH_BUFSZ, u32));
		idx = FW(ch, CH_NBUF, u32) + i;
		FW(vb, VB_STATE, u32) = 0;
		FW(vb, VB_QUEUE, u8 *) = q;
		FW(vb, VB_INDEX, u32) = idx;
		FW(vb, VB_TYPE, u32) = FW(ch, CH_VBQ, u32);
		FW(vb, VB_MEMORY, u32) = FW(ch, CH_MEMTYPE, u32);
		FW(ch, CH_VBS + idx * 4, u8 *) = vb;
	}
	isp_printf(0, "Allocated %d buffers!\n", i);
	if (!i) {
		isp_printf(2, "Memory allocation failed\n");
		return -ENOMEM;
	}
	ev = i;
	if (i < n) {
		__vb2_queue_free(q, i);
		isp_printf(2, "Can't allocation %d buffers!\n", n);
		return -ENOMEM;
	}
	FW(ch, CH_NBUF, u32) = i;
	b[0] = i;
	ret = tx_isp_send_event_to_remote(FW(ch, CH_PAD, void *), EV_CHAN_ENABLE, &ev);
	if (ret && ret != FW_ENOIOCTLCMD) {
		isp_printf(2, "streamon: driver refused to start streaming\n");
		return ret;
	}
	if (fch_put_user((void __user *)arg, b, 20))
		return -ENOMEM;
	/* the format the channel was set to, kept for the buffers: type, then 32 bytes */
	FW(ch, 0x40, u32) = FW(ch, CH_FMT, u32);
	memcpy(ch + 0x44, ch + CH_FMT + 4, 32);
	msca_ch_nrvbs = FW(ch, CH_NBUF, int);
	return 0;
}

static long fch_querybuf(u8 *ch, unsigned long arg)
{
	u32 b[17];

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(b, (void __user *)arg, V4L2_BUF_SIZE))
		return -ENOMEM;
	if (b[1] != FW(ch, CH_VBQ, u32)) {
		isp_printf(2, "dqbuf: invalid buffer type\n");
		return -EINVAL;
	}
	if (b[0] >= FW(ch, CH_NBUF, u32)) {
		isp_printf(2, "querybuf: buffer index out of range\n");
		return -EINVAL;
	}
	__fill_v4l2_buffer(FW(ch, CH_VBS + b[0] * 4, u8 *), b);
	return fch_put_user((void __user *)arg, b, V4L2_BUF_SIZE);
}

static long fch_qbuf(u8 *ch, unsigned long arg)
{
	u32 b[17];
	unsigned long flags = 0;
	u32 idx, nbuf, len, userptr;
	u8 *vb, *prev;

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(b, (void __user *)arg, V4L2_BUF_SIZE))
		return -ENOMEM;
	if (b[1] != FW(ch, CH_VBQ, u32)) {
		isp_printf(2, "qbuf: invalid buffer type\n");
		return -EINVAL;
	}
	idx = b[0];
	nbuf = FW(ch, CH_NBUF, u32);
	if (idx >= nbuf) {
		isp_printf(2, "qbuf: buffer index(%d) out of range(%d) \n", idx, nbuf);
		return -EINVAL;
	}
	vb = FW(ch, CH_VBS + idx * 4, u8 *);
	if (!vb) {
		isp_printf(2, "qbuf: buffer is NULL\n");
		return -EINVAL;
	}
	/* the first buffer of channel 0 is what the vic's "saveraw" dumps into: the last address queued */
	if (!FW(vb, VB_INDEX, u32) && !FW(ch, CH_INDEX, u32))
		msca_ch_buf_addr = FW(vb, VB_USERPTR, u32);
	FW(vb, VB_STATE8, u8) = 1;
	if (b[12] != FW(ch, CH_MEMTYPE, u32)) {
		isp_printf(2, "qbuf: invalid memory type\n");
		return -EINVAL;
	}
	len = b[14];
	if (len != FW(ch, CH_SIZEIMAGE, u32)) {
		isp_printf(2, "qbuf: invalid memory size, length = %d sizeimage = %d\n",
			   len, FW(ch, CH_SIZEIMAGE, u32));
		return -EINVAL;
	}
	if (FW(vb, VB_STATE, u32)) {
		isp_printf(2, "qbuf: buffer already in use\n");
		return -EINVAL;
	}
	userptr = b[13];
	FW(vb, VB_USERPTR, u32) = userptr;
	FW(vb, VB_LENGTH, u32) = len;
	FW(vb, VB_F10, u32) = b[4];
	FW(vb, VB_F14, u32) = b[5];
	FW(vb, VB_F18, u32) = b[6];
	FW(vb, VB_FLAGS, u32) = b[3] & 0xffff1bb8;
	private_dma_sync_single_for_device(NULL, userptr, len, DMA_FROM_DEVICE);
	FW(vb, VB_LIST, u8 *) = vb + VB_LIST;
	FW(vb, VB_LIST + 4, u8 *) = vb + VB_LIST;
	FW(vb, VB_SYNC, u32) = userptr;
	FW(vb, VB_STATE, u32) = 2;
	FW(vb, VB_STATE8, u8) = 2;
	private_mutex_lock(ch + CH_QLOCK);
	__private_spin_lock_irqsave(ch + CH_LIST_LOCK, &flags);
	prev = FW(ch, CH_QUEUED_PREV, u8 *);
	FW(ch, CH_QUEUED_PREV, u8 *) = vb + VB_QENTRY;
	FW(vb, VB_QENTRY, u8 *) = ch + CH_QUEUED;
	FW(vb, VB_QENTRY + 4, u8 *) = prev;
	FW(prev, 0, u8 *) = vb + VB_QENTRY;
	FW(vb, VB_STATE, u32) = 1;
	FW(ch, CH_QUEUED_CNT, u32)++;
	private_spin_unlock_irqrestore(ch + CH_LIST_LOCK, flags);
	if (FW(ch, CH_FLAGS, u32) & CH_STREAMING)
		__enqueue_in_driver(vb);
	FW(vb, VB_STATE8, u8) = 30;
	__fill_v4l2_buffer(vb, b);
	private_mutex_unlock(ch + CH_QLOCK);
	return fch_put_user((void __user *)arg, b, V4L2_BUF_SIZE);
}

static long fch_dqbuf(u8 *ch, unsigned long arg)
{
	u32 b[17];
	unsigned long flags = 0;
	u8 *vb = NULL;
	int ret;

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(b, (void __user *)arg, V4L2_BUF_SIZE))
		return -ENOMEM;
	if (b[1] != FW(ch, CH_VBQ, u32)) {
		isp_printf(2, "dqbuf: invalid buffer type\n");
		return -EINVAL;
	}
	for (;;) {
		u8 *e;

		if (!(FW(ch, CH_FLAGS, u32) & CH_STREAMING)) {
			isp_printf(2, "Streaming off, will not wait for buffers\n");
			return -EINVAL;
		}
		e = FW(ch, CH_DONE, u8 *);
		if (e != ch + CH_DONE) {
			u8 *next, *prv;

			__private_spin_lock_irqsave(ch + CH_DONE_LOCK, &flags);
			e = FW(ch, CH_DONE, u8 *);
			next = FW(e, 0, u8 *);
			prv = FW(e, 4, u8 *);
			FW(next, 4, u8 *) = prv;
			FW(prv, 0, u8 *) = next;
			FW(e, 0, u32) = 0x100100;
			FW(e, 4, u32) = 0x200200;
			FW(ch, CH_DONE_CNT, u32)--;
			private_spin_unlock_irqrestore(ch + CH_DONE_LOCK, flags);
			vb = e - VB_DENTRY;
			break;
		}
		ret = private_wait_event_interruptible(ch + CH_WQ, check_state, ch + CH_VBQ);
		if (ret == -ERESTARTSYS || ret == 0)
			continue;
		isp_printf(2, "Sleep was interrupted\n");
		if (ret < 0)
			return ret;
		break;
	}
	if (!vb)
		return -EINVAL;
	__fill_v4l2_buffer(vb, b);
	FW(vb, VB_STATE, u32) = 0;
	return fch_put_user((void __user *)arg, b, V4L2_BUF_SIZE);
}

/* ---- formats --------------------------------------------------------------- */

/* the last format, kept in the channel */
static long fch_fmt_done(u8 *ch, unsigned long arg, u32 *f)
{
	if (fch_put_user((void __user *)arg, f, V4L2_FMT_SIZE))
		return -ENOMEM;
	memcpy(ch + CH_FMT, f, V4L2_FMT_SIZE);
	return 0;
}

static long fch_get_fmt(u8 *ch, unsigned long arg)
{
	/* the firmware sent its stack to the event and to the user */
	u32 f[V4L2_FMT_SIZE / 4] = { 0 };
	int ret;

	ret = tx_isp_send_event_to_remote(FW(ch, CH_PAD, void *), EV_G_FMT, f);
	if (ret && ret != FW_ENOIOCTLCMD) {
		isp_printf(2, "Failed to set fmt to frame chan%d!\n", FW(ch, CH_INDEX, int));
		return ret;
	}
	f[0] = 1;
	f[4] = 4;
	f[7] = 8;
	return fch_fmt_done(ch, arg, f);
}

/* set_fmt of the channel itself (counted) and of its peer */
static long fch_set_fmt(u8 *ch, unsigned long arg, unsigned int ev)
{
	u32 f[V4L2_FMT_SIZE / 4];
	int ret;

	if (ev == EV_S_FMT) {
		/* the first channel opened is the main one, the second the sub */
		if (!open_count) {
			main_channel_pad = FW(ch, CH_PAD, void *);
			main_channel = ch;
		} else if (open_count == 1) {
			second_channel_pad = FW(ch, CH_PAD, void *);
			second_channel = ch;
		}
		open_count++;
	}
	if (fch_get_user(f, (void __user *)arg, V4L2_FMT_SIZE))
		return -ENOMEM;
	if (f[0] != 1) {
		isp_printf(2, "The frame type is invalid!\n");
		return -EINVAL;
	}
	if (!f[4]) {
		f[4] = 4;
	} else if (f[4] != 4) {
		isp_printf(2, "The field is invalid!\n");
		return -EINVAL;
	}
	if (f[7] != 8) {
		isp_printf(2, "The colorspace is invalid!\n");
		return -EINVAL;
	}
	ret = tx_isp_send_event_to_remote(FW(ch, CH_PAD, void *), ev, f);
	if (ret && ret != FW_ENOIOCTLCMD) {
		isp_printf(2, "Failed to set fmt to frame chan%d!\n", FW(ch, CH_INDEX, int));
		return ret;
	}
	return fch_fmt_done(ch, arg, f);
}

long fwc_frame_channel_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u8 *ch = file->private_data;

	if (IS_ERR_OR_NULL(ch)) {
		/* the firmware refused with -EINVAL, but dereferenced NULL in the completion wait */
		switch (cmd) {
		case FCH_IOC_SET_BANKS:
		case FCH_IOC_WAIT:
		case FCH_IOC_GET_FMT:
		case FCH_IOC_SET_FMT:
		case FCH_IOC_SET_PEER_FMT:
		case FCH_IOC_STREAMON:
		case FCH_IOC_STREAMOFF:
		case FCH_IOC_REQBUFS:
		case FCH_IOC_QUERYBUF:
		case FCH_IOC_QBUF:
		case FCH_IOC_DQBUF:
			return -EINVAL;
		}
		return FW_ENOIOCTLCMD;
	}

	switch (cmd) {
	case FCH_IOC_SET_BANKS:
		return fch_set_banks(ch, arg);
	case FCH_IOC_WAIT:
		return fch_wait(ch, arg);
	case FCH_IOC_GET_FMT:
		return fch_get_fmt(ch, arg);
	case FCH_IOC_SET_FMT:
		return fch_set_fmt(ch, arg, EV_S_FMT);
	case FCH_IOC_SET_PEER_FMT:
		return fch_set_fmt(ch, arg, EV_PEER_FMT);
	case FCH_IOC_STREAMON:
		return fch_streamon(ch, arg);
	case FCH_IOC_STREAMOFF:
		return fch_streamoff(ch, arg);
	case FCH_IOC_REQBUFS:
		return fch_reqbufs(ch, arg);
	case FCH_IOC_QUERYBUF:
		return fch_querybuf(ch, arg);
	case FCH_IOC_QBUF:
		return fch_qbuf(ch, arg);
	case FCH_IOC_DQBUF:
		return fch_dqbuf(ch, arg);
	}
	return FW_ENOIOCTLCMD;
}
