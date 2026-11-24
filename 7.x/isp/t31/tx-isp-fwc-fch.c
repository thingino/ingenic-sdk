// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-fch.c - C replacements for two firmware functions that read
 * kernel objects through 3.10 offsets the compiler inlined
 *
 * frame_channel_unlocked_ioctl: the ioctl slot (index 8) of the firmware's
 * 3.10 fs_channel_ops; it read file->private_data at 3.10's offset. Switched
 * in fwc_fch_install(), before tx_isp_init() registers anything.
 *
 * ispcore_pad_event_handle: reached through a pointer tx_isp_core_probe()
 * stores into every wired pad of the core subdev; it carried an inlined
 * spin_unlock_irqrestore() (preempt_count and TIF_NEED_RESCHED of the 3.10
 * thread_info). Switched after probe in fwc_pad_install().
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/dma-direction.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/string.h>

#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

#define FW_ENOIOCTLCMD	(-515)

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

/* frame channel helpers and data, local to the firmware */
extern int __frame_channel_vb2_streamoff(void *chan, int type);
extern int __vb2_queue_free(void *q, int n);
extern int __vb2_queue_cancel(void *q);
extern void __enqueue_in_driver(void *vb);
extern int __fill_v4l2_buffer(void *vb, void *buf);
extern int frame_channel_vidioc_get_fmt(void *chan, void __user *arg);
extern int frame_channel_vidioc_set_fmt(void *chan, void __user *arg);
extern int check_state(void *q);
extern u8 ch0_pre_dequeue_vb[];
extern int isp_ch0_pre_dequeue_time;
extern int isp_ch0_pre_dequeue_interrupt_process;

/* ispcore helpers and data */
extern int tisp_channel_start(int ch);
extern int tisp_channel_fifo_clear(int ch);
extern int tisp_channel_attr_set(int ch, void *attr);
extern int ispcore_frame_channel_dqbuf(void);
extern int ispcore_frame_channel_streamoff(void *pad);
extern u8 isp_output_fmt[];
extern u8 *ispcore_sd;

/* the firmware's own versions, for the slot and pointer checks */
extern char t31fw_frame_channel_unlocked_ioctl[];
extern char t31fw_ispcore_pad_event_handle[];

extern void *fs_channel_ops[];

enum { F310_UNLOCKED_IOCTL = 8 };

int fwc_fch_install(void);
int fwc_pad_install(void);

/* ---- /dev/framechanN ------------------------------------------------------
 *
 * private_data is the channel's miscdevice, at the start of the channel; the
 * vb2 queue starts at +0x24 (its type first). Spinlocks are empty on the
 * firmware's uniprocessor build, so a lock shares its address with the field
 * that follows it (the done lock at +0x224 with the done count).
 */
#define CH_VBQ		0x24
#define CH_BUFSZ	0x34
#define CH_MEMTYPE	0x3c
#define CH_SIZEIMAGE	0x58
#define CH_VBS		0x10c
#define CH_NBUF		0x20c
#define CH_QUEUED	0x210
#define CH_QUEUED_PREV	0x214
#define CH_QUEUED_CNT	0x218
#define CH_DONE		0x21c
#define CH_DONE_LOCK	0x224
#define CH_DONE_CNT	0x224
#define CH_WQ		0x228
#define CH_FLAGS	0x230
#define CH_PREDEQ	0x238
#define CH_OPEN_STATE	0x23c
#define CH_INDEX	0x2c0
#define CH_QLOCK	0x28
#define CH_LIST_LOCK	0x2c4
#define CH_PAD		0x2bc
#define CH_STATE	0x2d0
#define CH_DONE_CMPL	0x2d4

#define CH_STREAMING	0x1

/* offsets inside a firmware vb */
#define VB_INDEX	0x00
#define VB_TYPE		0x04
#define VB_FLAGS	0x0c
#define VB_F10		0x10
#define VB_F14		0x14
#define VB_F18		0x18
#define VB_MEMORY	0x30
#define VB_USERPTR	0x34
#define VB_LENGTH	0x38
#define VB_STATE	0x48
#define VB_STATE8	0x4c
#define VB_QUEUE	0x44
#define VB_QENTRY	0x58
#define VB_DENTRY	0x60
#define VB_LIST		0x68
#define VB_SYNC		0x70

