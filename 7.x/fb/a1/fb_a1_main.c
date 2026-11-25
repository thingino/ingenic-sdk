// SPDX-License-Identifier: GPL-2.0
/*
 * A1 framebuffer (/dev/fb0) on mainline 7.x: the vendor SDK 1.7.0 libfb-firmware
 * (built for the vendor 4.4.94 kernel, unmodified) plus this adapter.
 *
 * The firmware allocates and fills a 4.4 struct fb_info itself. It gets a twin
 * of that shape (kept in step with the real 7.x fb_info that is registered
 * with the fbdev core), and its fb_ops are reached through native trampolines.
 * See a1abi.h for the rest.
 */
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/thread_info.h>
#include <linux/workqueue.h>

#include "../../a1abi/a1abi.h"

static int print_level = 1;
module_param(print_level, int, 0444);
MODULE_PARM_DESC(print_level, "fb print level");

static unsigned int gui_ramsize = 1920 * 1080 * 2;
module_param(gui_ramsize, int, 0444);
static unsigned int cursor_ramsize = 128 * 128 * 2;
module_param(cursor_ramsize, int, 0444);
/* the vendor default is 3840x2160x2: a contiguous 16 MB that 7.x's page allocator does not give (4 MB at most); compress mode is off */
static unsigned int comp_ramsize = 1920 * 1080;
module_param(comp_ramsize, int, 0444);
static unsigned int comp_en;
module_param(comp_en, int, 0444);
static unsigned int comp_buf_cnt = 2;
module_param(comp_buf_cnt, int, 0444);
static unsigned int zone_en;
module_param(zone_en, int, 0444);
static unsigned int zone_a, zone_r, zone_g, zone_b;
module_param(zone_a, int, 0444);
module_param(zone_r, int, 0444);
module_param(zone_g, int, 0444);
module_param(zone_b, int, 0444);
static char mem_name[8];
module_param_string(mem_name, mem_name, sizeof(mem_name), 0444);
static int fix_canvas;
module_param(fix_canvas, int, 0444);
static int fix_line_length = 1920 * 2;
module_param(fix_line_length, int, 0444);

/* the firmware's entry points */
extern int ingenic_fb_init(void);
extern void ingenic_fb_exit(void);

/* called by the firmware, which declares them itself */
int fb_printk(unsigned int level, unsigned char *fmt, ...);
unsigned int get_gui_ramsize(void);
unsigned int get_cursor_ramsize(void);
unsigned int get_comp_ramsize(void);
unsigned int get_comp_en(void);
unsigned int get_comp_buf_cnt(void);
unsigned int get_zone_en(void);
unsigned int get_zone_a(void);
unsigned int get_zone_r(void);
unsigned int get_zone_g(void);
unsigned int get_zone_b(void);
void *get_mem_name(void);
unsigned int get_fix_canvas(void);
unsigned int get_fix_line_length(void);
struct proc_dir_entry *jz_proc_mkdir(char *s);
void __iomem *ioremap_a1(phys_addr_t phys, unsigned long size, unsigned long flags);
void iounmap_a1(const volatile void __iomem *addr);
int remap_pfn_range_a1(void *vma44, unsigned long addr, unsigned long pfn,
		       unsigned long size, pgprot_t prot);
void cancel_work_sync_a1(void *work44);

int fb_printk(unsigned int level, unsigned char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r = 0;

	if (level >= print_level) {
		va_start(args, fmt);
		vaf.fmt = (const char *)fmt;
		vaf.va = &args;
		r = printk("%pV", &vaf);
		va_end(args);
	}
	return r;
}

unsigned int get_gui_ramsize(void) { return gui_ramsize; }
unsigned int get_cursor_ramsize(void) { return cursor_ramsize; }
unsigned int get_comp_ramsize(void) { return comp_ramsize; }
unsigned int get_comp_en(void) { return comp_en; }
unsigned int get_comp_buf_cnt(void) { return comp_buf_cnt; }
unsigned int get_zone_en(void) { return zone_en; }
unsigned int get_zone_a(void) { return zone_a; }
unsigned int get_zone_r(void) { return zone_r; }
unsigned int get_zone_g(void) { return zone_g; }
unsigned int get_zone_b(void) { return zone_b; }
void *get_mem_name(void) { return mem_name; }
unsigned int get_fix_canvas(void) { return fix_canvas; }
unsigned int get_fix_line_length(void) { return fix_line_length; }

struct proc_dir_entry *jz_proc_mkdir(char *s)
{
	return private_jz_proc_mkdir(s);
}

void __iomem *ioremap_a1(phys_addr_t phys, unsigned long size, unsigned long flags)
{
	return ioremap(phys, size);
}

