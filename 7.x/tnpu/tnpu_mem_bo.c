/*
 * Copyright 2005 (C) Jes Sorensen <jes@trained-monkey.org>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include "tnpu_drv.h"

#include <linux/idr.h>
#include <linux/bitmap.h>
#include <linux/rculist.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/addrspace.h>
#include <linux/soc/ingenic/rmem.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
#include <linux/dma-map-ops.h>
#else
#include <asm/dma-coherence.h>
#endif

#ifdef CONFIG_TNPU_MEM_DEBUG
#define PRINT_TNPU_DEBUG(fmt, ...) \
    printk(KERN_DEBUG "<TNPU> " fmt, ##__VA_ARGS__)
#else
#define PRINT_TNPU_DEBUG(fmt, ...)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
    #ifndef idr_is_empty
    #define idr_is_empty(__idr) ((__idr)->top == NULL)
    #endif // idr_is_empty
#endif // LINUX_VERSION

static inline void *tnpu_cached_alias(void *addr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    return (void *)KSEG0ADDR((unsigned long)addr);
#else
    return (void *)CAC_ADDR((unsigned long)addr);
#endif
}

static inline void *tnpu_uncached_alias(void *addr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    return (void *)KSEG1ADDR((unsigned long)addr);
#else
    return (void *)UNCAC_ADDR((unsigned long)addr);
#endif
}

static inline void __iomem *tnpu_ioremap_cached(phys_addr_t paddr, size_t size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    return ioremap_cache(paddr, size);
#else
    return ioremap_cachable(paddr, size);
#endif
}

static inline bool tnpu_dma_need_cached_alias(struct device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    /*
     * On 5.x MIPS, dma_direct_alloc() already returns an uncached alias
     * for non-coherent devices, so only the device coherency bit matters
     * when we switch the CPU view back to cached/uncached aliases.
     */
    return !dev_is_dma_coherent(dev);
#else
    return !plat_device_is_coherent(dev) && !hw_coherentio;
#endif
}

struct tnpu_pool {
    spinlock_t lock;
    struct list_head chunks;
    int min_alloc_order;
    void *data;
};

struct tnpu_pool_chunk {
    struct list_head next_chunk;
    atomic_t avail;
    phys_addr_t phys_addr;
    unsigned long start_addr;
    unsigned long end_addr;
    unsigned long bits[0];
};

static inline size_t chunk_size(const struct tnpu_pool_chunk *chunk)
{
    return chunk->end_addr - chunk->start_addr + 1;
}

static int set_bits_ll(unsigned long *addr, unsigned long mask_to_set)
{
    unsigned long val, nval;

    nval = *addr;
    do {
        val = nval;
        if (val & mask_to_set)
            return -EBUSY;
        cpu_relax();
    } while ((nval = cmpxchg(addr, val, val | mask_to_set)) != val);

    return 0;
}

static int clear_bits_ll(unsigned long *addr, unsigned long mask_to_clear)
{
    unsigned long val, nval;

    nval = *addr;
    do {
        val = nval;
        if ((val & mask_to_clear) != mask_to_clear)
            return -EBUSY;
        cpu_relax();
    } while ((nval = cmpxchg(addr, val, val & ~mask_to_clear)) != val);

    return 0;
}

/*
 * bitmap_set_ll - set the specified number of bits at the specified position
 * @map: pointer to a bitmap
 * @start: a bit position in @map
 * @nr: number of bits to set
 *
 * Set @nr bits start from @start in @map lock-lessly. Several users
 * can set/clear the same bitmap simultaneously without lock. If two
 * users set the same bit, one user will return remain bits, otherwise
 * return 0.
 */
static int bitmap_set_ll(unsigned long *map, int start, int nr)
{
    unsigned long *p = map + BIT_WORD(start);
    const int size = start + nr;
    int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
    unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

    while (nr - bits_to_set >= 0) {
        if (set_bits_ll(p, mask_to_set))
            return nr;
        nr -= bits_to_set;
        bits_to_set = BITS_PER_LONG;
        mask_to_set = ~0UL;
        p++;
    }
    if (nr) {
        mask_to_set &= BITMAP_LAST_WORD_MASK(size);
        if (set_bits_ll(p, mask_to_set))
            return nr;
    }

    return 0;
}