#define FCH_IOC_SET_BANKS	0x800456c5
#define FCH_IOC_WAIT		0x400456bf
#define FCH_IOC_GET_FMT		0x407056c4
#define FCH_IOC_SET_FMT		0xc07056c3
#define FCH_IOC_STREAMON	0x80045612
#define FCH_IOC_STREAMOFF	0x80045613
#define FCH_IOC_REQBUFS		0xc0145608
#define FCH_IOC_QUERYBUF	0xc0445609
#define FCH_IOC_QBUF		0xc044560f
#define FCH_IOC_DQBUF		0xc0445611

#define EV_CHAN_ENABLE		0x3000008
#define EV_STREAM_START		0x3000003

/* 3.10's GFP_KERNEL, as the firmware passes it to private_kmalloc */
#define FW_GFP_KERNEL		0xd0

#define FCH_MAX_BUFS		64

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

static long fch_send_event(u8 *ch, unsigned int ev, void *data)
{
	int ret = tx_isp_send_event_to_remote(FW(ch, CH_PAD, void *), ev, data);

	return ret == FW_ENOIOCTLCMD ? 0 : ret;
}

static long fch_set_banks(u8 *ch, unsigned long arg)
{
	u32 v = 0;
	long ret;

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(&v, (void __user *)arg, 4))
		return -ENOMEM;
	ret = fch_send_event(ch, EV_CHAN_ENABLE, &v);
	if (ret)
		isp_printf(2, "streamon: driver refused to start streaming\n");
	return ret;
}

static long fch_wait(u8 *ch, unsigned long arg)
{
	u32 v = 0;
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
	long ret;

	if (FW(ch, CH_STATE, int) != 3) {
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
	/* queued_list entries sit at VB_QENTRY in each vb */
	for (vb = FW(ch, CH_QUEUED, u8 *) - VB_QENTRY; vb + VB_QENTRY != ch + CH_QUEUED;
	     vb = FW(vb, VB_QENTRY, u8 *) - VB_QENTRY)
		__enqueue_in_driver(vb);
	FW(ch, CH_FLAGS, u8) |= CH_STREAMING;
	ret = tx_isp_send_event_to_remote(FW(ch, CH_PAD, void *), EV_STREAM_START, NULL);
	if (ret && ret != FW_ENOIOCTLCMD) {
		isp_printf(2, "streamon: driver refused to start streaming\n");
		__vb2_queue_cancel(ch + CH_VBQ);
		FW(ch, CH_FLAGS, u8) &= ~CH_STREAMING;
		return ret;
	}
	FW(ch, CH_STATE, int) = 4;
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
	u32 n, i, nbuf;
	long ret;

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
	/* channel 0 with pre-dequeue schedules one buffer only */
	if (isp_ch0_pre_dequeue_time > 0 && !FW(ch, CH_INDEX, int) && n >= 2) {
		isp_printf(2, "error: one buffer schedule only support nrvbs = 1, chan index:%d, num_buffers:%d\n",
			   0, n);
		return -EINVAL;
	}

	nbuf = FW(ch, CH_NBUF, u32);
	for (i = 0; i != n; i++) {
		u8 *vb = private_kmalloc(FW(ch, CH_BUFSZ, u32), FW_GFP_KERNEL);

		if (!vb) {
			isp_printf(2, "Memory alloc for buffer struct failed\n");
			break;
		}
		memset(vb, 0, FW(ch, CH_BUFSZ, u32));
		FW(vb, VB_STATE, u32) = 0;
		FW(vb, VB_QUEUE, u8 *) = q;
		FW(vb, VB_INDEX, u32) = nbuf + i;
		FW(vb, VB_TYPE, u32) = FW(ch, CH_VBQ, u32);
		FW(vb, VB_MEMORY, u32) = FW(ch, CH_MEMTYPE, u32);
		FW(ch, CH_VBS + (nbuf + i) * 4, u8 *) = vb;
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
	if (!IS_ERR_VALUE((unsigned long)(ch + CH_OPEN_STATE)) &&
	    !IS_ERR_VALUE((unsigned long)(ch + 0x40))) {
		FW(ch, 0x40, u32) = FW(ch, CH_OPEN_STATE, u32);
		memcpy(ch + 0x44, ch + CH_OPEN_STATE + 4, 0x30);
	}
	return 0;
}

static long fch_querybuf(u8 *ch, unsigned long arg)
{
	u32 b[17];

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(b, (void __user *)arg, 68))
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
	return fch_put_user((void __user *)arg, b, 68);
}

static long fch_qbuf(u8 *ch, unsigned long arg)
{
	u32 b[17];
	unsigned long flags = 0;
	u32 idx, nbuf, len, userptr, chan;
	u8 *vb, *prev;

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(b, (void __user *)arg, 68))
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
	chan = FW(ch, CH_INDEX, u32);
	FW(vb, VB_USERPTR, u32) = userptr;
	FW(vb, VB_LENGTH, u32) = len;
	FW(vb, VB_F10, u32) = b[4];
	FW(vb, VB_F14, u32) = b[5];
	FW(vb, VB_F18, u32) = b[6];
	FW(vb, VB_FLAGS, u32) = b[3] & 0xffff1bb8;
	private_dma_sync_single_for_device(NULL, userptr, len, DMA_FROM_DEVICE);
	/* channel 0 pre-dequeue: tag the buffer's uncached alias */
	if (isp_ch0_pre_dequeue_time && !chan)
		*(volatile u32 *)((unsigned long)userptr | 0xa0000000) = 0x12345678;
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
	return fch_put_user((void __user *)arg, b, 68);
}

