// SPDX-License-Identifier: GPL-2.0
/*
 * txx-funcs.c - translators for the T31 ISP firmware's 3.10 kernel calls
 *
 * Two kinds of entry point live here: the table members of tx-isp-abi.c
 * that need more than a one-line wrapper (netlink, vfs, kmalloc, dma), and
 * the t31_*_310 functions the Kbuild redirects the firmware's direct imports
 * to with objcopy.
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

static gfp_t t31_gfp_from_310(gfp_t old)
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

unsigned long t31_jiffies_from_310(unsigned long j)
{
	return DIV_ROUND_UP(j * HZ, 100);
}

void *t31_priv_kmalloc(size_t s, gfp_t gfp)
{
	void *addr = kmalloc(s, t31_gfp_from_310(gfp));

	if (!addr)
		pr_err("%s: kmalloc(%zu) failed\n", __func__, s);
	return addr;
}

/*
 * The firmware inlines kmalloc() of constant sizes as
 * kmem_cache_alloc(kmalloc_caches[idx], gfp) with 3.10's array of
 * 1 << idx byte caches. 7.x has a 2-D array of caches that it does not
 * export in a usable shape, so the array is ours and its "caches" are the
 * sizes; t31_kmem_cache_alloc_310() turns them back into kmalloc().
 */
void *t31_kmalloc_caches[14] = {
	[3] = (void *)8, [4] = (void *)16, [5] = (void *)32, [6] = (void *)64,
	[7] = (void *)128, [8] = (void *)256, [9] = (void *)512,
	[10] = (void *)1024, [11] = (void *)2048, [12] = (void *)4096,
	[13] = (void *)8192,
};

void *t31_kmem_cache_alloc_310(void *cache, gfp_t gfp)
{
	return kmalloc((unsigned long)cache, t31_gfp_from_310(gfp));
}

/* targets of the objcopy --redefine-sym rewrites in the Kbuild */

unsigned long t31_irq_save_310(void)
{
	unsigned long flags;

	local_irq_save(flags);
	return flags;
}

void t31_irq_restore_310(unsigned long flags)
{
	local_irq_restore(flags);
}

static struct class *t31_classes[4];
static DEFINE_MUTEX(t31_class_lock);

/* 7.x dropped the owner and key arguments */
struct class *t31_class_create_310(struct module *owner, const char *name,
				   struct lock_class_key *key)
{
	struct class *c = class_create(name);
	int i;

	if (IS_ERR(c))
		return c;
	mutex_lock(&t31_class_lock);
	for (i = 0; i < ARRAY_SIZE(t31_classes); i++) {
		if (!t31_classes[i]) {
			t31_classes[i] = c;
			break;
		}
	}
	mutex_unlock(&t31_class_lock);
	return c;
}

static bool t31_class_take(struct class *c, bool drop)
{
	bool live = false;
	int i;

	mutex_lock(&t31_class_lock);
	for (i = 0; i < ARRAY_SIZE(t31_classes); i++) {
		if (c && t31_classes[i] == c) {
			live = true;
			if (drop)
				t31_classes[i] = NULL;
		}
	}
	mutex_unlock(&t31_class_lock);
	return live;
}

/* the firmware's tuning-node teardown runs twice (see t31_cdev_del_310) */
void t31_class_destroy_310(struct class *c)
{
	if (t31_class_take(c, true))
		class_destroy(c);
}

void t31_device_destroy_310(struct class *c, dev_t devt)
{
	if (t31_class_take(c, false))
		device_destroy(c, devt);
}

/* dev is the address of the firmware's own struct device */
void *t31_dev_get_drvdata_310(const struct device *dev)
{
	return t31_dev_get_drvdata(dev);
}

void t31_kfree_skb_310(struct sk_buff *skb)
{
	kfree_skb(skb);
}

/* 3.10's PDE_DATA(inode) */
void *t31_pde_data_310(const struct inode *inode)
{
	return pde_data(inode);
}

/* the firmware inlines might_fault()/might_sleep() into its user copies */
void t31_might_sleep_310(const char *file, int line, int preempt_offset)
{
}

/* the firmware inlines preempt_enable(); this kernel is not preemptible */
void t31_preempt_schedule_310(void)
{
	schedule();
}

signed long t31_schedule_timeout_310(signed long timeout)
{
	if (timeout > 0 && timeout != MAX_SCHEDULE_TIMEOUT)
		timeout = t31_jiffies_from_310(timeout);
	return schedule_timeout(timeout);
}

/*
 * 3.10's seq_printf() returned 0 on success and -1 on overflow, and the
 * firmware's show() callbacks add those results up as their status. 7.x
 * returns void.
 */
int t31_seq_printf_310(struct seq_file *m, const char *fmt, ...)
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
 * on 7.x. Only the
 * config struct differs: 3.10's is 20 bytes {groups, flags, input, cb_mutex,
 * bind}.
 */
struct sk_buff *t31_priv_nlmsg_new(size_t payload, gfp_t flags)
{
	return nlmsg_new(payload, t31_gfp_from_310(flags));
}

struct nlmsghdr *t31_priv_nlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
				    int type, int payload, int flags)
{
	return nlmsg_put(skb, portid, seq, type, payload, flags);
}

struct sock *t31_priv_netlink_kernel_create(struct net *net, int unit,
					    struct netlink_kernel_cfg *cfg310)
{
	struct netlink_kernel_cfg cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.groups = *(unsigned int *)((u8 *)cfg310 + 0);
	cfg.flags = *(unsigned int *)((u8 *)cfg310 + 4);
	cfg.input = *(void (**)(struct sk_buff *))((u8 *)cfg310 + 8);
	return netlink_kernel_create(&init_net, unit, &cfg);
}

void t31_priv_sock_release(struct socket *sock)
{
	sock_release(sock);
}

struct net *t31_priv_get_init_net(void)
{
	return &init_net;
}

/*
 * ---- vfs -------------------------------------------------------------------
 *
 * The firmware reads its IQ file into a kernel buffer under
 * set_fs(KERNEL_DS) and vfs_read(); 7.x has no set_fs, and vfs_read() would
 * now insist on a __user buffer.
 */
ssize_t t31_priv_vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	return kernel_read(file, (void __force *)buf, count, pos);
}

ssize_t t31_priv_vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	return kernel_write(file, (const void __force *)buf, count, pos);
}

unsigned long t31_priv_get_fs(void)
{
	return 0;
}

void t31_priv_set_fs(unsigned long val)
{
}

void t31_priv_getrawmonotonic(struct timespec *ts)
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
static void t31_sync(struct device *dev, phys_addr_t paddr, size_t size,
		     enum dma_data_direction dir)
{
	struct device *d = t31_any_dev();

	if (!d || !pfn_valid(PHYS_PFN(paddr)))
		return;
	dma_sync_single_for_device(d, paddr, size, dir);
}

void t31_priv_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
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
				t31_sync(dev, page_to_phys(pg) +
					 offset_in_page(va + off), chunk, dir);
		}
		return;
	}
	if (va >= CKSEG0 && va < CKSEG2)
		t31_sync(dev, CPHYSADDR(va), size, dir);
}

/* weak hook the firmware calls with a physical address */
void private_dma_sync_single_for_device(struct device *dev, dma_addr_t addr,
					size_t size, enum dma_data_direction dir)
{
	t31_sync(dev, addr, size, dir);
}
