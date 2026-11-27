// SPDX-License-Identifier: GPL-2.0
/*
 * soc-nna.ko: /dev/soc-nna for the T40, T41 and A1 NNA, after the vendor
 * SDK's drivers/soc-nna (T40 1.3.1, T41 1.2.6, A1 1.7.0; one source with
 * per-SoC constants).
 *
 * The NNA itself is driven from user space: MXU3 NN instructions on the
 * CPU, and the NN DMA through mappings of this device. The driver only
 * hands out those mappings (the DMA and descriptor window, the ORAM, the
 * nmem= carve-out and buffers it allocates), keeps the caches of user
 * ranges coherent and reports the SoC's NNA version.
 */
#include <linux/bitfield.h>
#include <linux/cpuhotplug.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#include <linux/smp.h>
#include <linux/uaccess.h>
#include <asm/mipsregs.h>

#include "rmem_cache.h"
#include "soc_nna.h"

#define CCU_MSCR		0x60
#define CCU_MSCR_L2_SIZE	GENMASK(12, 10)	/* 1: 128 KiB ... 4: 1 MiB */
#define CCU_MSCR_ORAM_CLK	BIT(5)

/*
 * The NN DMA waits are RDHWR $8 and up (libvenus reads $8 to $13); user
 * space may only read them with their HWREna bits set, on every CPU.
 */
#define SOC_NNA_HWRENA		GENMASK(15, 8)

struct soc_nna_info {
	u32 version;		/* what IOCTL_SOC_NNA_VERSION reports */
	bool ddr_qos;		/* T41: DDRC port setting on open */
	phys_addr_t oram_fixed;	/* where the SoC's NNA library expects the ORAM */
};

struct soc_nna_region {
	phys_addr_t base;
	phys_addr_t size;
};

struct soc_nna_mem {
	struct list_head list;
	void *vaddr;
	dma_addr_t paddr;
	size_t size;
};

struct soc_nna {
	struct device *dev;
	const struct soc_nna_info *info;
	struct miscdevice mdev;
	struct soc_nna_region nndma;
	struct soc_nna_region sram;
	void __iomem *ccu;
	struct mutex lock;		/* refcnt, mem */
	int refcnt;
	struct list_head mem;
	enum cpuhp_state hp_state;
};

static int soc_nna_hwrena_set(unsigned int cpu)
{
	write_c0_hwrena(read_c0_hwrena() | SOC_NNA_HWRENA);
	return 0;
}

static void soc_nna_hwrena_clear(void *unused)
{
	write_c0_hwrena(read_c0_hwrena() & ~SOC_NNA_HWRENA);
}

/* the ORAM is the SRAM above the L2's share of it, as CCU MSCR sets it now */
static struct soc_nna_region soc_nna_oram(struct soc_nna *nna)
{
	struct soc_nna_region r = { };
	u32 f = FIELD_GET(CCU_MSCR_L2_SIZE, readl(nna->ccu + CCU_MSCR));
	phys_addr_t l2 = f ? (phys_addr_t)SZ_64K << f : 0;

	if (f && l2 < nna->sram.size) {
		r.base = nna->sram.base + l2;
		r.size = nna->sram.size - l2;
	}
	return r;
}

static int soc_nna_open(struct inode *inode, struct file *file)
{
	struct soc_nna *nna = container_of(file->private_data, struct soc_nna, mdev);

	mutex_lock(&nna->lock);
	if (nna->refcnt == 0 && nna->info->oram_fixed) {
		struct soc_nna_region oram = soc_nna_oram(nna);

		if (oram.base != nna->info->oram_fixed)
			dev_warn(nna->dev, "ORAM at %pa, the NNA library expects %pa: set the L2 split in the boot loader\n",
				 &oram.base, &nna->info->oram_fixed);
	}
	if (nna->refcnt++ == 0 && nna->info->ddr_qos) {
		/* the T41 vendor driver's DDR controller port setting */
		void __iomem *ddrc = ioremap(0x13012038, 4);

		if (ddrc) {
			/* the vendor brackets the write with syncs */
			wmb();
			writel(0x88404002, ddrc);
			wmb();	/* as above */
			iounmap(ddrc);
		}
	}
	mutex_unlock(&nna->lock);
	return 0;
}