static long fch_dqbuf(u8 *ch, unsigned long arg)
{
	u32 b[17];
	unsigned long flags = 0;
	u8 *vb = NULL;
	int ret = 0;
	bool got;

	if (!fch_uarg_ok(arg))
		return -EINVAL;
	if (fch_get_user(b, (void __user *)arg, 68))
		return -ENOMEM;
	if (b[1] != FW(ch, CH_VBQ, u32)) {
		isp_printf(2, "dqbuf: invalid buffer type\n");
		return -EINVAL;
	}
	for (;;) {
		u8 *lock = ch + CH_DONE_LOCK;

		if (!(FW(ch, CH_FLAGS, u32) & CH_STREAMING)) {
			isp_printf(2, "Streaming off, will not wait for buffers,chan index %d\n",
				   FW(ch, CH_INDEX, int));
			return -EINVAL;
		}
		__private_spin_lock_irqsave(lock, &flags);
		got = false;
		if (!FW(ch, CH_INDEX, int) && isp_ch0_pre_dequeue_interrupt_process &&
		    FW(ch, CH_PREDEQ, u8) && FW(ch0_pre_dequeue_vb, 0x40, u16) == 2) {
			FW(ch, CH_PREDEQ, u8) = 0;
			vb = ch0_pre_dequeue_vb;
			got = true;
		} else {
			u8 *e = FW(ch, CH_DONE, u8 *);

			if (e != ch + CH_DONE) {
				u8 *next = FW(e, 0, u8 *);
				u8 *prv = FW(e, 4, u8 *);

				FW(next, 4, u8 *) = prv;
				FW(prv, 0, u8 *) = next;
				FW(e, 0, u32) = 0x100100;
				FW(e, 4, u32) = 0x200200;
				FW(ch, CH_DONE_CNT, u32)--;
				vb = e - VB_DENTRY;
				got = true;
			}
		}
		private_spin_unlock_irqrestore(lock, flags);
		if (got)
			break;
		ret = private_wait_event_interruptible(ch + CH_WQ, check_state, ch + CH_VBQ);
		if (ret == -ERESTARTSYS || ret == 0)
			continue;
		isp_printf(2, "Sleep was interrupted\n");
		break;
	}
	/*
	 * A buffer taken after an earlier interrupted wait is dropped with
	 * the error, as the firmware does.
	 */
	if (ret < 0)
		return ret;
	if (!vb)
		return -EINVAL;
	__fill_v4l2_buffer(vb, b);
	FW(vb, VB_STATE, u32) = 0;
	return fch_put_user((void __user *)arg, b, 68);
}