/*
 * bitmap_clear_ll - clear the specified number of bits at the specified position
 * @map: pointer to a bitmap
 * @start: a bit position in @map
 * @nr: number of bits to set
 *
 * Clear @nr bits start from @start in @map lock-lessly. Several users
 * can set/clear the same bitmap simultaneously without lock. If two
 * users clear the same bit, one user will return remain bits,
 * otherwise return 0.
 */
static int bitmap_clear_ll(unsigned long *map, int start, int nr)
{
    unsigned long *p = map + BIT_WORD(start);
    const int size = start + nr;
    int bits_to_clear = BITS_PER_LONG - (start % BITS_PER_LONG);
    unsigned long mask_to_clear = BITMAP_FIRST_WORD_MASK(start);

    while (nr - bits_to_clear >= 0) {
        if (clear_bits_ll(p, mask_to_clear))
            return nr;
        nr -= bits_to_clear;
        bits_to_clear = BITS_PER_LONG;
        mask_to_clear = ~0UL;
        p++;
    }
    if (nr) {
        mask_to_clear &= BITMAP_LAST_WORD_MASK(size);
        if (clear_bits_ll(p, mask_to_clear))
            return nr;
    }

    return 0;
}

/**
 * gen_pool_first_fit - find the first available region
 * of memory matching the size requirement (no alignment constraint)
 * @map: The address to base the search on
 * @size: The bitmap size in bits
 * @start: The bitnumber to start searching at
 * @nr: The number of zeroed bits we're looking for
 * @data: additional data - unused
 */
static unsigned long tnpu_pool_first_fit(unsigned long *map, unsigned long size,
        unsigned long start, unsigned int nr, void *data)
{
    return bitmap_find_next_zero_area(map, size, start, nr, 0);
}

/**
 * tnpu_pool_create - create a new special memory pool
 * @min_alloc_order: log base 2 of number of bytes each bitmap bit represents
 * @nid: node id of the node the pool structure should be allocated on, or -1
 *
 * Create a new special memory pool that can be used to manage special purpose
 * memory not managed by the regular kmalloc/kfree interface.
 */
static struct tnpu_pool *tnpu_pool_create(int min_alloc_order, int nid)
{
    struct tnpu_pool *pool;

    pool = kmalloc_node(sizeof(struct tnpu_pool), GFP_KERNEL, nid);
    if (pool != NULL) {
        spin_lock_init(&pool->lock);
        INIT_LIST_HEAD(&pool->chunks);
        pool->min_alloc_order = min_alloc_order;
        pool->data = NULL;
    }
    return pool;
}

/**
 * tnpu_pool_add_virt - add a new chunk of special memory to the pool
 * @pool: pool to add new memory chunk to
 * @virt: virtual starting address of memory chunk to add to pool
 * @phys: physical starting address of memory chunk to add to pool
 * @size: size in bytes of the memory chunk to add to pool
 * @nid: node id of the node the chunk structure and bitmap should be
 *       allocated on, or -1
 *
 * Add a new chunk of special memory to the specified pool.
 *
 * Returns 0 on success or a -ve errno on failure.
 */
static int tnpu_pool_add_virt(struct tnpu_pool *pool, unsigned long virt,
        phys_addr_t phys, size_t size, int nid)
{
    struct tnpu_pool_chunk *chunk;
    int nbits = size >> pool->min_alloc_order;
    int nbytes = sizeof(struct tnpu_pool_chunk) +
                BITS_TO_LONGS(nbits) * sizeof(long);

    chunk = kzalloc_node(nbytes, GFP_KERNEL, nid);
    if (unlikely(chunk == NULL))
        return -ENOMEM;

    chunk->phys_addr = phys;
    chunk->start_addr = virt;
    chunk->end_addr = virt + size - 1;
    atomic_set(&chunk->avail, size);

    spin_lock(&pool->lock);
    list_add_rcu(&chunk->next_chunk, &pool->chunks);
    spin_unlock(&pool->lock);

    return 0;
}

/**
 * tnpu_pool_virt_to_phys - return the physical address of memory
 * @pool: pool to allocate from
 * @addr: starting address of memory
 *
 * Returns the physical address on success, or -1 on error.
 */