static int soc_nna_release(struct inode *inode, struct file *file)
{
	struct soc_nna *nna = container_of(file->private_data, struct soc_nna, mdev);
	struct soc_nna_mem *m, *n;

	mutex_lock(&nna->lock);
	if (nna->refcnt > 0 && --nna->refcnt == 0) {
		/* as the vendor driver: the last close frees every buffer */
		list_for_each_entry_safe(m, n, &nna->mem, list) {
			list_del(&m->list);
			dma_free_coherent(nna->dev, m->size, m->vaddr, m->paddr);
			kfree(m);
		}
	}
	mutex_unlock(&nna->lock);
	return 0;
}

static long soc_nna_malloc(struct soc_nna *nna, void __user *arg)
{
	struct soc_nna_buf buf;
	struct soc_nna_mem *m;

	if (copy_from_user(&buf, arg, sizeof(buf)))
		return -EFAULT;
	if (buf.size <= 0)
		return -EINVAL;

	m = kzalloc_obj(*m, GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->size = PAGE_ALIGN(buf.size);
	m->vaddr = dma_alloc_coherent(nna->dev, m->size, &m->paddr, GFP_KERNEL);
	if (!m->vaddr) {
		kfree(m);
		return -ENOMEM;
	}

	buf.vaddr = (u32)(unsigned long)m->vaddr;
	buf.paddr = m->paddr;
	buf.size = m->size;
	if (copy_to_user(arg, &buf, sizeof(buf))) {
		dma_free_coherent(nna->dev, m->size, m->vaddr, m->paddr);
		kfree(m);
		return -EFAULT;
	}

	mutex_lock(&nna->lock);
	list_add_tail(&m->list, &nna->mem);
	mutex_unlock(&nna->lock);
	return 0;
}

static long soc_nna_free(struct soc_nna *nna, void __user *arg)
{
	struct soc_nna_buf buf;
	struct soc_nna_mem *m;
	long ret = -EINVAL;

	if (copy_from_user(&buf, arg, sizeof(buf)))
		return -EFAULT;

	mutex_lock(&nna->lock);
	list_for_each_entry(m, &nna->mem, list) {
		if ((u32)(unsigned long)m->vaddr == buf.vaddr && m->paddr == buf.paddr) {
			list_del(&m->list);
			dma_free_coherent(nna->dev, m->size, m->vaddr, m->paddr);
			kfree(m);
			ret = 0;
			break;
		}
	}
	mutex_unlock(&nna->lock);
	return ret;
}

/*
 * Cache maintenance on a range of the caller's mappings: the pages behind
 * this device's mappings are found through their page tables, ordinary
 * memory through its pages, uncached mappings need none. Holes and pages
 * that cannot be resolved are skipped rather than failing the call: the
 * vendor's dma_cache_sync() never failed, and libvenus asks for ranges
 * that run past its own mappings.
 */
static int soc_nna_sync_user(unsigned long addr, size_t len,
			     enum dma_data_direction dir)
{
	struct mm_struct *mm = current->mm;
	unsigned long end = addr + len;
	int ret = 0;

	if (end < addr || !mm)
		return -EINVAL;

	mmap_read_lock(mm);
	while (addr < end && !ret) {
		struct vm_area_struct *vma = find_vma(mm, addr);
		unsigned long stop, off, n;

		if (!vma)
			break;
		if (vma->vm_start > addr) {
			pr_debug("soc-nna: flush skips the hole %#lx-%#lx\n", addr,
				 min(end, vma->vm_start));
			addr = vma->vm_start;
			continue;
		}
		stop = min(end, vma->vm_end);
		if (rmem_cache_vma_uncached(vma)) {
			__sync();
			addr = stop;
			continue;
		}

		for (; addr < stop && !ret; addr += n) {
			off = addr & ~PAGE_MASK;
			n = min(stop - addr, PAGE_SIZE - off);

			if (vma->vm_flags & VM_PFNMAP) {
				struct follow_pfnmap_args args = {
					.vma = vma,
					.address = addr,
				};

				if (follow_pfnmap_start(&args))
					continue;
				ret = rmem_cache_sync_phys(PFN_PHYS(args.pfn) + off, n, dir);
				follow_pfnmap_end(&args);
			} else {
				struct page *page;

				if (pin_user_pages(addr, 1, 0, &page) != 1)
					continue;
				ret = rmem_cache_sync_phys(page_to_phys(page) + off, n, dir);
				unpin_user_page(page);
			}
		}
	}
	mmap_read_unlock(mm);
	return ret;
}

static long soc_nna_flushcache(void __user *arg)
{
	struct soc_nna_flush_cache_info info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;
	if (!info.addr || !info.len)
		return -EINVAL;
	/* as the vendor's dma_cache_sync(): anything else writes back too (NNA_DMA_NONE is 3) */
	if (info.dir != DMA_TO_DEVICE && info.dir != DMA_FROM_DEVICE)
		info.dir = DMA_BIDIRECTIONAL;

	return soc_nna_sync_user(info.addr, info.len, info.dir);
}

static long soc_nna_version(struct soc_nna *nna, void __user *arg)
{
	struct soc_nna_version v;

	if (copy_from_user(&v, arg, sizeof(v)))
		return -EFAULT;
	v.version = nna->info->version;
	return copy_to_user(arg, &v, sizeof(v)) ? -EFAULT : 0;
}

static long soc_nna_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct soc_nna *nna = container_of(file->private_data, struct soc_nna, mdev);
	void __user *uarg = (void __user *)arg;

	switch (cmd) {
	case IOCTL_SOC_NNA_MALLOC:
		return soc_nna_malloc(nna, uarg);
	case IOCTL_SOC_NNA_FREE:
		return soc_nna_free(nna, uarg);
	case IOCTL_SOC_NNA_FLUSHCACHE:
		return soc_nna_flushcache(uarg);
	case IOCTL_SOC_NNA_VERSION:
		return soc_nna_version(nna, uarg);
	case IOCTL_SOC_NNA_SETUP_DES:
	case IOCTL_SOC_NNA_RDCH_START:
	case IOCTL_SOC_NNA_WRCH_START:
		/*
		 * The vendor's descriptor builder: no NNA library calls it (they
		 * write the descriptors themselves) and it describes an older
		 * DMA layout than the silicon's.
		 */
		dev_warn_once(nna->dev, "descriptor ioctl %#x not supported\n", cmd);
		return -ENOTTY;
	default:
		return -ENOTTY;
	}
}