static long fwc_frame_channel_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u8 *ch = file->private_data;

	if (IS_ERR_OR_NULL(ch)) {
		/*
		 * The firmware called the format handlers with NULL (both
		 * refuse it) and dereferenced NULL for the completion wait.
		 */
		switch (cmd) {
		case FCH_IOC_SET_BANKS:
		case FCH_IOC_STREAMON:
		case FCH_IOC_STREAMOFF:
		case FCH_IOC_REQBUFS:
		case FCH_IOC_QUERYBUF:
		case FCH_IOC_QBUF:
		case FCH_IOC_DQBUF:
		case FCH_IOC_GET_FMT:
		case FCH_IOC_SET_FMT:
		case FCH_IOC_WAIT:
			return -EINVAL;
		default:
			return FW_ENOIOCTLCMD;
		}
	}

	switch (cmd) {
	case FCH_IOC_SET_BANKS:
		return fch_set_banks(ch, arg);
	case FCH_IOC_WAIT:
		return fch_wait(ch, arg);
	case FCH_IOC_GET_FMT:
		return frame_channel_vidioc_get_fmt(ch, (void __user *)arg);
	case FCH_IOC_SET_FMT:
		return frame_channel_vidioc_set_fmt(ch, (void __user *)arg);
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
	default:
		return FW_ENOIOCTLCMD;
	}
}

/*
 * Replace the ioctl slot of the firmware's fs_channel_ops. All or nothing:
 * a different firmware build fails the check.
 */
int fwc_fch_install(void)
{
	if (fs_channel_ops[F310_UNLOCKED_IOCTL] != t31fw_frame_channel_unlocked_ioctl) {
		pr_err("t31-isp: frame_channel_unlocked_ioctl: unexpected firmware table entry\n");
		return -ENOEXEC;
	}
	fs_channel_ops[F310_UNLOCKED_IOCTL] = fwc_frame_channel_unlocked_ioctl;
	return 0;
}

/* ---- ispcore pad events ----------------------------------------------------
 *
 * The pad is one element (0x24 bytes) of the core subdev's pad array:
 * +0 the owning subdev, +4 the channel index, +5 non-zero when wired, +7 the
 * pad state, +0x14 flags, +0x1c this handler, +0x20 the channel. The channel
 * (0xc4 bytes) holds: +4 width, +8 height, +0xc pixelformat, +0x14 bytes per
 * line, +0x18 image size, +0x20 its format table entry, +0x70 index, +0x74
 * state, +0x7c the core, +0x9c its lock, +0xbc line stride in bytes. The core
 * (the subdev at +0xd4 of the pad's subdev) keeps the pad array at +0xcc, its
 * length at +0x154, the format table range at +0x148/+0x14c, the bypass mode
 * flag at +0x15c and, in that mode, the callbacks at +0x1c0/+0x1c4/+0x1c8/
 * +0x1cc with their cookie at +0x1d0.
 */
#define PAD_SD		0x00
#define PAD_INDEX	0x04
#define PAD_WIRED	0x05
#define PAD_STATE	0x07
#define PAD_FLAGS	0x14
#define PAD_EVENT	0x1c
#define PAD_CHAN	0x20
#define PAD_SIZE	0x24
#define PAD_F_NOSTREAM	0x20

#define CHAN_W		0x04
#define CHAN_H		0x08
#define CHAN_FMT	0x0c
#define CHAN_BPL	0x14
#define CHAN_SIZE	0x18
#define CHAN_FMTENT	0x20
#define CHAN_INDEX	0x70
#define CHAN_STATE	0x74
#define CHAN_CORE	0x7c
#define CHAN_LOCK	0x9c
#define CHAN_STRIDE	0xbc
#define CHAN_SIZEOF	0x70

#define SD_PEERS	0x38
#define SD_PEERS_END	0x78
#define SD_REGBASE	0xb8
#define SD_HOST		0xd4
#define SD_OPS		0xc4

#define CORE_PADS	0xcc
#define CORE_NPADS	0x154
#define CORE_FMT_FIRST	0x148
#define CORE_FMT_LAST	0x14c
#define CORE_BYPASS	0x15c
#define CORE_CB_FIFO	0x1c0
#define CORE_CB_DQBUF	0x1c4
#define CORE_CB_CLEAR	0x1c8
#define CORE_CB_START	0x1cc
#define CORE_CB_COOKIE	0x1d0
#define CORE_CB_AREA	0x1c0
#define CORE_CB_AREA_SZ	0x18
#define CORE_CB_PAD	0x1d4