static phys_addr_t tnpu_pool_virt_to_phys(struct tnpu_pool *pool, unsigned long addr)
{
    struct tnpu_pool_chunk *chunk;
    phys_addr_t paddr = -1;

    rcu_read_lock();
    list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
        if (addr >= chunk->start_addr && addr <= chunk->end_addr) {
            paddr = chunk->phys_addr + (addr - chunk->start_addr);
            break;
        }
    }
    rcu_read_unlock();

    return paddr;
}

/**
 * tnpu_pool_destroy - destroy a special memory pool
 * @pool: pool to destroy
 *
 * Destroy the specified special memory pool. Verifies that there are no
 * outstanding allocations.
 */
static void tnpu_pool_destroy(struct tnpu_pool *pool)
{
    struct list_head *_chunk, *_next_chunk;
    struct tnpu_pool_chunk *chunk;
    int order = pool->min_alloc_order;
    int bit, end_bit;

    list_for_each_safe(_chunk, _next_chunk, &pool->chunks) {
        chunk = list_entry(_chunk, struct tnpu_pool_chunk, next_chunk);
        list_del(&chunk->next_chunk);

        end_bit = chunk_size(chunk) >> order;
        bit = find_next_bit(chunk->bits, end_bit, 0);
        BUG_ON(bit < end_bit);

        kfree(chunk);
    }
    kfree(pool);
}

/**
 * tnpu_pool_alloc - allocate special memory from the pool
 * @pool: pool to allocate from
 * @size: number of bytes to allocate from the pool
 *
 * Allocate the requested number of bytes from the specified pool.
 * Uses the pool allocation function (with first-fit algorithm by default).
 * Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
static unsigned long tnpu_pool_alloc(struct tnpu_pool *pool, size_t size)
{
    struct tnpu_pool_chunk *chunk;
    unsigned long addr = 0;
    int order = pool->min_alloc_order;
    int nbits, start_bit, end_bit, remain;

    if (size == 0)
        return 0;

    nbits = (size + (1UL << order) - 1) >> order;
    rcu_read_lock();
    list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
        if (size > atomic_read(&chunk->avail))
            continue;

        start_bit = 0;
        end_bit = chunk_size(chunk) >> order;
retry:
        start_bit = tnpu_pool_first_fit(chunk->bits, end_bit, start_bit, nbits, pool->data);
        if (start_bit >= end_bit)
            continue;
        remain = bitmap_set_ll(chunk->bits, start_bit, nbits);
        if (remain) {
            remain = bitmap_clear_ll(chunk->bits, start_bit,
                         nbits - remain);
            BUG_ON(remain);
            goto retry;
        }

        addr = chunk->start_addr + ((unsigned long)start_bit << order);
        size = nbits << order;
        atomic_sub(size, &chunk->avail);
        break;
    }
    rcu_read_unlock();
    return addr;
}

/**
 * tnpu_pool_dma_alloc - allocate special memory from the pool for DMA usage
 * @pool: pool to allocate from
 * @size: number of bytes to allocate from the pool
 * @dma: dma-view physical address return value.  Use NULL if unneeded.
 *
 * Allocate the requested number of bytes from the specified pool.
 * Uses the pool allocation function (with first-fit algorithm by default).
 * Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
static void *tnpu_pool_dma_alloc(struct tnpu_pool *pool, size_t size, dma_addr_t *dma)
{
    unsigned long vaddr;

    if (!pool)
        return NULL;

    vaddr = tnpu_pool_alloc(pool, size);
    if (!vaddr)
        return NULL;

    if (dma)
        *dma = tnpu_pool_virt_to_phys(pool, vaddr);

    return (void *)vaddr;
}

/**
 * tnpu_pool_dma_free - free allocated special memory back to the pool
 * @pool: pool to free to
 * @addr: starting address of memory to free back to pool
 * @size: size in bytes of memory to free
 *
 * Free previously allocated special memory back to the specified
 * pool.  Can not be used in NMI handler on architectures without
 * NMI-safe cmpxchg implementation.
 */
