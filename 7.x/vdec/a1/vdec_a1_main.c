// SPDX-License-Identifier: GPL-2.0
/*
 * A1 video decoder (VC8000D, /dev/vdec) on mainline 7.x: the vendor SDK 1.7.0
 * libvdec-firmware (built for the vendor 4.4.94 kernel, unmodified) plus this
 * adapter. See a1abi.h. The firmware takes its hardware addresses from its own
 * data and registers its platform device and misc device itself; the interrupt
 * number is the one thing set from here (it was a 4.4 number).
 */
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/of_irq.h>
#include <linux/semaphore.h>
#include <linux/thread_info.h>
#include <linux/wait.h>

/*
 * The firmware's command-queue driver requests 4.4's number for the decoder
 * interrupt (hardware interrupt 42 plus the 8 MIPS CPU lines); the devicetree
 * virq replaces it.
 */
static int vdec_virq;
#define A1_VDEC_LEGACY_IRQ	50
#define A1_IRQ_XLATE(i)		((i) == A1_VDEC_LEGACY_IRQ ? vdec_virq : (i))

#include "../../a1abi/a1abi.h"

/* the firmware's entry points and data */
extern int vdec_init(void);
extern void vdec_exit(void);
extern int irq[4];

/* soc-vpu-a1 */
extern void vpu_work_lock(void);

/* called by the firmware, which declares them itself */
void *a1_kmem_cache_alloc(void *cache, gfp_t gfp);
bool dma_alloc_from_coherent_a1(struct device *dev, ssize_t size, dma_addr_t *handle, void **ret);
int dma_release_from_coherent_a1(struct device *dev, int order, void *vaddr);
void __mutex_init_a1(struct mutex *m, const char *name, struct lock_class_key *key);
void mutex_lock_a1(struct mutex *m);
void mutex_unlock_a1(struct mutex *m);

/*
 * The firmware inlines kmalloc() of constant sizes as
 * kmem_cache_alloc(kmalloc_caches[idx], gfp) with 4.4's array of
 * 1 << idx byte caches (1: 96, 2: 192). Ours are the sizes.
 */
void *a1_kmalloc_caches[14] = {
	[1] = (void *)96, [2] = (void *)192,
	[3] = (void *)8, [4] = (void *)16, [5] = (void *)32, [6] = (void *)64,
	[7] = (void *)128, [8] = (void *)256, [9] = (void *)512,
	[10] = (void *)1024, [11] = (void *)2048, [12] = (void *)4096,
	[13] = (void *)8192,
};

void *a1_kmem_cache_alloc(void *cache, gfp_t gfp)
{
	return kzalloc((unsigned long)cache, GFP_KERNEL);
}

void __mutex_init_a1(struct mutex *m, const char *name, struct lock_class_key *key)
{
	private_raw_mutex_init(m, name, key);
}

void mutex_lock_a1(struct mutex *m)
{
	private_mutex_lock(m);
}

void mutex_unlock_a1(struct mutex *m)
{
	private_mutex_unlock(m);
}

/* ---- the device the firmware registers for its dma memory ----------------------- */
static struct platform_device *a1_vdec_pdev;

/* 4.4's inlined dma_alloc_coherent(): no per-device pool, then ops->alloc/free */
bool dma_alloc_from_coherent_a1(struct device *dev, ssize_t size, dma_addr_t *handle, void **ret)
{
	return false;
}

int dma_release_from_coherent_a1(struct device *dev, int order, void *vaddr)
{
	return 0;
}

struct a1_dma_ops {
	void *(*alloc)(struct device *dev, size_t size, dma_addr_t *handle,
		       gfp_t gfp, unsigned long attrs);
	void (*free)(struct device *dev, size_t size, void *vaddr,
		     dma_addr_t handle, unsigned long attrs);
};

static void *a1_dma_alloc(struct device *dev, size_t size, dma_addr_t *handle,
			  gfp_t gfp, unsigned long attrs)
{
	return dma_alloc_coherent(&a1_vdec_pdev->dev, size, handle, GFP_KERNEL);
}

static void a1_dma_free(struct device *dev, size_t size, void *vaddr,
			dma_addr_t handle, unsigned long attrs)
{
	dma_free_coherent(&a1_vdec_pdev->dev, size, vaddr, handle);
}