void iounmap_a1(const volatile void __iomem *addr)
{
	iounmap(addr);
}

/* ---- file helpers: the firmware reads /dev/rmem_info under set_fs() ------------- */
struct file *private_filp_open(const char *name, int flags, umode_t mode)
{
	return filp_open(name, flags, mode);
}

int private_filp_close(struct file *filp, fl_owner_t id)
{
	return filp_close(filp, id);
}

/* the firmware passes &file->f_pos at 4.4's offset into a file of this kernel: read from 0 */
ssize_t private_vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	loff_t p = 0;

	return kernel_read(file, (void __force *)buf, count, &p);
}

unsigned long private_get_fs(void)
{
	return 0;
}

void private_set_fs(unsigned long val)
{
}

/* ---- work: 4.4's work_struct has data (0), entry (4), func (12) ------------------- */
struct a1_work {
	void *key;
	struct work_struct w;
};

static struct a1_work a1_works[4];

static void a1_work_cb(struct work_struct *w)
{
	struct a1_work *tw = container_of(w, struct a1_work, w);
	void (*func)(void *) = *(void **)((char *)tw->key + 12);

	func(tw->key);
}

static struct a1_work *a1_work_of(void *key)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(a1_works); i++)
		if (a1_works[i].key == key)
			return &a1_works[i];
	for (i = 0; i < ARRAY_SIZE(a1_works); i++) {
		if (!a1_works[i].key) {
			a1_works[i].key = key;
			INIT_WORK(&a1_works[i].w, a1_work_cb);
			return &a1_works[i];
		}
	}
	return NULL;
}

bool private_schedule_work(void *work44)
{
	struct a1_work *tw = a1_work_of(work44);

	return tw ? schedule_work(&tw->w) : false;
}

void cancel_work_sync_a1(void *work44)
{
	struct a1_work *tw = a1_work_of(work44);

	if (tw)
		cancel_work_sync(&tw->w);
}

/* ---- dma memory: always the framebuffer's own device -------------------------------- */
static struct platform_device *a1_fb_pdev;
static void *a1_fb_pdev44;

void *private_dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp)
{
	return dma_alloc_coherent(&a1_fb_pdev->dev, size, handle, GFP_KERNEL);
}

void private_dma_free_coherent(struct device *dev, size_t size, void *cpu, dma_addr_t handle)
{
	dma_free_coherent(&a1_fb_pdev->dev, size, cpu, handle);
}

/*
 * ---- 4.4 fb_info twin -----------------------------------------------------------
 *
 * Offsets from the firmware's own debug info: var 44, fix 204, fbops 552,
 * device 556, dev 560, screen_base 568, screen_size 572, pseudo_palette 576,
 * par 588; 600 bytes in all.
 */
#define FBI44_NODE		4
#define FBI44_VAR		44
#define FBI44_FIX		204
#define FBI44_FBOPS		552
#define FBI44_DEVICE		556
#define FBI44_DEV		560
#define FBI44_SCREEN_BASE	568
#define FBI44_SCREEN_SIZE	572
#define FBI44_PSEUDO_PALETTE	576
#define FBI44_PAR		588
#define FBI44_SIZE		600

/* 4.4 fb_ops */
enum { FO_OWNER, FO_OPEN, FO_RELEASE, FO_READ, FO_WRITE, FO_CHECK_VAR, FO_SET_PAR,
       FO_SETCOLREG, FO_SETCMAP, FO_BLANK, FO_PAN_DISPLAY, FO_FILLRECT, FO_COPYAREA,
       FO_IMAGEBLIT, FO_CURSOR, FO_ROTATE, FO_SYNC, FO_IOCTL, FO_COMPAT_IOCTL,
       FO_MMAP };
#define FO(o, i)	(((void * const *)(o))[i])

#define TW(t, off, type)	(*(type *)((char *)(t) + (off)))

struct a1_fbi {
	struct fb_info *real;
	void *twin;
	const void *ops44;
};

static struct a1_fbi *a1_fbis[4];
#define OPS44(info)	(TW((info)->par, FBI44_FBOPS, const void *))

static void fbi_in(struct fb_info *info)
{
	void *t = info->par;

	memcpy((char *)t + FBI44_VAR, &info->var, sizeof(info->var));
	memcpy((char *)t + FBI44_FIX, &info->fix, sizeof(info->fix));
	TW(t, FBI44_SCREEN_BASE, void *) = info->screen_base;
	TW(t, FBI44_SCREEN_SIZE, unsigned long) = info->screen_size;
}

