// SPDX-License-Identifier: GPL-2.0
/*
 * txx-funcs.c - translators for the T21 ISP firmware's 3.10 kernel calls
 *
 * Two kinds of entry point live here. The private_* functions are strong
 * definitions of the weak stubs the firmware ships (its txx-funcs.c is built
 * with the kernel, not in the archive); the t21_*_310 functions are the
 * targets the Kbuild redirects the firmware's direct imports to with
 * objcopy.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/irqflags.h>
#include <linux/pfn.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/timekeeping.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <net/net_namespace.h>
#include <net/netlink.h>
#include <net/sock.h>

#include <txx-funcs.h>
#include <tx-isp-debug.h>
#include "tx-isp-abi.h"

/*
 * The firmware was built with 3.10 gfp bits: GFP_KERNEL was 0xd0 with
 * __GFP_WAIT at 0x10, which on 7.x reads as __GFP_RECLAIMABLE and carries no
 * reclaim permission at all; GFP_ATOMIC was __GFP_HIGH (0x20). Rebuild the
 * mask with 7.x semantics.
 */
#define G310_DMA	0x01u
#define G310_HIGHMEM	0x02u
#define G310_MOVABLE	0x08u
#define G310_WAIT	0x10u
#define G310_HIGH	0x20u
#define G310_IO		0x40u
#define G310_FS		0x80u
#define G310_NOWARN	0x200u
#define G310_NORETRY	0x1000u
#define G310_ZERO	0x8000u

static gfp_t t21_gfp_from_310(gfp_t old)
{
	gfp_t new = 0;

	if (old & G310_DMA)
		new |= __GFP_DMA;
	if (old & G310_HIGHMEM)
		new |= __GFP_HIGHMEM;
	if (old & G310_MOVABLE)
		new |= __GFP_MOVABLE;
	new |= (old & G310_WAIT) ? __GFP_RECLAIM : __GFP_KSWAPD_RECLAIM;
	if (old & G310_HIGH)
		new |= __GFP_HIGH;
	if (old & G310_IO)
		new |= __GFP_IO;
	if (old & G310_FS)
		new |= __GFP_FS;
	if (old & G310_NOWARN)
		new |= __GFP_NOWARN;
	if (old & G310_NORETRY)
		new |= __GFP_NORETRY;
	if (old & G310_ZERO)
		new |= __GFP_ZERO;
	return new;
}

/* 3.10's HZ was 100 */
unsigned long t21_jiffies_from_310(unsigned long j)
{
	return DIV_ROUND_UP(j * HZ, 100);
}

void *private_kmalloc(size_t s, gfp_t gfp)
{
	void *addr = kmalloc(s, t21_gfp_from_310(gfp));

	if (!addr)
		pr_err("%s: kmalloc(%zu) failed\n", __func__, s);
	return addr;
}

/* the firmware inlines kmalloc() of a variable size as __kmalloc() */
void *t21_kmalloc_310(size_t size, gfp_t gfp)
{
	return kmalloc(size, t21_gfp_from_310(gfp));
}

/* targets of the objcopy --redefine-sym rewrites in the Kbuild */

unsigned long t21_irq_save_310(void)
{
	unsigned long flags;

	local_irq_save(flags);
	return flags;
}

void t21_irq_restore_310(unsigned long flags)
{
	local_irq_restore(flags);
}

void t21_kfree_skb_310(struct sk_buff *skb)
{
	kfree_skb(skb);
}

/* 3.10's PDE_DATA(inode) */
void *t21_pde_data_310(const struct inode *inode)
{
	return pde_data(inode);
}

/* the firmware inlines preempt_enable(); this kernel is not preemptible */
void t21_preempt_schedule_310(void)
{
	schedule();
}

/*
 * 3.10's seq_printf() returned 0 on success and -1 on overflow, and the
 * firmware's show() callbacks add those results up as their status. 7.x
 * returns void.
 */
int t21_seq_printf_310(struct seq_file *m, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	seq_vprintf(m, fmt, args);
	va_end(args);
	return seq_has_overflowed(m) ? -1 : 0;
}

int private_seq_printf(struct seq_file *m, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	seq_vprintf(m, fmt, args);
	va_end(args);
	return seq_has_overflowed(m) ? -1 : 0;
}

/*
 * ---- netlink ---------------------------------------------------------------
 *
 * The firmware's tuning channel is a unit-23 netlink socket. It keeps the
 * struct sock * the table returns and passes it back to netlink_unicast();
 * tisp_netlink_exit (tx-isp-fwc.c) releases sock->sk_socket. Its receive
 * callback reads only skb->len and skb->data, which sit at the same offsets
 * on 7.x. Only the config struct differs: 3.10's is 20 bytes {groups, flags,
 * input, cb_mutex, bind}.
 */
