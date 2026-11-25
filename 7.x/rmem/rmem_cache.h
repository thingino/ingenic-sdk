/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Cache maintenance for user ranges handed to the T31 media drivers.
 *
 * Shared by rmem.ko, avpu.ko and ipu.ko as static inlines: mainline MIPS does
 * not export _dma_cache_wback_inv() and friends, but the cache-op helpers in
 * <asm/r4kcache.h> are inline and need only cpu_data, which is exported.
 *
 * The user range is translated to a physical address through its vma and
 * the operation runs on the KSEG0 (cached, unmapped) alias. That needs no
 * TLB entry, cannot fault, and works for rmem, which has no struct page and
 * no kernel mapping. Hit-type cache ops compare the physical tag, and the
 * L1 D-cache (32 KB, 8 ways = 4 KB per way) is no larger than a page per way,
 * so the index bits are the same for the user and the KSEG0 address: every
 * line that holds the data is found. The L2 is physically indexed.
 *
 * Needs a single CPU (no IPI for the cache ops) and a 512 MB-or-lower range.
 */
#ifndef RMEM_CACHE_H
#define RMEM_CACHE_H

#include <linux/dma-direction.h>
#include <linux/mm.h>
#include <linux/mmap_lock.h>
#include <linux/sched.h>
#include <linux/sizes.h>
#include <linux/soc/ingenic/rmem.h>
#include <asm/addrspace.h>
#include <asm/barrier.h>
#include <asm/cpu-info.h>
#include <asm/pgtable-bits.h>
#include <asm/r4kcache.h>

/* the T41's two cores share the coherence the kernel's own dma sync relies on */
#if defined(CONFIG_SMP) && !defined(CONFIG_INGENIC_XBURST2_SMP)
#error "rmem_cache.h runs cache ops on the local CPU only"
#endif

#define RMEM_CACHE_CHUNK	SZ_64K

/* exported by arch/mips/mm/c-r4k.c, not declared in a header; L1 D index-type writeback+invalidate */
extern void (*r4k_blast_dcache)(void);

/* false if a cacheable mapping could alias in the L1 D-cache */
static inline bool rmem_cache_supported(void)
{
	return !(cpu_data[0].dcache.flags & MIPS_CACHE_ALIASES);
}

static inline bool rmem_cache_has_l2(void)
{
	return !(cpu_data[0].scache.flags & MIPS_CACHE_NOT_PRESENT);
}

static inline unsigned long rmem_cache_line(void)
{
	unsigned long l = cpu_dcache_line_size();

	if (rmem_cache_has_l2() && cpu_scache_line_size() > l)
		l = cpu_scache_line_size();
	return l;
}

/* L1 first so its dirty lines reach L2, then L2 pushes them to memory */
static inline void rmem_cache_wbinv(unsigned long start, unsigned long end)
{
	blast_dcache_range(start, end);
	if (rmem_cache_has_l2())
		blast_scache_range(start, end);
}

static inline void rmem_cache_wbinv_l2(unsigned long start, unsigned long end)
{
	if (rmem_cache_has_l2())
		blast_scache_range(start, end);
}

/*
 * L2 first: if L1 were dropped first, a read between the two ops could
 * refill it from the stale L2 line.
 */
static inline void rmem_cache_inv(unsigned long start, unsigned long end)
{
	if (rmem_cache_has_l2())
		blast_inv_scache_range(start, end);
	blast_inv_dcache_range(start, end);
}

static inline void rmem_cache_chunked(void (*op)(unsigned long, unsigned long),
				      unsigned long start, unsigned long end)
{
	while (start < end) {
		unsigned long n = min_t(unsigned long, end - start,
					RMEM_CACHE_CHUNK);

		op(start, start + n);
		start += n;
		cond_resched();
	}
}

/*
 * MIPS32 has no writeback-without-invalidate hit op, so TO_DEVICE and
 * BIDIRECTIONAL are both writeback+invalidate.
 *
 * FROM_DEVICE drops the lines. A partial line at either edge also holds
 * bytes outside the range, so those edge lines are written back as well as
 * invalidated rather than just dropped. A dirty edge line is written over
 * the range's own bytes in it, which is why a buffer that a device writes
 * should start and end on a cache line.
 */
static inline int rmem_cache_sync_phys(phys_addr_t pa, size_t len,
				       enum dma_data_direction dir)
{
	unsigned long lsz = rmem_cache_line();
	unsigned long start, end, lo, hi;