static void fbi_out(struct fb_info *info)
{
	void *t = info->par;

	memcpy(&info->var, (char *)t + FBI44_VAR, sizeof(info->var));
	memcpy(&info->fix, (char *)t + FBI44_FIX, sizeof(info->fix));
	info->screen_base = TW(t, FBI44_SCREEN_BASE, void __iomem *);
	info->screen_size = TW(t, FBI44_SCREEN_SIZE, unsigned long);
}

static int a1_fb_open(struct fb_info *info, int user)
{
	int (*f)(void *, int) = FO(OPS44(info), FO_OPEN);

	fbi_in(info);
	return f(info->par, user);
}

static int a1_fb_release(struct fb_info *info, int user)
{
	int (*f)(void *, int) = FO(OPS44(info), FO_RELEASE);

	fbi_in(info);
	return f(info->par, user);
}

static int a1_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int (*f)(void *, void *) = FO(OPS44(info), FO_CHECK_VAR);

	fbi_in(info);
	return f(var, info->par);
}

static int a1_fb_set_par(struct fb_info *info)
{
	int (*f)(void *) = FO(OPS44(info), FO_SET_PAR);
	int ret;

	fbi_in(info);
	ret = f(info->par);
	fbi_out(info);
	return ret;
}

static int a1_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int (*f)(void *, void *) = FO(OPS44(info), FO_PAN_DISPLAY);
	int ret;

	fbi_in(info);
	ret = f(var, info->par);
	fbi_out(info);
	return ret;
}

static int a1_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	int (*f)(void *, unsigned int, unsigned long) = FO(OPS44(info), FO_IOCTL);
	int ret;

	/*
	 * The firmware's inlined get_user()/put_user() test the user address
	 * against thread_info->addr_limit, which 4.4 keeps at offset 24 and 7.x
	 * has no use for: that word is now the syscall number. Zero is what
	 * KERNEL_DS was there, which accepts every address.
	 */
	struct thread_info *ti = current_thread_info();
	long saved = ti->syscall;

	fbi_in(info);
	ti->syscall = 0;
	ret = f(info->par, cmd, arg);
	ti->syscall = saved;
	fbi_out(info);
	return ret;
}

/*
 * 4.4's vm_area_struct: vm_start 0, vm_end 4, vm_page_prot 36, vm_flags 40,
 * vm_pgoff 76, and (here) the real vma where 4.4 has vm_private_data (84).
 */
struct a1_vma44 {
	unsigned long vm_start, vm_end;
	u8 pad0[28];
	pgprot_t vm_page_prot;
	unsigned long vm_flags;
	u8 pad1[32];
	unsigned long vm_pgoff;
	void *vm_file;
	struct vm_area_struct *real;
	u8 tail[16];
};

static int a1_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	int (*f)(void *, void *) = FO(OPS44(info), FO_MMAP);
	struct a1_vma44 v = {
		.vm_start = vma->vm_start, .vm_end = vma->vm_end,
		.vm_page_prot = vma->vm_page_prot, .vm_flags = vma->vm_flags,
		.vm_pgoff = vma->vm_pgoff, .real = vma,
	};
	int ret;

	fbi_in(info);
	ret = f(info->par, &v);
	return ret;
}

int remap_pfn_range_a1(void *vma44, unsigned long addr, unsigned long pfn,
		       unsigned long size, pgprot_t prot)
{
	struct a1_vma44 *v = vma44;

	vm_flags_set(v->real, v->vm_flags & (VM_IO | VM_DONTEXPAND | VM_DONTDUMP));
	v->real->vm_page_prot = v->vm_page_prot;
	return remap_pfn_range(v->real, addr, pfn, size, v->real->vm_page_prot);
}

static const struct fb_ops a1_fb_ops_tmpl = {
	.owner = THIS_MODULE,
};

static struct fb_ops a1_fb_ops;

void *private_framebuffer_alloc(size_t size, struct device *dev)
{
	void *t = kzalloc(FBI44_SIZE + size, GFP_KERNEL);

	if (!t)
		return NULL;
	TW(t, FBI44_PAR, void *) = (char *)t + FBI44_SIZE;
	TW(t, FBI44_DEVICE, void *) = &a1_fb_pdev->dev;
	return t;
}

void private_framebuffer_release(void *t)
{
	kfree(t);
}

