// SPDX-License-Identifier: GPL-2.0
/*
 * txx-funcs.c - translators for the T41 ISP firmware's 4.4 kernel calls
 *
 * Two kinds of entry point live here: the private_* entry points of tx-isp-abi.c
 * that need more than a one-line wrapper (vfs, kmalloc, dma), and
 * the t41_*_44 functions the Kbuild redirects the firmware's direct imports
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
#include <linux/irqflags.h>
#include <linux/pfn.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/timekeeping.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <net/net_namespace.h>
#include <linux/clk.h>

#include <txx-funcs.h>
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

static gfp_t t41_gfp_from_44(gfp_t old)
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

unsigned long t41_jiffies_from_44(unsigned long j)
{
	return DIV_ROUND_UP(j * HZ, 100);
}

void *t41_priv_kmalloc(size_t s, gfp_t gfp)
{
	void *addr = kmalloc(s, t41_gfp_from_44(gfp));

	if (!addr)
		pr_err("%s: kmalloc(%zu) failed\n", __func__, s);
	return addr;
}

/*
 * The firmware inlines kmalloc() of constant sizes as
 * kmem_cache_alloc(kmalloc_caches[idx], gfp) with 3.10's array of
 * 1 << idx byte caches. 7.x has a 2-D array of caches that it does not
 * export in a usable shape, so the array is ours and its "caches" are the
 * sizes; t41_kmem_cache_alloc_44() turns them back into kmalloc().
 */
void *t41_kmalloc_caches[14] = {
	[3] = (void *)8, [4] = (void *)16, [5] = (void *)32, [6] = (void *)64,
	[7] = (void *)128, [8] = (void *)256, [9] = (void *)512,
	[10] = (void *)1024, [11] = (void *)2048, [12] = (void *)4096,
	[13] = (void *)8192,
};

void *t41_kmem_cache_alloc_44(void *cache, gfp_t gfp)
{
	return kmalloc((unsigned long)cache, t41_gfp_from_44(gfp));
}

/* targets of the objcopy --redefine-sym rewrites in the Kbuild */

static struct class *t41_classes[4];
static DEFINE_MUTEX(t41_class_lock);

/* 7.x dropped the owner and key arguments */
struct class *t41_class_create_44(struct module *owner, const char *name,
				   struct lock_class_key *key)
{
	struct class *c = class_create(name);
	int i;

	if (IS_ERR(c))
		return c;
	mutex_lock(&t41_class_lock);
	for (i = 0; i < ARRAY_SIZE(t41_classes); i++) {
		if (!t41_classes[i]) {
			t41_classes[i] = c;
			break;
		}
	}
	mutex_unlock(&t41_class_lock);
	return c;
}

static bool t41_class_take(struct class *c, bool drop)
{
	bool live = false;
	int i;

	mutex_lock(&t41_class_lock);
	for (i = 0; i < ARRAY_SIZE(t41_classes); i++) {
		if (c && t41_classes[i] == c) {
			live = true;
			if (drop)
				t41_classes[i] = NULL;
		}
	}
	mutex_unlock(&t41_class_lock);
	return live;
}

/* the firmware's tuning-node teardown runs twice (see t41_cdev_del_44) */
void t41_class_destroy_44(struct class *c)
{
	if (t41_class_take(c, true))
		class_destroy(c);
}

void t41_device_destroy_44(struct class *c, dev_t devt)
{
	if (t41_class_take(c, false))
		device_destroy(c, devt);
}

/* dev is the address of the firmware's own struct device */
void *t41_dev_get_drvdata_44(const struct device *dev)
{
	return t41_dev_get_drvdata(dev);
}

/* 4.4's PDE_DATA(inode): the firmware's data behind tx-isp-twin.c's proc entry */
void *t41_pde_data_44(const struct inode *inode)
{
	return t41_proc_data(inode);
}

/* the firmware inlines might_fault()/might_sleep() into its user copies */
void t41_might_sleep_44(const char *file, int line, int preempt_offset)
{
}

/* the firmware inlines preempt_enable(); this kernel is not preemptible */
void t41_preempt_schedule_44(void)
{
	schedule();
}

signed long t41_schedule_timeout_44(signed long timeout)
{
	if (timeout > 0 && timeout != MAX_SCHEDULE_TIMEOUT)
		timeout = t41_jiffies_from_44(timeout);
	return schedule_timeout(timeout);
}

/*
 * 3.10's seq_printf() returned 0 on success and -1 on overflow, and the
 * firmware's show() callbacks add those results up as their status. 7.x
 * returns void.
 */
int t41_seq_printf_44(void *twin, const char *fmt, ...)
{
	struct seq_file *m = t41_seq_real(twin);
	va_list args;

	va_start(args, fmt);
	seq_vprintf(m, fmt, args);
	va_end(args);
	return seq_has_overflowed(m) ? -1 : 0;
}

/* ---- net ---- */
struct net *t41_priv_get_init_net(void)
{
	return &init_net;
}

/*
 * ---- dma -------------------------------------------------------------------
 *
 * dma_cache_sync() is gone. The ISP memory the firmware syncs sits outside
 * the kernel's RAM (ispmem=, rmem=) and is mostly touched through KSEG1;
 * the DMA API needs a struct page, so only RAM gets the sync.
 */