static void tnpu_pool_dma_free(struct tnpu_pool *pool, unsigned long addr, size_t size)
{
    struct tnpu_pool_chunk *chunk;
    int order = pool->min_alloc_order;
    int start_bit, nbits, remain;

    nbits = (size + (1UL << order) - 1) >> order;
    rcu_read_lock();
    list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
        if (addr >= chunk->start_addr && addr <= chunk->end_addr) {
            BUG_ON(addr + size - 1 > chunk->end_addr);
            start_bit = (addr - chunk->start_addr) >> order;
            remain = bitmap_clear_ll(chunk->bits, start_bit, nbits);
            BUG_ON(remain);
            size = nbits << order;
            atomic_add(size, &chunk->avail);
            rcu_read_unlock();
            return;
        }
    }
    rcu_read_unlock();
    BUG();
}

struct tnpu_bo_node {
    pid_t                   pid;
    unsigned int            size;
    void                   *kva;
    dma_addr_t              dma;
};

struct tnpu_bo_manage {
    struct mutex       mutex;
    struct idr         idr;
    struct device     *dev;
    struct tnpu_pool  *pool;
    void              *pool_vaddr;
    unsigned int       pool_paddr;
    unsigned int       pool_size;
    unsigned int       ext_mem_size; // is_rmem
};

struct tnpu_bo_manage bo_manage;

void *tnpu_cma_alloc(size_t size, dma_addr_t *dma_handle)
{
    void *kvaddr = NULL;

    if (bo_manage.pool != NULL)
        return tnpu_pool_dma_alloc(bo_manage.pool, size, dma_handle);
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 0, 0)
    kvaddr = dma_alloc_wc(bo_manage.dev, size, dma_handle, GFP_KERNEL | __GFP_NOWARN);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(4, 0, 0)
    kvaddr = dma_alloc_writecombine(bo_manage.dev, size, dma_handle, GFP_KERNEL | __GFP_NOWARN);
#else
    kvaddr = dma_alloc_coherent(bo_manage.dev, size, dma_handle, GFP_KERNEL | __GFP_NOWARN);
#endif

    if (kvaddr && tnpu_dma_need_cached_alias(bo_manage.dev))
        kvaddr = tnpu_cached_alias(kvaddr);

    return kvaddr;
}

void tnpu_cma_free(size_t size, void *kvaddr, dma_addr_t dma_handle)
{
    if (bo_manage.pool != NULL) {
        tnpu_pool_dma_free(bo_manage.pool, (unsigned long)kvaddr, size);
        return;
    }

    if (kvaddr && tnpu_dma_need_cached_alias(bo_manage.dev))
        kvaddr = tnpu_uncached_alias(kvaddr);

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 0, 0)
    dma_free_wc(bo_manage.dev, size, kvaddr, dma_handle);
#elif LINUX_VERSION_CODE > KERNEL_VERSION(4, 0, 0)
    dma_free_writecombine(bo_manage.dev, size, kvaddr, dma_handle);
#else
    dma_free_coherent(bo_manage.dev, size, kvaddr, dma_handle);
#endif
}