static struct a1_dma_ops a1_dma_ops_tab = { a1_dma_alloc, a1_dma_free };
struct a1_dma_ops *mips_dma_map_ops_a1 = &a1_dma_ops_tab;

void *private_dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp)
{
	return dma_alloc_coherent(&a1_vdec_pdev->dev, size, handle, GFP_KERNEL);
}

void private_dma_free_coherent(struct device *dev, size_t size, void *cpu, dma_addr_t handle)
{
	dma_free_coherent(&a1_vdec_pdev->dev, size, cpu, handle);
}

/*
 * 4.4's platform_device_info: parent (0), fwnode (4), name (8), id (12), res (16),
 * num_res (20), data (24), size_data (28), dma_mask (32). The device is
 * only the owner of the dma memory; its dev member is handed back where the
 * firmware (which adds 16 to the pointer) expects it.
 */
struct platform_device *private_platform_device_register_full(const void *info44)
{
	const char *name = *(const char **)((const char *)info44 + 8);
	int id = *(int *)((const char *)info44 + 12);

	a1_vdec_pdev = platform_device_alloc(name, id);
	if (!a1_vdec_pdev)
		return NULL;
	a1_vdec_pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	a1_vdec_pdev->dev.dma_mask = &a1_vdec_pdev->dev.coherent_dma_mask;
	if (platform_device_add(a1_vdec_pdev)) {
		platform_device_put(a1_vdec_pdev);
		a1_vdec_pdev = NULL;
		return NULL;
	}
	return (struct platform_device *)((char *)&a1_vdec_pdev->dev - 16);
}

void private_platform_device_unregister(struct platform_device *p)
{
	platform_device_unregister(a1_vdec_pdev);
	a1_vdec_pdev = NULL;
}

/* the firmware's platform driver is empty: it never binds to anything */
int private_platform_driver_register(void *drv44)
{
	return 0;
}

void private_platform_driver_unregister(void *drv44)
{
}

/* ---- the plain ones the vdec wrapper has and a1abi does not ---------------------- */
struct resource *private_request_mem_region(resource_size_t start, resource_size_t n,
					    const char *name)
{
	return request_mem_region(start, n, name);
}

void private_release_mem_region(resource_size_t start, resource_size_t n)
{
	release_mem_region(start, n);
}

void __iomem *private_ioremap_nocache(unsigned long offset, unsigned long size)
{
	return ioremap(offset, size);
}

unsigned int private_ioread32(void __iomem *addr)
{
	return readl(addr);
}

unsigned int private_readl(const volatile void __iomem *addr)
{
	return readl(addr);
}

void private_iowrite32(unsigned int val, void __iomem *addr)
{
	writel(val, addr);
}

void private_spin_lock_irqsave(spinlock_t *l, unsigned long *flags)
{
	spin_lock_irqsave(l, *flags);
}

void private_sema_init(struct semaphore *sem, int val)
{
	sema_init(sem, val);
}

void private_up(struct semaphore *sem)
{
	up(sem);
}

int private_down_interruptible(struct semaphore *sem)
{
	return down_interruptible(sem);
}

void private_atomic_inc(atomic_t *v)
{
	atomic_inc(v);
}

int private_atomic_read(atomic_t *v)
{
	return atomic_read(v);
}

void private_mdelay(unsigned long ms)
{
	mdelay(ms);
}

int private_printk(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	r = printk("%pV", &vaf);
	va_end(args);
	return r;
}

void private_clk_disable(struct clk *clk)
{
}

void private_init_waitqueue_head(wait_queue_head_t *q)
{
	init_waitqueue_head(q);
}

void private_wake_up_interruptible_all(wait_queue_head_t *q)
{
	wake_up_interruptible_all(q);
}

int private_wait_event_interruptible1(wait_queue_head_t *q, int (*state)(void *), void *data)
{
	return wait_event_interruptible(*q, state(data));
}

int private_wait_event_interruptible2(wait_queue_head_t *q, int (*state)(void *, void *),
				      void *d1, void *d2)
{
	return wait_event_interruptible(*q, state(d1, d2));
}

int private_wait_event_interruptible3(wait_queue_head_t *q, int (*state)(void *, void *, void *),
				      void *d1, void *d2, void *d3)
{
	return wait_event_interruptible(*q, state(d1, d2, d3));
}