struct sk_buff *private_nlmsg_new(size_t payload, gfp_t flags)
{
	return nlmsg_new(payload, t21_gfp_from_310(flags));
}

struct nlmsghdr *private_nlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
				   int type, int payload, int flags)
{
	return nlmsg_put(skb, portid, seq, type, payload, flags);
}

int private_netlink_unicast(struct sock *ssk, struct sk_buff *skb, u32 portid,
			    int nonblock)
{
	return netlink_unicast(ssk, skb, portid, nonblock);
}

struct sock *private_netlink_kernel_create(struct net *net, int unit,
					   struct netlink_kernel_cfg *cfg310)
{
	struct netlink_kernel_cfg cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.groups = *(unsigned int *)((u8 *)cfg310 + 0);
	cfg.flags = *(unsigned int *)((u8 *)cfg310 + 4);
	cfg.input = *(void (**)(struct sk_buff *))((u8 *)cfg310 + 8);
	return netlink_kernel_create(&init_net, unit, &cfg);
}

void private_sock_release(struct socket *sock)
{
	sock_release(sock);
}

struct net *private_get_init_net(void)
{
	return &init_net;
}

/*
 * ---- vfs -------------------------------------------------------------------
 *
 * 7.x has no set_fs and vfs_read() insists on a __user buffer; the firmware
 * hands kernel buffers to these.
 */
struct file *private_filp_open(const char *filename, int flags, umode_t mode)
{
	return filp_open(filename, flags, mode);
}

int private_filp_close(struct file *filp, fl_owner_t id)
{
	return filp_close(filp, id);
}

ssize_t private_vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	return kernel_read(file, (void __force *)buf, count, pos);
}

ssize_t private_vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	return kernel_write(file, (const void __force *)buf, count, pos);
}

loff_t private_vfs_llseek(struct file *file, loff_t offset, int whence)
{
	return vfs_llseek(file, offset, whence);
}

/* 3.10's mm_segment_t is a one-member struct: returned through a hidden pointer */
mm_segment_t private_get_fs(void)
{
	mm_segment_t seg = { 0 };

	return seg;
}

void private_set_fs(mm_segment_t val)
{
}

void private_getrawmonotonic(struct timespec *ts)
{
	struct timespec64 t;

	ktime_get_raw_ts64(&t);
	ts->tv_sec = t.tv_sec;
	ts->tv_nsec = t.tv_nsec;
}

/*
 * ---- dma -------------------------------------------------------------------
 *
 * dma_cache_sync() is gone. The ISP memory the firmware syncs sits outside
 * the kernel's RAM (ispmem=, rmem=) and is mostly touched through KSEG1;
 * the DMA API needs a struct page, so only RAM gets the sync.
 */
static void t21_sync(struct device *dev, phys_addr_t paddr, size_t size,
		     enum dma_data_direction dir)
{
	struct device *d = t21_any_dev();

	if (!d || !pfn_valid(PHYS_PFN(paddr)))
		return;
	dma_sync_single_for_device(d, paddr, size, dir);
}

void t21_dma_cache_sync_310(struct device *dev, void *vaddr, size_t size,
			    enum dma_data_direction dir)
{
	unsigned long va = (unsigned long)vaddr;
	size_t off, chunk;

	/*
	 * A vmalloc address has no fixed physical alias: masking it gives an
	 * unrelated page. Sync such buffers page by page.
	 */
	if (is_vmalloc_addr(vaddr)) {
		for (off = 0; off < size; off += chunk) {
			struct page *pg = vmalloc_to_page(vaddr + off);

			chunk = min_t(size_t, size - off,
				      PAGE_SIZE - offset_in_page(va + off));
			if (pg)
				t21_sync(dev, page_to_phys(pg) +
					 offset_in_page(va + off), chunk, dir);
		}
		return;
	}
	if (va >= CKSEG0 && va < CKSEG2)
		t21_sync(dev, CPHYSADDR(va), size, dir);
}

void private_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
			    enum dma_data_direction dir)
{
	t21_dma_cache_sync_310(dev, vaddr, size, dir);
}

/* the firmware calls this one with a physical address */
void private_dma_sync_single_for_device(struct device *dev, dma_addr_t addr,
					size_t size, enum dma_data_direction dir)
{
	t21_sync(dev, addr, size, dir);
}