/* page by page, the window repeats the region (the vendor's "extended" maps) */
static int soc_nna_map_alias(struct vm_area_struct *vma, phys_addr_t start,
			     const struct soc_nna_region *r)
{
	unsigned long len = vma->vm_end - vma->vm_start, done = 0;
	phys_addr_t off;

	/* offset of the window's start inside the region, modulo its size */
	if (start >= r->base)
		off = (start - r->base) % r->size;
	else
		off = (r->size - (r->base - start) % r->size) % r->size;

	while (done < len) {
		unsigned long n = min_t(unsigned long, len - done, r->size - off);

		if (remap_pfn_range(vma, vma->vm_start + done,
				    PHYS_PFN(r->base + off), n, vma->vm_page_prot))
			return -EAGAIN;
		done += n;
		off = 0;
	}
	return 0;
}

static bool soc_nna_touches(phys_addr_t start, phys_addr_t end,
			    const struct soc_nna_region *r)
{
	return r->size && start < r->base + r->size && end > r->base;
}

static bool soc_nna_inside(phys_addr_t start, phys_addr_t end,
			   const struct soc_nna_region *r)
{
	return r->size && start >= r->base && end <= r->base + r->size;
}

static pgprot_t soc_nna_uncached(pgprot_t prot)
{
	return __pgprot((pgprot_val(prot) & ~_CACHE_MASK) |
			_CACHE_UNCACHED_ACCELERATED);
}

/*
 * libdrivers maps by physical address: the DMA and descriptor window, the
 * ORAM and nmem, and windows larger than the ORAM or nmem that it expects
 * to repeat them; also buffers from IOCTL_SOC_NNA_MALLOC. Nothing else.
 */
static int soc_nna_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct soc_nna *nna = container_of(file->private_data, struct soc_nna, mdev);
	phys_addr_t start = (phys_addr_t)vma->vm_pgoff << PAGE_SHIFT;
	phys_addr_t end = start + (vma->vm_end - vma->vm_start);
	struct soc_nna_region oram = soc_nna_oram(nna), nmem = { };
	struct soc_nna_mem *m;
	bool found = false;

	if (end <= start)
		return -EINVAL;

	if (soc_nna_inside(start, end, &nna->nndma)) {
		vma->vm_page_prot = soc_nna_uncached(vma->vm_page_prot);
		return io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
					  end - start, vma->vm_page_prot);
	}

	if (soc_nna_touches(start, end, &oram)) {
		vma->vm_page_prot = soc_nna_uncached(vma->vm_page_prot);
		return soc_nna_map_alias(vma, start, &oram);
	}

	if (!ingenic_nmem_region(&nmem.base, &nmem.size) &&
	    soc_nna_touches(start, end, &nmem))
		return soc_nna_map_alias(vma, start, &nmem);

	mutex_lock(&nna->lock);
	list_for_each_entry(m, &nna->mem, list) {
		struct soc_nna_region r = { m->paddr, m->size };

		if (soc_nna_inside(start, end, &r)) {
			found = true;
			break;
		}
	}
	mutex_unlock(&nna->lock);
	if (found)
		return remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				       end - start, vma->vm_page_prot);

	dev_dbg(nna->dev, "no NNA memory at %pa\n", &start);
	return -EINVAL;
}