int private_wait_event_interruptible4(wait_queue_head_t *q,
				      int (*state)(void *, void *, void *, void *),
				      void *d1, void *d2, void *d3, void *d4)
{
	return wait_event_interruptible(*q, state(d1, d2, d3, d4));
}

u32 ioread32_a1(const void __iomem *a) { return readl(a); }
void iowrite32_a1(u32 v, void __iomem *a) { writel(v, a); }
void __iomem *ioremap_a1(phys_addr_t p, unsigned long size, unsigned long flags)
{
	return ioremap(p, size);
}
void iounmap_a1(const volatile void __iomem *a) { iounmap((void __iomem *)a); }
void *vmalloc_a1(unsigned long size) { return vmalloc(size); }
void raw_spin_lock_a1(raw_spinlock_t *l) { raw_spin_lock(l); }
void raw_spin_unlock_a1(raw_spinlock_t *l) { raw_spin_unlock(l); }


/*
 * Replacements for the firmware's functions that read 4.4 core structures
 * (task_struct through the thread pointer, mm_struct, page tables, struct
 * page). Each follows the vendor function it replaces statement by
 * statement; the MMU bookkeeping they share with the firmware is reached
 * through its own data and helpers (vdec_* names, see the Kbuild). Nodes and
 * the g_mmu array are 32-bit word arrays with the firmware's layout.
 */
extern u32 *vdec_g_mmu;
extern u32 simple_map, map_shift, gBaseDDRHw;
extern void vdec_SplitFreeNode(int region, u32 **node, int pages);
extern void vdec_SMCreateNode(int region, u32 **node);
extern void vdec_InsertNode(int region, u32 **node);
extern void vdec_DeleteNode(u32 **node);
extern void vdec_RemoveNode(int region, u32 va, int pid);
extern void vdec_SMRemoveNode(int region, u32 va, int pid);

#define A1_MMU_ERR	((int)0xffff0001)

static struct mutex *mmu_lock(void)
{
	return (struct mutex *)vdec_g_mmu[0];
}

/* physical address (less the DDR base for user memory) of a virtual one */
int GetPhysicalAddress_a1(u32 va, u32 *out)
{
	struct page *page;
	u64 pa;

	if (va + 0x40000000 < 0x3ffe0000) {
		page = vmalloc_to_page((void *)va);
		out[1] = 0;
		out[0] = (page_to_pfn(page) << PAGE_SHIFT) | (va & 0xfff);
		return 0;
	}
	if (virt_addr_valid((void *)va)) {
		out[0] = va + 0x80000000;
		out[1] = 0;
		return 0;
	}
	if (get_user_pages_fast(va & PAGE_MASK, 1, 0, &page) != 1)
		return -19;
	pa = page_to_phys(page) | (va & 0xfff);
	put_page(page);
	pa -= gBaseDDRHw;
	out[0] = lower_32_bits(pa);
	out[1] = upper_32_bits(pa);
	return 0;
}

static int a1_region_phys(u32 lo, u32 hi)
{
	if (hi == 0 && lo < 0x40000000)
		return 0;
	if ((u32)(lo + 0xc0000000 < lo) + hi - 1 != 0 || lo + 0xc0000000 > 0x3fffffff) {
		if ((u32)(lo + 0x80000000 < lo) + hi - 1 == 0 && lo + 0x80000000 < 0x40000000)
			return 2;
		return 3;
	}
	return 1;
}

