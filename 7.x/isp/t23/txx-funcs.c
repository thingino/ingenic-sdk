// SPDX-License-Identifier: GPL-2.0
/*
 * txx-funcs.c - translators for the T23 ISP firmware's 3.10 kernel calls
 *
 * Two kinds of entry point live here: the table members of tx-isp-abi.c
 * that need more than a one-line wrapper (netlink, vfs, kmalloc, dma), and
 * the t23_*_310 functions the Kbuild redirects the firmware's direct imports
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
#include <linux/clk.h>

#include <txx-funcs.h>
#include <tx-isp-debug.h>
#include "tx-isp-abi.h"

/*
 * The firmware was built with 4.4 gfp bits: GFP_KERNEL was 0x24000c0
 * (__GFP_RECLAIM at 0x2400000, __GFP_IO, __GFP_FS), GFP_ATOMIC
 * __GFP_HIGH | __GFP_ATOMIC (0x80000) | __GFP_KSWAPD_RECLAIM. The bit values
 * moved on 7.x, so rebuild the mask.
 */
#define G44_DMA		0x01u
#define G44_HIGHMEM	0x02u
#define G44_MOVABLE	0x08u
#define G44_HIGH	0x20u
#define G44_IO		0x40u
#define G44_FS		0x80u
#define G44_NOWARN	0x200u
#define G44_NORETRY	0x1000u
#define G44_ZERO	0x8000u
#define G44_DIRECT_RECLAIM	0x400000u
#define G44_KSWAPD_RECLAIM	0x2000000u

static gfp_t t23_gfp_from_310(gfp_t old)
{
	gfp_t new = 0;

	if (old & G44_DMA)
		new |= __GFP_DMA;
	if (old & G44_HIGHMEM)
		new |= __GFP_HIGHMEM;
	if (old & G44_MOVABLE)
		new |= __GFP_MOVABLE;
	if (old & G44_DIRECT_RECLAIM)
		new |= __GFP_DIRECT_RECLAIM;
	if (old & G44_KSWAPD_RECLAIM)
		new |= __GFP_KSWAPD_RECLAIM;
	if (old & G44_HIGH)
		new |= __GFP_HIGH;
	if (old & G44_IO)
		new |= __GFP_IO;
	if (old & G44_FS)
		new |= __GFP_FS;
	if (old & G44_NOWARN)
		new |= __GFP_NOWARN;
	if (old & G44_NORETRY)
		new |= __GFP_NORETRY;
	if (old & G44_ZERO)
		new |= __GFP_ZERO;
	return new;
}

unsigned long t23_jiffies_from_310(unsigned long j)
{
	return DIV_ROUND_UP(j * HZ, 100);
}

void *t23_priv_kmalloc(size_t s, gfp_t gfp)
{
	void *addr = kmalloc(s, t23_gfp_from_310(gfp));

	if (!addr)
		pr_err("%s: kmalloc(%zu) failed\n", __func__, s);
	return addr;
}

/*
 * The firmware inlines kmalloc() of constant sizes as
 * kmem_cache_alloc(kmalloc_caches[idx], gfp) with 3.10's array of
 * 1 << idx byte caches. 7.x has a 2-D array of caches that it does not
 * export in a usable shape, so the array is ours and its "caches" are the
 * sizes; t23_kmem_cache_alloc_310() turns them back into kmalloc().
 */
void *t23_kmalloc_caches[14] = {
	[3] = (void *)8, [4] = (void *)16, [5] = (void *)32, [6] = (void *)64,
	[7] = (void *)128, [8] = (void *)256, [9] = (void *)512,
	[10] = (void *)1024, [11] = (void *)2048, [12] = (void *)4096,
	[13] = (void *)8192,
};

void *t23_kmem_cache_alloc_310(void *cache, gfp_t gfp)
{
	return kmalloc((unsigned long)cache, t23_gfp_from_310(gfp));
}

/* targets of the objcopy --redefine-sym rewrites in the Kbuild */

unsigned long t23_irq_save_310(void)
{
	unsigned long flags;

	local_irq_save(flags);
	return flags;
}

void t23_irq_restore_310(unsigned long flags)
{
	local_irq_restore(flags);
}

static struct class *t23_classes[4];
static DEFINE_MUTEX(t23_class_lock);

/* 7.x dropped the owner and key arguments */
struct class *t23_class_create_310(struct module *owner, const char *name,
				   struct lock_class_key *key)
{
	struct class *c = class_create(name);
	int i;

	if (IS_ERR(c))
		return c;
	mutex_lock(&t23_class_lock);
	for (i = 0; i < ARRAY_SIZE(t23_classes); i++) {
		if (!t23_classes[i]) {
			t23_classes[i] = c;
			break;
		}
	}
	mutex_unlock(&t23_class_lock);
	return c;
}

static bool t23_class_take(struct class *c, bool drop)
{
	bool live = false;
	int i;

	mutex_lock(&t23_class_lock);
	for (i = 0; i < ARRAY_SIZE(t23_classes); i++) {
		if (c && t23_classes[i] == c) {
			live = true;
			if (drop)
				t23_classes[i] = NULL;
		}
	}
	mutex_unlock(&t23_class_lock);
	return live;
}