static void t41_sync(struct device *dev, phys_addr_t paddr, size_t size,
		     enum dma_data_direction dir)
{
	struct device *d = t41_any_dev();

	if (!d || !pfn_valid(PHYS_PFN(paddr)))
		return;
	dma_sync_single_for_device(d, paddr, size, dir);
}

void t41_priv_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
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
				t41_sync(dev, page_to_phys(pg) +
					 offset_in_page(va + off), chunk, dir);
		}
		return;
	}
	if (va >= CKSEG0 && va < CKSEG2)
		t41_sync(dev, CPHYSADDR(va), size, dir);
}

/* weak hook the firmware calls with a physical address */
void private_dma_sync_single_for_device(struct device *dev, dma_addr_t addr,
					size_t size, enum dma_data_direction dir)
{
	t41_sync(dev, addr, size, dir);
}

/*
 * ---- clocks ------------------------------------------------------------------
 *
 * ispcore_set_clk_parent() asks for "mpll"/"sclka" and re-parents the ISP
 * clock. Parents are chosen by the CGU driver when it sets the rate, and the
 * names are not registered as clkdev aliases, so a missing parent is not an
 * error and the re-parenting is a no-op.
 */
struct clk *t41_clk_get_44(struct device *dev, const char *id)
{
	/* a firmware-owned dev would be read past the end of its object */
	return clk_get(NULL, id);
}

int t41_clk_set_parent_44(struct clk *clk, struct clk *parent)
{
	if (IS_ERR_OR_NULL(parent))
		return 0;
	return clk_set_parent(clk, parent);
}

/*
 * ---- direct imports with a changed shape ----------------------------------
 */

/*
 * The firmware's only __ioremap() caller is the isp_save_cmd_set debug
 * command, and 7.x has no such function; the mapping is refused and the
 * command reports the failure.
 */
void __iomem *t41___ioremap_44(phys_addr_t phys, unsigned long size, unsigned long flags)
{
	return NULL;
}

/* sched_setscheduler() is not exported on 7.x; the firmware's two kthreads ask for FIFO */
int t41_sched_setscheduler_44(struct task_struct *p, int policy,
			      const struct sched_param *param)
{
	struct sched_attr attr = {
		.sched_policy = policy,
		.sched_priority = param->sched_priority,
	};

	return sched_setattr_nocheck(p, &attr);
}

int t41_single_open_44(struct file *twin, int (*show)(struct seq_file *, void *), void *data)
{
	return t41_single_open_size(twin, show, data, 0);
}

/*
 * The firmware's INIT_WORK() ran inline with 4.4's data word (no pool, nothing
 * pending); 7.x keeps different flag bits in it. The first queueing of a work
 * resets the word, later ones find the pending bit or a live 7.x state.
 */
#define WORK44_NO_POOL		0xffffffe0u

bool t41_queue_work_44(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);

	if (data == WORK44_NO_POOL)
		atomic_long_set(&work->data, (unsigned long)WORK_STRUCT_NO_POOL);
	return queue_work(system_percpu_wq, work);
}

/* the firmware only ever passes system_wq, now deprecated */
bool t41_queue_work_on_44(int cpu, struct workqueue_struct *wq, struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);

	if (data == WORK44_NO_POOL)
		atomic_long_set(&work->data, (unsigned long)WORK_STRUCT_NO_POOL);
	return queue_work_on(cpu, system_percpu_wq, work);
}

/* the vendor kernel's rmem carve-out, which the firmware range-checks buffers against */
unsigned long t41_rmem_base;
unsigned long t41_rmem_size;


char *t41_strncpy(char *dst, const char *src, size_t n)
{
	size_t len = strnlen(src, n);

	memcpy(dst, src, len);
	memset(dst + len, 0, n - len);
	return dst;
}

void t41_arch_sync_dma_for_device(phys_addr_t paddr, size_t size, enum dma_data_direction dir)
{
	t41_sync(NULL, paddr, size, dir);
}

void t41_dma_sync_single_for_device(struct device *dev, dma_addr_t addr, size_t size,
				    enum dma_data_direction dir)
{
	t41_sync(dev, addr, size, dir);
}

/* the kernel-side encoder entry the firmware calls only with zeratul_open set */
int VPU_Encoder_SendFrame(int chn, void *frame)
{
	return -ENODEV;
}

/*
 * The firmware wraps its kernel-buffer file reads in set_fs(KERNEL_DS); the
 * file handle it works with is tx-isp-twin.c's twin, which reads into kernel
 * memory regardless.
 */
mm_segment_t private_get_fs(void)
{
	return 0;
}

void private_set_fs(mm_segment_t val)
{
}

/* MIPS32 7.x has no __div64_32 symbol, do_div() is inline */
u32 t41___div64_32(u64 *n, u32 base)
{
	return do_div(*n, base);
}

/*
 * The firmware's inlined port accesses add a physical address to
 * mips_io_port_base, which 4.4's MIPS kept at KSEG1; 7.x leaves it 0.
 */
unsigned long t41_mips_io_port_base = 0xa0000000;