int MMUKernelMemNodeMap(u32 *p)
{
	u32 *gm, *node, *pt, i, addr, carry = 0;
	u32 pa = p[0], size = p[2], end = size + pa, pages = ((size - 1) >> 12) + 1;
	bool nov = size <= end;
	int region, pid = task_tgid_nr(current);

	mutex_lock_a1(mmu_lock());
	if (nov && end < 0x40000000)
		region = 0;
	else if (pa > 0x3fffffff && nov && end < 0x80000000)
		region = 1;
	else if (pa > 0x7fffffff)
		region = (nov && end <= 0xbfffffff) ? 2 : 3;
	else
		region = 3;
	gm = vdec_g_mmu;
	if (!simple_map) {
		node = (u32 *)((u32 *)gm[region * 12 + 0x14])[9];
		while (node && node[6] < pages)
			node = (u32 *)node[9];
		if (!node) {
			mutex_unlock_a1(mmu_lock());
			return -1;
		}
		vdec_SplitFreeNode(region, &node, pages);
		node[1] = p[0];
		node[7] = pid;
		node[8] = 0;
	} else {
		u32 t = map_shift + pa, lo = (t & 0x3fffff) >> 12;
		u32 hi22 = (u32)(t < map_shift) << 10 | t >> 22;

		vdec_SMCreateNode(region, &node);
		node[1] = p[0];
		node[4] = ((lo + pages) >> 10) + hi22;
		node[5] = (lo + pages) & 0x3ff;
		node[7] = pid;
		node[8] = 0;
		node[2] = hi22;
		node[3] = lo;
		node[6] = pages;
	}
	pt = (u32 *)gm[10];
	for (i = 0, addr = pa; i < pages; i++) {
		if (i) {
			u32 prev = addr;

			addr = prev + 0x1000;
			carry += addr < prev;
		}
		pt[node[2] * 0x400 + i + node[3]] = (carry * 0x10 & 0xff0) | (addr & 0xfffff000) | 5;
	}
	p[1] = node[2] << 22 | node[3] << 12;
	mutex_unlock_a1(mmu_lock());
	return 0;
}

int MMUKernelMemNodeUnmap(u32 *p)
{
	u32 *gm, *node, *head, *cur, *local, i;
	int pid = task_tgid_nr(current), region = 0;
	bool second = false;
	u32 pa;

	mutex_lock_a1(mmu_lock());
	gm = vdec_g_mmu;
	pa = p[0];
	if (pa < 0x40000000 ||
	    (pa + 0xc0000000 < pa && (region = 1, pa + 0xc0000000 < 0x40000000)) ||
	    (pa + 0x80000000 < pa && (region = 2, pa + 0x80000000 < 0x40000000))) {
		head = gm + region * 12;
	} else {
		region = 3;
		head = gm + 0x24;
	}
	for (node = (u32 *)((u32 *)head[simple_map ? 0x12 : 0x15])[9]; node; node = (u32 *)node[9])
		if (pa == node[1] && node[7] == pid)
			break;
	if (!node) {
		mutex_unlock_a1(mmu_lock());
		return -4;
	}
	for (i = 0; i < node[6]; i++)
		((u32 *)gm[10])[node[2] * 0x400 + i + node[3]] = 0;
	pa = p[0];
	cur = (u32 *)((u32 *)gm[region * 12 + (simple_map ? 0x12 : 0x15)])[9];
	local = cur;
	for (; cur; cur = (u32 *)cur[9], second = true) {
		if (cur[1] == pa && pid == cur[7]) {
			if (second)
				local = cur;
			if (simple_map)
				vdec_DeleteNode(&local);
			else
				vdec_InsertNode(region, &local);
			break;
		}
	}
	mutex_unlock_a1(mmu_lock());
	return 0;
}