/* the firmware's tuning-node teardown runs twice (see t23_cdev_del_310) */
void t23_class_destroy_310(struct class *c)
{
	if (t23_class_take(c, true))
		class_destroy(c);
}

void t23_device_destroy_310(struct class *c, dev_t devt)
{
	if (t23_class_take(c, false))
		device_destroy(c, devt);
}

/* dev is the address of the firmware's own struct device */
void *t23_dev_get_drvdata_310(const struct device *dev)
{
	return t23_dev_get_drvdata(dev);
}

void t23_kfree_skb_310(struct sk_buff *skb)
{
	kfree_skb(skb);
}

/* 4.4's PDE_DATA(inode): the firmware's data behind tx-isp-twin.c's proc entry */
void *t23_pde_data_310(const struct inode *inode)
{
	return t23_proc_data(inode);
}

/* the firmware inlines might_fault()/might_sleep() into its user copies */
void t23_might_sleep_310(const char *file, int line, int preempt_offset)
{
}

/* the firmware inlines preempt_enable(); this kernel is not preemptible */
void t23_preempt_schedule_310(void)
{
	schedule();
}

signed long t23_schedule_timeout_310(signed long timeout)
{
	if (timeout > 0 && timeout != MAX_SCHEDULE_TIMEOUT)
		timeout = t23_jiffies_from_310(timeout);
	return schedule_timeout(timeout);
}

/*
 * 3.10's seq_printf() returned 0 on success and -1 on overflow, and the
 * firmware's show() callbacks add those results up as their status. 7.x
 * returns void.
 */
int t23_seq_printf_310(void *twin, const char *fmt, ...)
{
	struct seq_file *m = t23_seq_real(twin);
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
struct sk_buff *t23_priv_nlmsg_new(size_t payload, gfp_t flags)
{
	return nlmsg_new(payload, t23_gfp_from_310(flags));
}

struct nlmsghdr *t23_priv_nlmsg_put(struct sk_buff *skb, u32 portid, u32 seq,
				    int type, int payload, int flags)
{
	return nlmsg_put(skb, portid, seq, type, payload, flags);
}

struct sock *t23_priv_netlink_kernel_create(struct net *net, int unit,
					    struct netlink_kernel_cfg *cfg310)
{
	struct netlink_kernel_cfg cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.groups = *(unsigned int *)((u8 *)cfg310 + 0);
	cfg.flags = *(unsigned int *)((u8 *)cfg310 + 4);
	cfg.input = *(void (**)(struct sk_buff *))((u8 *)cfg310 + 8);
	return netlink_kernel_create(&init_net, unit, &cfg);
}

void t23_priv_sock_release(struct socket *sock)
{
	sock_release(sock);
}

struct net *t23_priv_get_init_net(void)
{
	return &init_net;
}

/*
 * ---- vfs -------------------------------------------------------------------
 *
 * The firmware reads its IQ file into a kernel buffer under
 * set_fs(KERNEL_DS); 7.x has no set_fs. The file handle it works with is
 * tx-isp-twin.c's 4.4-shaped twin, so get_fs/set_fs have nothing to do.
 */
unsigned long t23_priv_get_fs(void)
{
	return 0;
}

void t23_priv_set_fs(unsigned long val)
{
}

void t23_priv_getrawmonotonic(struct timespec *ts)
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
static void t23_sync(struct device *dev, phys_addr_t paddr, size_t size,
		     enum dma_data_direction dir)
{
	struct device *d = t23_any_dev();

	if (!d || !pfn_valid(PHYS_PFN(paddr)))
		return;
	dma_sync_single_for_device(d, paddr, size, dir);
}

void t23_priv_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
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
				t23_sync(dev, page_to_phys(pg) +
					 offset_in_page(va + off), chunk, dir);
		}
		return;
	}
	if (va >= CKSEG0 && va < CKSEG2)
		t23_sync(dev, CPHYSADDR(va), size, dir);
}

/* weak hook the firmware calls with a physical address */
void private_dma_sync_single_for_device(struct device *dev, dma_addr_t addr,
					size_t size, enum dma_data_direction dir)
{
	t23_sync(dev, addr, size, dir);
}

/*
 * ---- clocks ------------------------------------------------------------------
 *
 * ispcore_set_clk_parent() asks for "mpll"/"sclka" and re-parents the ISP
 * clock. Parents are chosen by the CGU driver when it sets the rate, and the
 * names are not registered as clkdev aliases, so a missing parent is not an
 * error and the re-parenting is a no-op.
 */
struct clk *t23_clk_get_44(struct device *dev, const char *id)
{
	/* a firmware-owned dev would be read past the end of its object */
	return clk_get(NULL, id);
}

int t23_clk_set_parent_44(struct clk *clk, struct clk *parent)
{
	if (IS_ERR_OR_NULL(parent))
		return 0;
	return clk_set_parent(clk, parent);
}