static const struct file_operations soc_nna_fops = {
	.owner		= THIS_MODULE,
	.open		= soc_nna_open,
	.release	= soc_nna_release,
	.unlocked_ioctl	= soc_nna_ioctl,
	.mmap		= soc_nna_mmap,
};

static int soc_nna_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct soc_nna *nna;
	struct resource *res;
	int ret;

	nna = devm_kzalloc(dev, sizeof(*nna), GFP_KERNEL);
	if (!nna)
		return -ENOMEM;
	nna->dev = dev;
	nna->info = of_device_get_match_data(dev);
	mutex_init(&nna->lock);
	INIT_LIST_HEAD(&nna->mem);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "nndma");
	if (!res)
		return -ENXIO;
	nna->nndma.base = res->start;
	nna->nndma.size = resource_size(res);
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sram");
	if (!res)
		return -ENXIO;
	nna->sram.base = res->start;
	nna->sram.size = resource_size(res);

	np = of_find_compatible_node(NULL, NULL, "ingenic,xburst2-ccu");
	nna->ccu = np ? of_iomap(np, 0) : NULL;
	of_node_put(np);
	if (!nna->ccu)
		return dev_err_probe(dev, -ENXIO, "no CCU\n");

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		goto err_ccu;

	/* the ORAM's clock, as the vendor probe */
	writel(readl(nna->ccu + CCU_MSCR) | CCU_MSCR_ORAM_CLK, nna->ccu + CCU_MSCR);

	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "soc-nna:online",
				soc_nna_hwrena_set, NULL);
	if (ret < 0)
		goto err_ccu;
	nna->hp_state = ret;

	nna->mdev.minor = MISC_DYNAMIC_MINOR;
	nna->mdev.name = "soc-nna";
	nna->mdev.fops = &soc_nna_fops;
	nna->mdev.parent = dev;
	ret = misc_register(&nna->mdev);
	if (ret)
		goto err_hp;

	platform_set_drvdata(pdev, nna);
	{
		struct soc_nna_region oram = soc_nna_oram(nna);

		dev_info(dev, "NNA version %08x, ORAM %pa + %pa\n",
			 nna->info->version, &oram.base, &oram.size);
	}
	return 0;

err_hp:
	cpuhp_remove_state(nna->hp_state);
	on_each_cpu(soc_nna_hwrena_clear, NULL, 1);
err_ccu:
	iounmap(nna->ccu);
	return ret;
}

static void soc_nna_remove(struct platform_device *pdev)
{
	struct soc_nna *nna = platform_get_drvdata(pdev);

	misc_deregister(&nna->mdev);
	cpuhp_remove_state(nna->hp_state);
	on_each_cpu(soc_nna_hwrena_clear, NULL, 1);
	iounmap(nna->ccu);
}

/* Magik nna1 (libvenus 1.2.x) maps the ORAM at 0x12620000, 128 KiB of L2 */
static const struct soc_nna_info soc_nna_t40 = {
	.version = 0x20220525,
	.oram_fixed = 0x12620000,
};
static const struct soc_nna_info soc_nna_t41 = { .version = 0x00000041, .ddr_qos = true };
static const struct soc_nna_info soc_nna_a1 = { .version = 0x000000a1 };

static const struct of_device_id soc_nna_of_match[] = {
	{ .compatible = "ingenic,t40-nna", .data = &soc_nna_t40 },
	{ .compatible = "ingenic,t41-nna", .data = &soc_nna_t41 },
	{ .compatible = "ingenic,a1-nna", .data = &soc_nna_a1 },
	{ }
};
MODULE_DEVICE_TABLE(of, soc_nna_of_match);

static struct platform_driver soc_nna_driver = {
	.probe	= soc_nna_probe,
	.remove	= soc_nna_remove,
	.driver	= {
		.name		= "soc-nna",
		.of_match_table	= soc_nna_of_match,
	},
};
module_platform_driver(soc_nna_driver);

MODULE_DESCRIPTION("Ingenic T40/T41/A1 NNA memory and DMA device");
MODULE_LICENSE("GPL");