/* bases: the firmware's four (core 0, core 1) register base pairs */
int MMUIoctl(int cmd, unsigned long file, void __user *arg, u32 *bases)
{
	u32 a[3], phys[2] = { 0, 0 }, *node, *gm, i, pages, *b;
	int pid = task_tgid_nr(current), region;

	for (i = 0; i < 8; i++)
		if (bases[i] && readl((void __iomem *)(bases[i] + 0x18)) >> 16 != 0x4d4d)
			return A1_MMU_ERR;

	if (cmd == (int)0xc0046d02) {
		if (copy_from_user(a, arg, 12))
			return A1_MMU_ERR;
		mutex_lock_a1(mmu_lock());
		GetPhysicalAddress_a1(a[0], phys);
		gm = vdec_g_mmu;
		region = a1_region_phys(phys[0], phys[1]);
		node = (u32 *)*(u32 *)(gm[region * 12 + (simple_map ? 0x12 : 0x15)] + 0x24);
		for (; node; node = (u32 *)node[9]) {
			if (node[0] != a[0] || node[7] != pid)
				continue;
			for (i = 0; i < node[6]; i++)
				((u32 *)gm[10])[node[2] * 0x400 + node[3] + i] = 0;
			if (simple_map)
				vdec_SMRemoveNode(region, a[0], pid);
			else
				vdec_RemoveNode(region, a[0], pid);
			break;
		}
		mutex_unlock_a1(mmu_lock());
		return 0;
	}
	if (cmd == (int)0xc0046d03) {
		if (copy_from_user(a, arg, 4))
			return A1_MMU_ERR;
		b = bases + a[0] * 2;
		mutex_lock_a1(mmu_lock());
		if (!b[0]) {
			pr_err("vdec: MMU flush: no such core\n");
		} else {
			writel(0x10, (void __iomem *)(b[0] + 0x184));
			writel(0, (void __iomem *)(b[0] + 0x184));
			if (b[1]) {
				writel(0x10, (void __iomem *)(b[1] + 0x184));
				writel(0, (void __iomem *)(b[1] + 0x184));
			}
		}
		mutex_unlock_a1(mmu_lock());
		return 0;
	}
	if (cmd != (int)0xc0046d01)
		return A1_MMU_ERR;

	if (copy_from_user(a, arg, 12))
		return A1_MMU_ERR;
	mutex_lock_a1(mmu_lock());
	GetPhysicalAddress_a1(a[0], phys);
	{
		u32 end = a[2] + phys[0], endhi = (end < a[2]) + phys[1];

		pages = ((a[2] - 1) >> 12) + 1;
		if (endhi == 0 && end < 0x40000000)
			region = 0;
		else if (phys[1] == 0 && phys[0] < 0x40000000)
			region = 3;
		else if (endhi == 0 && end < 0x80000000)
			region = 1;
		else if (phys[1] == 0 && phys[0] < 0x80000000)
			region = 3;
		else
			region = (endhi != 0 || end > 0xbfffffff) ? 3 : 2;
	}
	gm = vdec_g_mmu;
	if (simple_map) {
		u32 t, lo, hi22;

		for (node = (u32 *)gm[region * 12 + 0x12]; (node = (u32 *)node[9]);)
			if (a[0] == node[0])
				goto out;
		vdec_SMCreateNode(region, &node);
		node[0] = a[0];
		node[8] = file;
		node[7] = pid;
		node[6] = pages;
		t = map_shift + phys[0];
		lo = (t & 0x3fffff) >> 12;
		hi22 = ((u32)(t < map_shift) + phys[1]) * 0x400 | t >> 22;
		node[2] = hi22;
		node[3] = lo;
		node[4] = ((lo + pages) >> 10) + hi22;
		node[5] = (lo + pages) & 0x3ff;
	} else {
		node = (u32 *)gm[region * 12 + 0x14];
		do {
			node = (u32 *)node[9];
			if (!node)
				goto out;
		} while (node[6] < pages);
		vdec_SplitFreeNode(region, &node, pages);
		node[0] = a[0];
		node[8] = file;
		node[7] = pid;
	}
	for (i = 0; i < pages; i++) {
		GetPhysicalAddress_a1(a[0] + i * 0x1000, phys);
		((u32 *)gm[10])[node[2] * 0x400 + i + node[3]] =
			(phys[1] & 0xff) << 4 | (phys[0] & 0xfffff000) | 5;
	}
	a[1] = node[2] << 22 | node[3] << 12;
out:
	mutex_unlock_a1(mmu_lock());
	if (copy_to_user(arg, a, 12))
		return A1_MMU_ERR;
	return 0;
}

static int __init vdec_a1_init(void)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, "ingenic,a1-vc8000d");
	int virq = np ? irq_of_parse_and_map(np, 0) : 0;
	int ret;

	of_node_put(np);
	if (virq <= 0) {
		pr_err("vdec: no interrupt in the device tree\n");
		return -ENODEV;
	}
	vdec_virq = virq;
	irq[0] = virq;
	ret = vdec_init();
	if (ret)
		pr_err("vdec: firmware init failed (%d)\n", ret);
	return ret;
}

static void __exit vdec_a1_exit(void)
{
	vdec_exit();
	a1_procs_free();
}

module_init(vdec_a1_init);
module_exit(vdec_a1_exit);

MODULE_DESCRIPTION("A1 video decoder: vendor firmware 1.7.0 and its adapter for mainline 7.x");
MODULE_LICENSE("GPL");