int tnpu_bo_pool_add(unsigned int paddr, unsigned int size)
{
    struct tnpu_pool *pool;
    void *pool_vaddr;
    dma_addr_t pool_dma = 0;
    unsigned int ext_mem_size = 0;
    int ret;

    if (size == 0)
        return 0;

    if (paddr ==0) {
        pool_vaddr = tnpu_cma_alloc(size, &pool_dma);
        if (!pool_vaddr) {
            PRINT_TNPU_DEBUG("Failed to allocate memory from CMA, size=0x%x\n", size);
            PRINT_TNPU_INFO("Use external memory, size=0x%x\n", size);
            mutex_lock(&bo_manage.mutex);
            if (!bo_manage.pool_vaddr && !bo_manage.pool)
                bo_manage.ext_mem_size = size;
            mutex_unlock(&bo_manage.mutex);
            return 0;
        }
    } else {
        ext_mem_size = size;
        pool_vaddr = tnpu_ioremap_cached(paddr, size);
        if (!pool_vaddr) {
            PRINT_TNPU_ERROR("Failed to alloc memory, paddr=0x%x, size=0x%x\n", paddr, size);
            return -ENOMEM;
        }
        pool_dma = paddr;
    }

    pool = tnpu_pool_create(12, -1);
    if (!pool) {
        if (ext_mem_size == 0) {
            tnpu_cma_free(size, pool_vaddr, pool_dma);
        } else {
            iounmap(pool_vaddr);
        }
        PRINT_TNPU_ERROR("Failed to create CMA memory pool\n");
        return -ENOMEM;
    }

    ret = tnpu_pool_add_virt(pool, (unsigned long)pool_vaddr, pool_dma, size, -1);
    if (ret) {
        tnpu_pool_destroy(pool);
        if (ext_mem_size == 0) {
            tnpu_cma_free(size, pool_vaddr, pool_dma);
        } else {
            iounmap(pool_vaddr);
        }
        PRINT_TNPU_ERROR("Failed to add CMA memory to the pool\n");
        return -ENOMEM;
    }

    mutex_lock(&bo_manage.mutex);
    if (bo_manage.pool_vaddr || bo_manage.pool) {
        mutex_unlock(&bo_manage.mutex);
        tnpu_pool_destroy(pool);
        if (ext_mem_size == 0) {
            tnpu_cma_free(size, pool_vaddr, pool_dma);
        } else {
            iounmap(pool_vaddr);
        }
        return 0;
    }

    bo_manage.pool = pool;
    bo_manage.pool_vaddr = pool_vaddr;
    bo_manage.pool_paddr = pool_dma;
    bo_manage.pool_size = size;
    bo_manage.ext_mem_size = ext_mem_size;
    mutex_unlock(&bo_manage.mutex);

    PRINT_TNPU_INFO("Alloc 0x%x from CMA for tnpu pool\n", bo_manage.pool_size);
    return 0;
}

void tnpu_bo_pool_remove(void)
{
    mutex_lock(&bo_manage.mutex);
    if (bo_manage.pool) {
        tnpu_pool_destroy(bo_manage.pool);
        bo_manage.pool = NULL;
        if (bo_manage.ext_mem_size == 0) {
            tnpu_cma_free(bo_manage.pool_size, bo_manage.pool_vaddr, bo_manage.pool_paddr);
        } else {
            iounmap(bo_manage.pool_vaddr);
        }
        bo_manage.pool_vaddr = NULL;
        bo_manage.pool_paddr = 0;
        bo_manage.pool_size = 0;
        bo_manage.ext_mem_size = 0;
    }
    mutex_unlock(&bo_manage.mutex);
}

unsigned int tnpu_pool_avail(void)
{
    struct tnpu_pool *pool;
    struct tnpu_pool_chunk *chunk;
    unsigned int avail = 0;

    mutex_lock(&bo_manage.mutex);
    pool = bo_manage.pool;
    if (pool) {
        list_for_each_entry(chunk, &pool->chunks, next_chunk)
            avail += atomic_read(&chunk->avail);
    }
    mutex_unlock(&bo_manage.mutex);

    return avail;
}

unsigned int tnpu_pool_size(void)
{
    struct tnpu_pool *pool;
    struct tnpu_pool_chunk *chunk;
    unsigned int size = 0;

    mutex_lock(&bo_manage.mutex);
    pool = bo_manage.pool;
    if (pool) {
        list_for_each_entry(chunk, &pool->chunks, next_chunk)
            size += chunk_size(chunk);
    }
    mutex_unlock(&bo_manage.mutex);

    return size;
}

/*
 * The vendor reads nmem=<size>@<base> back from /proc/cmdline; the kernel
 * reserves that region and tells where it is.
 */
static void tnpu_get_nmem_info(unsigned int *base, unsigned int *size)
{
    phys_addr_t b, sz;

    if (ingenic_nmem_region(&b, &sz))
        return;
    *base = b;
    *size = sz;
}

int tnpu_bo_init(struct device *dev, unsigned int cma_pool_size)
{
    unsigned int nmem_base = 0;
    unsigned int nmem_size = 0;

    mutex_init(&bo_manage.mutex);
    idr_init(&bo_manage.idr);

    bo_manage.dev = dev;
    bo_manage.pool_vaddr = NULL;
    bo_manage.pool_size = 0;

    tnpu_get_nmem_info(&nmem_base, &nmem_size);

    if (nmem_base) {
        bo_manage.ext_mem_size = 0;
        return tnpu_bo_pool_add(nmem_base, nmem_size);
    } else {
#if defined(CONFIG_CMA) && !defined(CONFIG_USED_EXT_MEM)
        bo_manage.ext_mem_size = 0;
        return tnpu_bo_pool_add(0, cma_pool_size);
#else
        PRINT_TNPU_DEBUG("Used external memory\n");
        bo_manage.ext_mem_size = cma_pool_size;
#endif
    }
    return 0;
}