int private_register_framebuffer(void *t)
{
	struct a1_fbi *f;
	struct fb_info *info;
	int ret, i;

	f = kzalloc(sizeof(*f), GFP_KERNEL);
	info = framebuffer_alloc(0, &a1_fb_pdev->dev);
	if (!f || !info) {
		kfree(f);
		if (info)
			framebuffer_release(info);
		return -ENOMEM;
	}
	f->real = info;
	f->twin = t;
	f->ops44 = TW(t, FBI44_FBOPS, void *);

	a1_fb_ops = a1_fb_ops_tmpl;
	if (FO(f->ops44, FO_OPEN))
		a1_fb_ops.fb_open = a1_fb_open;
	if (FO(f->ops44, FO_RELEASE))
		a1_fb_ops.fb_release = a1_fb_release;
	if (FO(f->ops44, FO_CHECK_VAR))
		a1_fb_ops.fb_check_var = a1_fb_check_var;
	if (FO(f->ops44, FO_SET_PAR))
		a1_fb_ops.fb_set_par = a1_fb_set_par;
	if (FO(f->ops44, FO_PAN_DISPLAY))
		a1_fb_ops.fb_pan_display = a1_fb_pan_display;
	if (FO(f->ops44, FO_IOCTL))
		a1_fb_ops.fb_ioctl = a1_fb_ioctl;
	if (FO(f->ops44, FO_MMAP))
		a1_fb_ops.fb_mmap = a1_fb_mmap;

	memcpy(&info->var, (char *)t + FBI44_VAR, sizeof(info->var));
	memcpy(&info->fix, (char *)t + FBI44_FIX, sizeof(info->fix));
	info->fbops = &a1_fb_ops;
	info->screen_base = TW(t, FBI44_SCREEN_BASE, void __iomem *);
	info->screen_size = TW(t, FBI44_SCREEN_SIZE, unsigned long);
	info->pseudo_palette = TW(t, FBI44_PSEUDO_PALETTE, void *);
	info->par = t;
	ret = register_framebuffer(info);
	if (ret) {
		framebuffer_release(info);
		kfree(f);
		return ret;
	}
	for (i = 0; i < ARRAY_SIZE(a1_fbis) && a1_fbis[i]; i++)
		;
	if (i < ARRAY_SIZE(a1_fbis))
		a1_fbis[i] = f;
	TW(t, FBI44_NODE, int) = info->node;
	TW(t, FBI44_DEV, void *) = info->dev;
	return 0;
}

int private_unregister_framebuffer(void *t)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(a1_fbis); i++) {
		struct a1_fbi *f = a1_fbis[i];

		if (f && f->twin == t) {
			unregister_framebuffer(f->real);
			framebuffer_release(f->real);
			kfree(f);
			a1_fbis[i] = NULL;
			return 0;
		}
	}
	return -EINVAL;
}

/*
 * ---- the platform device and driver ----------------------------------------------
 *
 * The firmware defines its own platform_device (name 0, id 4) and a
 * platform_driver (probe 0, remove 4, driver.name 20), registers both, and its
 * probe takes its own device back. 4.4's platform_device is handed to it as it
 * is (a1_twin is that object).
 */
struct a1_drv44 {
	int (*probe)(void *);
	int (*remove)(void *);
};

static struct a1_drv44 *a1_drv;
static struct platform_driver a1_platform_driver;

int private_platform_device_register(void *p44)
{
	const char *name = *(const char **)p44;
	int id = *(int *)((char *)p44 + 4);

	a1_fb_pdev44 = p44;
	a1_fb_pdev = platform_device_alloc(name, id);
	if (!a1_fb_pdev)
		return -ENOMEM;
	a1_fb_pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	a1_fb_pdev->dev.dma_mask = &a1_fb_pdev->dev.coherent_dma_mask;
	return platform_device_add(a1_fb_pdev);
}

void private_platform_device_unregister(void *p44)
{
	platform_device_unregister(a1_fb_pdev);
	a1_fb_pdev = NULL;
}

static int a1_probe(struct platform_device *pdev)
{
	return a1_drv->probe(a1_fb_pdev44);
}

static void a1_remove(struct platform_device *pdev)
{
	a1_drv->remove(a1_fb_pdev44);
}

int private_platform_driver_register(void *drv44)
{
	a1_drv = drv44;
	a1_platform_driver.probe = a1_probe;
	a1_platform_driver.remove = a1_remove;
	a1_platform_driver.driver.name = *(const char **)((char *)drv44 + 20);
	/* the firmware's own platform_device is already registered by this name */
	a1_twin = (struct a1_twin *)a1_fb_pdev44;
	return platform_driver_register(&a1_platform_driver);
}

void private_platform_driver_unregister(void *drv44)
{
	platform_driver_unregister(&a1_platform_driver);
}

static int __init fb_a1_init(void)
{
	return ingenic_fb_init();
}

static void __exit fb_a1_exit(void)
{
	ingenic_fb_exit();
	a1_procs_free();
}

module_init(fb_a1_init);
module_exit(fb_a1_exit);

MODULE_DESCRIPTION("A1 framebuffer: vendor firmware 1.7.0 and its adapter for mainline 7.x");
MODULE_LICENSE("GPL");
