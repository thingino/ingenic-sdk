/*
 * rmem.c - Reserved memory driver for libimp.
 * Ported from BSP to mainline 7.x.
 */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <asm/pgtable.h>
#include <linux/soc/ingenic/rmem.h>

#include "rmem_cache.h"

static phys_addr_t rmem_base, rmem_size;
/*
 * The T10/T20 also carve out "ispmem=" (the ISP's noise-reduction frames),
 * which libimp maps through /dev/rmem too. The T20 kernel has no early_param
 * for it, so it comes in here the way tx-isp takes it.
 */
static phys_addr_t ispmem_base, ispmem_size;
static char *ispmem;
module_param(ispmem, charp, 0444);
MODULE_PARM_DESC(ispmem, "SIZE@BASE of the ispmem= region libimp may map");

static bool cached;
module_param(cached, bool, 0444);
MODULE_PARM_DESC(cached, "map rmem cacheable (the vendor behaviour); default uncached");

static bool trace;
module_param(trace, bool, 0644);
MODULE_PARM_DESC(trace, "log every flush ioctl");

#define RMEN_MAGIC 'r'
#define RMEN_CMD_FLUSH_CACHE _IOWR(RMEN_MAGIC, 0, struct rmem_flush_cache_info)

struct rmem_flush_cache_info {
	unsigned int addr;
	unsigned int len;
	unsigned int dir;
};

static int rmem_open(struct inode *inode, struct file *filp) { return 0; }
static int rmem_release(struct inode *inode, struct file *filp) { return 0; }

static long rmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct rmem_flush_cache_info info;

	if (cmd != RMEN_CMD_FLUSH_CACHE)
		return -EINVAL;
	if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
		return -EFAULT;
	if (trace)
		pr_info("rmem: flush %08x+%x dir %u\n", info.addr, info.len, info.dir);
	return rmem_cache_sync_user(info.addr, info.len, info.dir);
}

/*
 * Dirty lines of a cacheable mapping that outlive it would be written back
 * over whatever a device or the next owner puts there.
 */
static void rmem_vma_close(struct vm_area_struct *vma)
{
	rmem_cache_sync_phys((phys_addr_t)vma->vm_pgoff << PAGE_SHIFT,
			     vma->vm_end - vma->vm_start, DMA_BIDIRECTIONAL);
}

static const struct vm_operations_struct rmem_vm_ops = {
	.close = rmem_vma_close,
};

static int rmem_mmap(struct file *file, struct vm_area_struct *vma)
{
	phys_addr_t start = (phys_addr_t)vma->vm_pgoff << PAGE_SHIFT;
	unsigned long len = vma->vm_end - vma->vm_start;

	/* libimp maps by physical address: only the rmem= and ispmem= regions */
	if (!((start >= rmem_base && len <= rmem_size &&
	       start - rmem_base <= rmem_size - len) ||
	      (ispmem_size && start >= ispmem_base && len <= ispmem_size &&
	       start - ispmem_base <= ispmem_size - len)))
		return -EPERM;

	vm_flags_set(vma, VM_IO);
	if (cached) {
		vma->vm_page_prot = __pgprot((pgprot_val(vma->vm_page_prot) & ~_CACHE_MASK) |
					     _page_cachable_default);
		vma->vm_ops = &rmem_vm_ops;
	} else {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	}
	if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot))
		return -EAGAIN;
	return 0;
}

static struct file_operations rmem_fops = {
	.owner = THIS_MODULE,
	.open = rmem_open,
	.release = rmem_release,
	.unlocked_ioctl = rmem_ioctl,
	.mmap = rmem_mmap,
};

static struct miscdevice rmem_mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rmem",
	.fops = &rmem_fops,
};

static int __init rmem_init(void)
{
	int ret = ingenic_rmem_region(&rmem_base, &rmem_size);

	if (ret) {
		pr_err("rmem: no rmem= region on the command line\n");
		return ret;
	}
	if (ispmem) {
		char *rest;

		ispmem_size = memparse(ispmem, &rest);
		if (*rest == '@')
			ispmem_base = memparse(rest + 1, NULL);
		else
			ispmem_size = 0;
		if (ispmem_size)
			pr_info("rmem: ispmem %pa + %pa\n", &ispmem_base, &ispmem_size);
	}
	if (cached && !rmem_cache_supported()) {
		pr_warn("rmem: L1 D-cache can alias, mapping uncached\n");
		cached = false;
	}
	if (cached && rmem_base + rmem_size > SZ_512M) {
		pr_warn("rmem: region above 512 MB has no KSEG0 alias, mapping uncached\n");
		cached = false;
	}
	pr_info("rmem: %pa + %pa, %s\n", &rmem_base, &rmem_size,
		cached ? "cached" : "uncached");
	return misc_register(&rmem_mdev);
}

static void __exit rmem_exit(void)
{
	misc_deregister(&rmem_mdev);
}

module_init(rmem_init);
module_exit(rmem_exit);
MODULE_DESCRIPTION("Reserved memory device for libimp");
MODULE_LICENSE("GPL");