#define FMT_ENT_SIZE	0x2c
#define FMT_ENT_ID	0x20
#define FMT_ENT_BPP	0x24

#define PIX_NV12	0x3231564e
#define PIX_NV21	0x3132564e
#define PIX_RGB_BYPASS	0x32314752

#define SD_OPS_CORE	0
#define COREOP_IOCTL	0x1c
#define EV_PEER_FMT	0x3000009

#define EV_G_FMT	0x3000001
#define EV_S_FMT	0x3000002
#define EV_STREAMON	0x3000003
#define EV_STREAMOFF	0x3000004
#define EV_QBUF		0x3000005
#define EV_SYNC		0x3000006
#define EV_FIFO_CLEAR	0x3000007

#define TISP_ATTR_WORDS	13

typedef int (*fw_coreop_ioctl)(void *sd, unsigned int ev, void *arg);
typedef int (*fw_core_cb)(void *cookie, void *arg);

static u8 *pad_sd(u8 *pad)
{
	u8 *sd;

	if (IS_ERR_OR_NULL(pad))
		return NULL;
	sd = FW(pad, PAD_SD, u8 *);
	return IS_ERR_OR_NULL(sd) ? NULL : sd;
}

/*
 * Offer the event to each peer subdev's core ioctl, stopping at the first
 * error other than "not implemented".
 */
static void pad_notify_peers(u8 *sd, void *arg)
{
	u8 *slot;

	for (slot = sd + SD_PEERS; slot != sd + SD_PEERS_END; slot += 4) {
		u8 *peer = FW(slot, 0, u8 *);
		u8 *ops;
		fw_coreop_ioctl op;
		int ret;

		if (!peer)
			continue;
		ops = FW(peer, SD_OPS, u8 *);
		if (!ops || !FW(ops, SD_OPS_CORE, u8 *))
			continue;
		op = FW(FW(ops, SD_OPS_CORE, u8 *), COREOP_IOCTL, fw_coreop_ioctl);
		if (!op)
			continue;
		ret = op(peer, EV_PEER_FMT, arg);
		if (ret && ret != FW_ENOIOCTLCMD)
			break;
	}
}

static int pad_g_fmt(u8 *pad, u8 *arg)
{
	u8 *chan = FW(pad, PAD_CHAN, u8 *);

	if (!arg || !chan)
		return 0;
	if (FW(FW(chan, CHAN_CORE, u8 *), CORE_BYPASS, int) != 1) {
		memcpy(arg, chan, CHAN_SIZEOF);
		return 0;
	}
	FW(arg, CHAN_W, u32) = FW(chan, CHAN_W, u32);
	FW(arg, CHAN_H, u32) = FW(chan, CHAN_H, u32);
	FW(arg, CHAN_FMT, u32) = PIX_RGB_BYPASS;
	FW(arg, 0x34, u8) = 0;
	FW(arg, 0x48, u8) = 0;
	FW(arg, CHAN_SIZE, u32) = FW(chan, CHAN_W, u32) * FW(chan, CHAN_H, u32) * 2;
	return 0;
}