static void tnpu_bo_remove(struct tnpu_bo_node *bo, int idr)
{
    PRINT_TNPU_DEBUG("CMA free: pid[%d],vaddr[%px],dma[0x%x],size[%d]\n",
            bo->pid, bo->kva, bo->dma, bo->size);
    if (idr > 0)
        idr_remove(&bo_manage.idr, idr);
    if (bo->kva)
        tnpu_cma_free(bo->size, bo->kva, bo->dma);
    kfree(bo);
}

void tnpu_bo_release(pid_t pid)
{
    int id;
    struct tnpu_bo_node *bo;
    mutex_lock(&bo_manage.mutex);
    if (!idr_is_empty(&bo_manage.idr)) {
        idr_for_each_entry(&bo_manage.idr, bo, id) {
            if (bo->pid == pid) {
                tnpu_bo_remove(bo, id);
            }
        }
    }
    mutex_unlock(&bo_manage.mutex);
}

int tnpu_bo_mmap(struct vm_area_struct *vma)
{
    struct tnpu_bo_node *bo;
    unsigned long map_size = vma->vm_end - vma->vm_start;
    unsigned long handle = vma->vm_pgoff;
    unsigned long pfn;
    unsigned long prot;
    pid_t pid = task_tgid_nr(current);
    int ret = 0;

    if (handle == 0 || handle > 0x7fffffffUL)
        return -EINVAL;

    mutex_lock(&bo_manage.mutex);
    bo = idr_find(&bo_manage.idr, (int)handle);
    if (!bo) {
        ret = -ENOENT;
        goto out;
    }

    if (bo->pid != pid) {
        ret = -EPERM;
        goto out;
    }

    if (map_size > bo->size) {
        ret = -EINVAL;
        goto out;
    }

    pfn = ((unsigned long)bo->dma) >> PAGE_SHIFT;
    prot = pgprot_val(vma->vm_page_prot);
    prot &= ~_CACHE_MASK;
    prot |= _page_cachable_default;
    vma->vm_page_prot = __pgprot(prot);
    vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP | VM_IO | VM_PFNMAP);

    ret = remap_pfn_range(vma, vma->vm_start, pfn, map_size, vma->vm_page_prot);

out:
    mutex_unlock(&bo_manage.mutex);
    return ret;
}

int ioctl_tnpu_bo_append(unsigned long arg)
{
    int ret;
    tnpu_ioctl_bo_t dat;
    size_t need_size;
    struct tnpu_bo_node *bo;

    ret = copy_from_user(&dat, (tnpu_ioctl_bo_t __user *)arg, sizeof(dat));
    if (ret) {
        PRINT_TNPU_ERROR("Failed to copy data from user\n");
        return -EFAULT;
    }

    need_size = round_up(dat.size, PAGE_SIZE);

    if (need_size > 0xc000000) {
        PRINT_TNPU_ERROR("Alloc 0x%x, too large!\n", need_size);
        PRINT_TNPU_ERROR("Please update libaie.so first\n");
        return -EFAULT;
    }

    if (dat.size == 0) {
        PRINT_TNPU_ERROR("Alloc size = 0 !\n");
        PRINT_TNPU_ERROR("Please update libaie.so first\n");
        return -EFAULT;
    }

    mutex_lock(&bo_manage.mutex);
    bo = kzalloc(sizeof(*bo), GFP_KERNEL);
    if (!bo) {
        mutex_unlock(&bo_manage.mutex);
        PRINT_TNPU_ERROR("bo alloc failed\n");
        return -ENOMEM;
    }

    bo->kva = tnpu_cma_alloc(need_size, &bo->dma);
    bo->size = need_size;
    bo->pid = task_tgid_nr(current);
    if (!bo->kva) {
        mutex_unlock(&bo_manage.mutex);
        if (bo_manage.pool != NULL) {
            PRINT_TNPU_ERROR("Failed to alloc mem from pool,need=0x%x,avail=0x%x\n",
                        need_size, tnpu_pool_avail());
        } else {
            if (bo_manage.ext_mem_size != 0) {
                PRINT_TNPU_ERROR("Failed to alloc mem, to create a pool with 0x%x\n",
                                bo_manage.ext_mem_size);
            } else {
                PRINT_TNPU_ERROR("Failed to alloc mem from CMA, need=0x%x\n", need_size);
	    }
        }
        kfree(bo);
        return -ENOMEM;
    }

    dat.handle = idr_alloc(&bo_manage.idr, bo, 1, 0, GFP_KERNEL);
    if (dat.handle < 0) {
        tnpu_bo_remove(bo, 0);
        mutex_unlock(&bo_manage.mutex);
        PRINT_TNPU_ERROR("Failed to allocate IDR\n");
        return -EFAULT;
    }
    mutex_unlock(&bo_manage.mutex);

    dat.size = bo->size;
    dat.paddr = bo->dma;
    ret = copy_to_user((tnpu_ioctl_bo_t __user *)arg, &dat, sizeof(dat));
    if (ret) {
        PRINT_TNPU_ERROR("DMA copy to user failed\n");
        mutex_lock(&bo_manage.mutex);
        tnpu_bo_remove(bo, dat.handle);
        mutex_unlock(&bo_manage.mutex);
        return -EFAULT;
    }

    PRINT_TNPU_DEBUG("CMA alloc: pid[%d] vaddr[%px], dma[0x%x], size[%d]\n",
            bo->pid, bo->kva, bo->dma, need_size);

    return 0;
}