	if (!len)
		return 0;
	if (dir != DMA_TO_DEVICE && dir != DMA_FROM_DEVICE &&
	    dir != DMA_BIDIRECTIONAL)
		return -EINVAL;
	if (pa + len < pa || pa + len > SZ_512M)
		return -EINVAL;

	start = CKSEG0ADDR(pa);
	end = start + len;

	if (dir != DMA_FROM_DEVICE) {
		if (len >= cpu_data[0].dcache.waysize * cpu_data[0].dcache.ways) {
			/* cheaper than a hit op per line; the L2 has no such shortcut */
			r4k_blast_dcache();
			rmem_cache_chunked(rmem_cache_wbinv_l2, start, end);
		} else {
			rmem_cache_chunked(rmem_cache_wbinv, start, end);
		}
		__sync();
		return 0;
	}

	lo = ALIGN(start, lsz);
	hi = ALIGN_DOWN(end, lsz);
	if (lo > hi) {
		/* inside one line */
		rmem_cache_wbinv(ALIGN_DOWN(start, lsz), ALIGN(end, lsz));
	} else {
		if (start < lo)
			rmem_cache_wbinv(lo - lsz, lo);
		if (hi < end)
			rmem_cache_wbinv(hi, hi + lsz);
		if (lo < hi)
			rmem_cache_chunked(rmem_cache_inv, lo, hi);
	}
	__sync();
	return 0;
}

static inline bool rmem_cache_vma_uncached(struct vm_area_struct *vma)
{
	unsigned long cca = pgprot_val(vma->vm_page_prot) & _CACHE_MASK;

	return cca == _CACHE_UNCACHED || cca == _CACHE_UNCACHED_ACCELERATED;
}

static inline bool rmem_cache_in_rmem(phys_addr_t pa, size_t len)
{
	phys_addr_t base, size;

	if (ingenic_rmem_region(&base, &size))
		return false;
	return pa >= base && len <= size && pa - base <= size - len;
}

/*
 * Maintain the caches for [addr, addr + len) of the calling process, which
 * must lie in its own mappings. Per vma:
 *  - uncached mapping: nothing can be cached, nothing to do;
 *  - cacheable PFN mapping: must lie inside the rmem= region;
 *  - cacheable mapping backed by struct pages: the pages' physical address.
 * Anything else is rejected (-EINVAL), an unmapped hole is -EFAULT.
 */
static inline int rmem_cache_sync_user(unsigned long addr, size_t len,
				       enum dma_data_direction dir)
{
	struct mm_struct *mm = current->mm;
	unsigned long end = addr + len;
	int ret = 0;

	if (!len)
		return 0;
	if (end < addr || !mm)
		return -EINVAL;
	if (dir != DMA_TO_DEVICE && dir != DMA_FROM_DEVICE &&
	    dir != DMA_BIDIRECTIONAL)
		return -EINVAL;

	mmap_read_lock(mm);
	while (addr < end) {
		struct vm_area_struct *vma = find_vma(mm, addr);
		unsigned long stop;

		if (!vma || vma->vm_start > addr) {
			ret = -EFAULT;
			break;
		}
		stop = min(end, vma->vm_end);

		if (rmem_cache_vma_uncached(vma)) {
			__sync();
		} else if (vma->vm_flags & VM_PFNMAP) {
			phys_addr_t pa = ((phys_addr_t)vma->vm_pgoff << PAGE_SHIFT) +
					 (addr - vma->vm_start);

			if (!rmem_cache_in_rmem(pa, stop - addr))
				ret = -EINVAL;
			else
				ret = rmem_cache_sync_phys(pa, stop - addr, dir);
		} else {
			unsigned long a = addr;

			while (a < stop && !ret) {
				unsigned long off = a & ~PAGE_MASK;
				unsigned long n = min(stop - a, PAGE_SIZE - off);
				struct page *page;

				if (pin_user_pages(a, 1, 0, &page) != 1) {
					ret = -EFAULT;
					break;
				}
				ret = rmem_cache_sync_phys(page_to_phys(page) + off,
							   n, dir);
				unpin_user_page(page);
				a += n;
			}
		}
		if (ret)
			break;
		addr = stop;
	}
	mmap_read_unlock(mm);
	return ret;
}

#endif