static int pad_s_fmt(u8 *pad, u8 *arg)
{
	u8 *sd = pad_sd(pad);
	u8 *core, *chan, *ent = NULL;
	u32 fmt, i;
	u32 attr[TISP_ATTR_WORDS];

	if (!sd)
		return -EINVAL;
	core = FW(sd, SD_HOST, u8 *);
	if (IS_ERR_OR_NULL(core))
		return -EINVAL;
	chan = FW(pad, PAD_CHAN, u8 *);

	if (FW(core, CORE_BYPASS, int) == 1) {
		memset(core + CORE_CB_AREA, 0, CORE_CB_AREA_SZ);
		FW(core, CORE_CB_PAD, u8 *) = pad;
		FW(core, CORE_CB_DQBUF, void *) = ispcore_frame_channel_dqbuf;
		pad_notify_peers(sd, core + CORE_CB_AREA);
		return 0;
	}

	pad_notify_peers(sd, NULL);

	fmt = FW(arg, CHAN_FMT, u32);
	for (i = FW(FW(chan, CHAN_CORE, u8 *), CORE_FMT_FIRST, u32);
	     i <= FW(FW(chan, CHAN_CORE, u8 *), CORE_FMT_LAST, u32); i++) {
		if (FW(isp_output_fmt + i * FMT_ENT_SIZE, FMT_ENT_ID, u32) == fmt) {
			ent = isp_output_fmt + i * FMT_ENT_SIZE;
			break;
		}
	}

	if (ent) {
		u32 bpp = FW(ent, FMT_ENT_BPP, u32);
		u32 bpl = (FW(arg, CHAN_W, u32) * bpp) >> 3;
		u32 h = FW(arg, CHAN_H, u32);

		FW(arg, CHAN_BPL, u32) = bpl;
		if (fmt == PIX_NV12 || fmt == PIX_NV21)
			h = (h + 15) & ~15;
		FW(arg, CHAN_SIZE, u32) = h * bpl;
		FW(chan, CHAN_STRIDE, u32) = (bpp >> 3) * FW(arg, CHAN_W, u32);
		FW(arg, CHAN_FMTENT, u8 *) = ent;
	} else {
		isp_printf(2, "%s[%d] unfound the pixelformat = %d\n",
			   "ispcore_frame_channel_s_fmt", 830, fmt);
	}

	memset(attr, 0, sizeof(attr));
	attr[0] = FW(arg, 0x48, u8);
	attr[1] = FW(arg, 0x4c, u32);
	attr[2] = FW(arg, 0x50, u32);
	attr[3] = FW(arg, 0x34, u8);
	attr[4] = FW(arg, 0x3c, u32);
	attr[5] = FW(arg, 0x38, u32);
	attr[6] = FW(arg, 0x40, u32);
	attr[7] = FW(arg, 0x44, u32);
	attr[8] = FW(arg, 0x5c, u8);
	attr[9] = FW(arg, 0x64, u32);
	attr[10] = FW(arg, 0x60, u32);
	attr[11] = FW(arg, 0x68, u32);
	attr[12] = FW(arg, 0x6c, u32);
	if (tisp_channel_attr_set(FW(pad, PAD_INDEX, u8), attr)) {
		isp_printf(2, "%s:%d:tiziano set channel attr is error!!!\n",
			   "ispcore_frame_channel_set_fmt", 956);
		return 0;
	}
	memcpy(chan, arg, CHAN_SIZEOF);
	return 0;
}

static int pad_streamon(u8 *pad, u8 *core)
{
	u8 *chan = FW(pad, PAD_CHAN, u8 *);
	unsigned long flags = 0;

	if (FW(core, CORE_BYPASS, int) == 1) {
		fw_core_cb cb = FW(core, CORE_CB_START, fw_core_cb);

		if (cb)
			cb(FW(core, CORE_CB_COOKIE, void *), (void *)1);
		return 0;
	}
	if (FW(pad, PAD_STATE, u8) != 3)
		return 0;
	__private_spin_lock_irqsave(chan + CHAN_LOCK, &flags);
	if (FW(chan, CHAN_STATE, int) == 4) {
		spin_unlock_irqrestore((spinlock_t *)(chan + CHAN_LOCK), flags);
		return 0;
	}
	tisp_channel_start(FW(pad, PAD_INDEX, u8));
	FW(chan, CHAN_STATE, int) = 4;
	FW(pad, PAD_STATE, u8) = 4;
	private_spin_unlock_irqrestore(chan + CHAN_LOCK, flags);
	return 0;
}