int ioctl_tnpu_bo_destroy(unsigned long arg)
{
    int ret, id;
    struct tnpu_bo_node *bo;
    pid_t pid = task_tgid_nr(current);

    ret = copy_from_user(&id, (const int __user *)arg, sizeof(id));
    if (ret) {
        PRINT_TNPU_ERROR("Failed to copy data from user\n");
        return -EFAULT;
    }

    if (id <= 0)
        return -EINVAL;

    mutex_lock(&bo_manage.mutex);
    bo = idr_find(&bo_manage.idr, id);
    if (!bo) {
        ret = -ENOENT;
        goto out;
    }

    if (bo->pid != pid) {
        ret = -EPERM;
        goto out;
    }

    tnpu_bo_remove(bo, id);
    ret = 0;

out:
    mutex_unlock(&bo_manage.mutex);
    return ret;
}

unsigned int tnpu_pool_quota(void)
{
    return bo_manage.ext_mem_size;
}

int ioctl_tnpu_pool_quota(unsigned long arg)
{
    if (copy_to_user((void __user *)arg, &bo_manage.ext_mem_size, sizeof(unsigned int)))
        return -EFAULT;
    return 0;
}

int ioctl_tnpu_pool_create(unsigned long arg)
{
    int ret;
    tnpu_ioctl_bo_t dat;

    ret = copy_from_user(&dat, (tnpu_ioctl_bo_t __user *)arg, sizeof(dat));
    if (ret) {
        PRINT_TNPU_ERROR("Failed to copy data from user\n");
        return -EFAULT;
    }

    return tnpu_bo_pool_add(dat.paddr, dat.size);
}


void tnpu_show_allocations(void)
{
    int id;
    struct tnpu_bo_node *bo;
    unsigned total_alloc = 0;
    unsigned chunk_count = 0;

    PRINT_TNPU_INFO("TNPU Memory Allocation:\n");

    mutex_lock(&bo_manage.mutex);
    if (idr_is_empty(&bo_manage.idr)) {
        PRINT_TNPU_INFO("  (No allocations)\n");
    } else {
        PRINT_TNPU_INFO("%-8s %-8s %-10s %-12s\n",  "ID", "PID", "Size", "KVA");
        idr_for_each_entry(&bo_manage.idr, bo, id) {
            PRINT_TNPU_INFO("%-8d %-8d %-10u %px\n",
                    id, bo->pid, bo->size, bo->kva);
            total_alloc += bo->size;
            chunk_count++;
        }
        PRINT_TNPU_INFO("Total: %u allocations, %u bytes\n",
                chunk_count, total_alloc);
    }
    mutex_unlock(&bo_manage.mutex);
}