static int pad_qbuf(u8 *pad, u8 *sd, u8 *core, u8 *arg)
{
	u8 *chan;
	unsigned long flags = 0;
	u32 stride_base, uv;

	if (FW(core, CORE_BYPASS, int) == 1) {
		fw_core_cb cb = FW(core, CORE_CB_FIFO, fw_core_cb);

		if (cb)
			cb(FW(core, CORE_CB_COOKIE, void *), arg);
		return 0;
	}
	if (FW(pad, PAD_FLAGS, u32) & PAD_F_NOSTREAM)
		return 0;
	chan = FW(pad, PAD_CHAN, u8 *);
	if (!arg || !chan) {
		isp_printf(2, "error: %s,%d, buf = %p, chan = %p\n",
			   "ispcore_frame_channel_qbuf", 704, arg, chan);
		return 0;
	}
	/* the buffer's own state byte sits 0x1c bytes before the argument */
	FW(arg, -0x1c, u8) = 4;
	__private_spin_lock_irqsave(chan + CHAN_LOCK, &flags);
	if (FW(chan, CHAN_FMT, u32) != PIX_NV12) {
		isp_printf(2, "error: %s, %d, not support this format\n",
			   "ispcore_frame_channel_qbuf", 689);
		/* the firmware returned with the lock still held */
		private_spin_unlock_irqrestore(chan + CHAN_LOCK, flags);
		return -1;
	}
	stride_base = (FW(chan, CHAN_H, u32) + 15) & ~15u;
	FW(arg, -0x1c, u8) = 5;
	FW(arg, -0x18, u32)++;
	uv = FW(arg, 8, u32);
	FW(arg, 0xc, u32) = stride_base * FW(chan, CHAN_W, u32) + uv;
	FW(FW(sd, SD_REGBASE, u8 *), FW(chan, CHAN_INDEX, u32) * 0x100 + 0x996c, u32) = uv;
	FW(FW(sd, SD_REGBASE, u8 *), FW(chan, CHAN_INDEX, u32) * 0x100 + 0x9984, u32) =
		FW(arg, 0xc, u32);
	private_spin_unlock_irqrestore(chan + CHAN_LOCK, flags);
	return 0;
}

static int pad_fifo_clear(u8 *pad, u8 *core, void *arg)
{
	u8 *chan;
	unsigned long flags = 0;

	if (FW(core, CORE_BYPASS, int) == 1) {
		fw_core_cb cb = FW(core, CORE_CB_CLEAR, fw_core_cb);

		if (cb)
			cb(FW(core, CORE_CB_COOKIE, void *), arg);
		return 0;
	}
	if (FW(pad, PAD_FLAGS, u32) & PAD_F_NOSTREAM)
		return 0;
	chan = FW(pad, PAD_CHAN, u8 *);
	if (!chan)
		return 0;
	__private_spin_lock_irqsave(chan + CHAN_LOCK, &flags);
	tisp_channel_fifo_clear(FW(pad, PAD_INDEX, u8));
	private_spin_unlock_irqrestore(chan + CHAN_LOCK, flags);
	return 0;
}

static int fwc_ispcore_pad_event_handle(u8 *pad, unsigned int ev, void *arg)
{
	u8 *sd, *core;

	if (!FW(pad, PAD_WIRED, u8))
		return 0;

	switch (ev) {
	case EV_G_FMT:
		return pad_g_fmt(pad, arg);
	case EV_S_FMT:
		return pad_s_fmt(pad, arg);
	case EV_STREAMOFF:
		return ispcore_frame_channel_streamoff(pad);
	case EV_STREAMON:
	case EV_QBUF:
	case EV_FIFO_CLEAR:
		/* the firmware dereferenced NULL when the pad had no core */
		sd = pad_sd(pad);
		core = sd ? FW(sd, SD_HOST, u8 *) : NULL;
		if (IS_ERR_OR_NULL(core))
			return -EINVAL;
		if (ev == EV_STREAMON)
			return pad_streamon(pad, core);
		if (ev == EV_QBUF)
			return pad_qbuf(pad, sd, core, arg);
		return pad_fifo_clear(pad, core, arg);
	default:
		return 0;
	}
}

/*
 * tx_isp_core_probe() stores the handler into pad +0x1c of every wired pad
 * of the core subdev (array at core+0xcc, count at core+0x154, 0x24 bytes
 * each), then publishes the core in ispcore_sd. Call this after the probe
 * has succeeded; it replaces those pointers, all or nothing.
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
		    FW(pad, PAD_EVENT, void *) != (void *)t31fw_ispcore_pad_event_handle) {
			pr_err("t31-isp: ispcore_pad_event_handle: unexpected pad %u handler\n", i);
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
