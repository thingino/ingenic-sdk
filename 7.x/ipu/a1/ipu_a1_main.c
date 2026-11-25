// SPDX-License-Identifier: GPL-2.0
/*
 * A1 IPU (/dev/ipu) on mainline 7.x: the vendor SDK 1.7.0 libipu-firmware
 * (built for the vendor 4.4.94 kernel, unmodified) plus this adapter.
 *
 * The firmware calls the kernel through the vendor wrapper's private_*
 * functions, which are defined here against the 7.x API, and directly for a
 * handful of imports that changed (see the Kbuild: redirected with objcopy).
 *
 * The firmware's probe and remove take a 4.4 platform_device and use only its
 * dev member (offset 16) and driver data (offset 96). They get a twin of that
 * shape, zeroed, that points back at the real device, so that 4.4's inlined
 * dma_alloc_coherent() (which reads dev->archdata) takes its default path.
 */
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static int print_level = 1;
module_param(print_level, int, 0444);
MODULE_PARM_DESC(print_level, "ipu print level");

static int ipu_clk = 600000000;
module_param(ipu_clk, int, 0444);
MODULE_PARM_DESC(ipu_clk, "ipu clock freq");

static char *pclk_name = "vpll";
module_param(pclk_name, charp, 0444);
MODULE_PARM_DESC(pclk_name, "chose parent clk");

char *pclk_name_tabs[4] = { "sclka", "mpll", "vpll", "epll" };

/* the firmware's entry points */
extern int jz_ipu_init(void);
extern void jz_ipu_exit(void);
extern int ipu_start_job(void *);	/* the fb module's entry, signature as the firmware has it */
EXPORT_SYMBOL(ipu_start_job);

/* called by the firmware, which declares them itself */
int ipu_printk(unsigned int level, unsigned char *fmt, ...);
int get_ipu_clk(void);
char *get_pclk_name(void);
void dev_err_a1(const struct device *dev, const char *fmt, ...);
void *pde_data_a1(const struct inode *inode);
void warn_slowpath_null_a1(const char *file, int line);
bool dma_alloc_from_coherent_a1(struct device *dev, ssize_t size,
				dma_addr_t *handle, void **ret);
int dma_release_from_coherent_a1(struct device *dev, int order, void *vaddr);
struct dma_chan *dma_request_channel_a1(const dma_cap_mask_t *mask,
					dma_filter_fn fn, void *param);
void private_dma_release_channel(struct dma_chan *chan);
int private_dmaengine_slave_config(struct dma_chan *chan, struct dma_slave_config *cfg);
dma_cookie_t private_dmaengine_submit(struct dma_async_tx_descriptor *desc);
void private_dma_async_issue_pending(struct dma_chan *chan);

int ipu_printk(unsigned int level, unsigned char *fmt, ...)
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

int get_ipu_clk(void)
{
	return ipu_clk;
}

char *get_pclk_name(void)
{
	return pclk_name;
}

/*
 * ---- the 4.4 platform_device twin -----------------------------------------
 */
#define TWIN_MAGIC	0x41315055
#define TWIN_ID		4	/* where the 4.4 layout has pdev->id */
#define TWIN_DEV	16	/* pdev->dev */
#define TWIN_DRVDATA	96	/* pdev->dev.driver_data */
#define TWIN_SIZE	1024

struct a1_twin {
	struct platform_device *pdev;
	u32 magic;
	u8 pad[8];
	u8 dev[TWIN_SIZE - 16];
};

static struct a1_twin *a1_twin;

static struct a1_twin *twin_of_pdev(struct platform_device *p)
{
	struct a1_twin *t = (struct a1_twin *)p;

	return t == a1_twin ? t : NULL;
}

/* a struct device * from the firmware: the twin's dev member, or a real one */
static struct device *real_dev(struct device *d)
{
	struct a1_twin *t = (struct a1_twin *)((char *)d - TWIN_DEV);

	if (a1_twin && t == a1_twin && t->magic == TWIN_MAGIC)
		return &t->pdev->dev;
	return d;
}

static struct platform_device *real_pdev(struct platform_device *p)
{
	struct a1_twin *t = twin_of_pdev(p);

	return t ? t->pdev : p;
}

void dev_err_a1(const struct device *dev, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	dev_err(real_dev((struct device *)dev), "%pV", &vaf);
	va_end(args);
}

void *pde_data_a1(const struct inode *inode)
{
	return pde_data(inode);
}

void warn_slowpath_null_a1(const char *file, int line)
{
	pr_warn("ipu: warning from the firmware at %s:%d\n", file, line);
}

/* 4.4's inlined dma_alloc_coherent(): no per-device pool, then ops->alloc/free */
bool dma_alloc_from_coherent_a1(struct device *dev, ssize_t size,
				dma_addr_t *handle, void **ret)
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
	return dma_alloc_coherent(real_dev(dev), size, handle, GFP_KERNEL);
}

static void a1_dma_free(struct device *dev, size_t size, void *vaddr,
			dma_addr_t handle, unsigned long attrs)
{
	dma_free_coherent(real_dev(dev), size, vaddr, handle);
}

static struct a1_dma_ops a1_dma_ops_tab = { a1_dma_alloc, a1_dma_free };
struct a1_dma_ops *mips_dma_map_ops_a1 = &a1_dma_ops_tab;

/*
 * ---- dmaengine: the firmware's memcpy channel -----------------------------
 *
 * 4.4's dma_chan starts with the device pointer, whose device_prep_dma_memcpy
 * the firmware calls through at offset 88, and then fills the returned
 * descriptor's callback (24) and callback_param (28). The twin channel and
 * device carry those, and private_dmaengine_submit() replays the job on the
 * real channel.
 */
#define DMADEV_PREP_MEMCPY	88
#define DESC_CALLBACK		24
#define DESC_CALLBACK_PARAM	28

struct a1_fdesc {
	u8 head[32];
	dma_addr_t dest, src;
	size_t len;
	unsigned long flags;
};

struct a1_fdev {
	u8 head[DMADEV_PREP_MEMCPY];
	void *prep_memcpy;
};

struct a1_fchan {
	struct a1_fdev *device;
	struct dma_chan *real;
	struct a1_fdev fdev;
};

static void *a1_prep_memcpy(struct a1_fchan *c, dma_addr_t dest, dma_addr_t src,
			    size_t len, unsigned long flags)
{
	struct a1_fdesc *d = kzalloc(sizeof(*d), GFP_KERNEL);

	if (!d)
		return NULL;
	d->dest = dest;
	d->src = src;
	d->len = len;
	d->flags = flags;
	*(struct a1_fchan **)(d->head + 12) = c;	/* 4.4's desc->chan */
	return d;
}

struct dma_chan *dma_request_channel_a1(const dma_cap_mask_t *mask,
					dma_filter_fn fn, void *param)
{
	struct a1_fchan *c;
	dma_cap_mask_t m;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return NULL;
	dma_cap_zero(m);
	dma_cap_set(DMA_MEMCPY, m);
	{
		struct dma_chan *rc = dma_request_chan_by_mask(&m);

		c->real = IS_ERR(rc) ? NULL : rc;
	}
	c->device = &c->fdev;
	c->fdev.prep_memcpy = a1_prep_memcpy;
	if (!c->real)
		pr_warn("ipu: no memcpy dma channel, the dma ioctl will fail\n");
	return (struct dma_chan *)c;
}

void private_dma_release_channel(struct dma_chan *chan)
{
	struct a1_fchan *c = (struct a1_fchan *)chan;

	if (!c)
		return;
	if (c->real)
		dma_release_channel(c->real);
	kfree(c);
}

int private_dmaengine_slave_config(struct dma_chan *chan, struct dma_slave_config *cfg)
{
	return 0;
}

static void a1_dma_cb(void *param)
{
	struct a1_fdesc *d = param;
	void (*cb)(void *) = *(void (**)(void *))(d->head + DESC_CALLBACK);
	void *cbp = *(void **)(d->head + DESC_CALLBACK_PARAM);

	if (cb)
		cb(cbp);
	kfree(d);
}

dma_cookie_t private_dmaengine_submit(struct dma_async_tx_descriptor *desc)
{
	struct a1_fdesc *d = (struct a1_fdesc *)desc;
	struct a1_fchan *c = *(struct a1_fchan **)(d->head + 12);
	struct dma_async_tx_descriptor *rd;

	if (!c->real) {
		kfree(d);
		return -ENODEV;
	}
	rd = dmaengine_prep_dma_memcpy(c->real, d->dest, d->src, d->len, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!rd) {
		kfree(d);
		return -ENOMEM;
	}
	rd->callback = a1_dma_cb;
	rd->callback_param = d;
	return dmaengine_submit(rd);
}

void private_dma_async_issue_pending(struct dma_chan *chan)
{
	struct a1_fchan *c = (struct a1_fchan *)chan;

	if (c->real)
		dma_async_issue_pending(c->real);
}

/*
 * ---- the private_* services ------------------------------------------------
 */
int private_platform_driver_register(void *drv310);
void private_platform_driver_unregister(void *drv310);

void private_platform_set_drvdata(struct platform_device *p, void *data)
{
	struct a1_twin *t = twin_of_pdev(p);

	if (t)
		*(void **)((char *)t + TWIN_DRVDATA) = data;
	else
		platform_set_drvdata(p, data);
}

void *private_platform_get_drvdata(struct platform_device *p)
{
	struct a1_twin *t = twin_of_pdev(p);

	return t ? *(void **)((char *)t + TWIN_DRVDATA) : platform_get_drvdata(p);
}

struct resource *private_platform_get_resource(struct platform_device *p,
					       unsigned int type, unsigned int num)
{
	return platform_get_resource(real_pdev(p), type, num);
}

int private_platform_get_irq(struct platform_device *p, unsigned int num)
{
	return platform_get_irq(real_pdev(p), num);
}

void __iomem *private_ioremap(phys_addr_t offset, unsigned long size)
{
	return ioremap(offset, size);
}

void private_iounmap(const volatile void __iomem *addr)
{
	iounmap(addr);
}

int private_request_irq(unsigned int irq, irq_handler_t handler,
			unsigned long flags, const char *name, void *dev)
{
	return request_irq(irq, handler, 0, name, dev);
}

void private_free_irq(unsigned int irq, void *dev)
{
	free_irq(irq, dev);
}

void private_spin_lock(spinlock_t *l)
{
	spin_lock(l);
}

void private_spin_unlock(spinlock_t *l)
{
	spin_unlock(l);
}

void __private_spin_lock_irqsave(spinlock_t *l, unsigned long *flags)
{
	spin_lock_irqsave(l, *flags);
}

void private_spin_unlock_irqrestore(spinlock_t *l, unsigned long flags)
{
	spin_unlock_irqrestore(l, flags);
}

void private_spin_lock_init(spinlock_t *l)
{
	spin_lock_init(l);
}

/*
 * 4.4's mutex is 16 bytes and the firmware lays its next field right behind
 * it; 7.x's is larger, so initialising one in place would trample that field
 * (here the spinlock after it). The firmware's own slot only carries a
 * pointer to a real mutex.
 */
void private_raw_mutex_init(struct mutex *m, const char *name, struct lock_class_key *key)
{
	struct mutex *real = kzalloc(sizeof(*real), GFP_KERNEL);

	if (real)
		__mutex_init(real, name, key);
	memset(m, 0, 16);
	*(struct mutex **)m = real;
}

void private_mutex_lock(struct mutex *m)
{
	mutex_lock(*(struct mutex **)m);
}

void private_mutex_unlock(struct mutex *m)
{
	mutex_unlock(*(struct mutex **)m);
}

struct clk *private_devm_clk_get(struct device *dev, const char *id)
{
	return devm_clk_get(real_dev(dev), id);
}

void private_devm_clk_put(struct device *dev, struct clk *clk)
{
	devm_clk_put(real_dev(dev), clk);
}

int private_clk_set_rate(struct clk *clk, unsigned long rate)
{
	return clk_set_rate(clk, rate);
}

int private_clk_prepare_enable(struct clk *clk)
{
	return clk_prepare_enable(clk);
}

void private_clk_disable_unprepare(struct clk *clk)
{
	clk_disable_unprepare(clk);
}

void private_msleep(unsigned int ms)
{
	msleep(ms);
}

void private_init_completion(struct completion *x)
{
	init_completion(x);
}

void private_complete(struct completion *x)
{
	complete(x);
}

int private_wait_for_completion_interruptible(struct completion *x)
{
	return wait_for_completion_interruptible(x);
}

unsigned long private_wait_for_completion_interruptible_timeout(struct completion *x,
								unsigned long t)
{
	return wait_for_completion_interruptible_timeout(x, t);
}

void *private_kmalloc(size_t s, gfp_t gfp)
{
	return kzalloc(s, GFP_KERNEL);
}

void private_kfree(void *p)
{
	kfree(p);
}

long private_copy_from_user(void *to, const void __user *from, long size)
{
	return copy_from_user(to, from, size);
}

long private_copy_to_user(void __user *to, const void *from, long size)
{
	return copy_to_user(to, from, size);
}

/*
 * 4.4's seq_file keeps private at offset 76; the firmware's show function
 * reads it there. The show gets a stand-in with that member and a pointer
 * to the real seq_file, which private_seq_printf() unwraps.
 */
#define SEQ44_PRIVATE	76
#define SEQ44_MAGIC	0x41315351

struct a1_seq44 {
	u32 magic;
	struct seq_file *real;
	u8 pad[SEQ44_PRIVATE - 8];
	void *private;
	u8 tail[32];
};

static int (*a1_blob_show)(struct seq_file *, void *);

static int a1_show(struct seq_file *m, void *v)
{
	struct a1_seq44 f = { .magic = SEQ44_MAGIC, .real = m, .private = m->private };

	return a1_blob_show((struct seq_file *)&f, v);
}

void private_seq_printf(struct seq_file *m, const char *fmt, ...)
{
	struct a1_seq44 *f = (struct a1_seq44 *)m;
	struct va_format vaf;
	va_list args;

	if (f->magic == SEQ44_MAGIC)
		m = f->real;
	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	seq_printf(m, "%pV", &vaf);
	va_end(args);
}

int private_single_open_size(struct file *file, int (*show)(struct seq_file *, void *),
			     void *data, size_t size)
{
	a1_blob_show = show;
	return single_open_size(file, a1_show, data, size);
}

ssize_t private_seq_read(struct file *f, char __user *b, size_t s, loff_t *p)
{
	return seq_read(f, b, s, p);
}

loff_t private_seq_lseek(struct file *f, loff_t o, int w)
{
	return seq_lseek(f, o, w);
}

int private_single_release(struct inode *i, struct file *f)
{
	return single_release(i, f);
}

/*
 * ---- 4.4 file_operations ---------------------------------------------------
 */
enum { F44_OWNER, F44_LLSEEK, F44_READ, F44_WRITE, F44_READ_ITER, F44_WRITE_ITER,
       F44_ITERATE, F44_POLL, F44_UNLOCKED_IOCTL, F44_COMPAT_IOCTL, F44_MMAP,
       F44_OPEN, F44_FLUSH, F44_RELEASE };
#define F44(f, i)	(((void * const *)(f))[i])

struct a1_proc {
	struct proc_dir_entry *de;
	struct proc_ops ops;
	struct a1_proc *next;
};

static struct a1_proc *a1_procs;
static struct proc_dir_entry *a1_jz;

struct proc_dir_entry *private_jz_proc_mkdir(char *s)
{
	if (!a1_jz)
		a1_jz = proc_mkdir("jz", NULL);
	return a1_jz ? proc_mkdir(s, a1_jz) : NULL;
}

struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode,
						struct proc_dir_entry *parent,
						const void *f44, void *data)
{
	struct a1_proc *p = kzalloc(sizeof(*p), GFP_KERNEL);

	if (!p)
		return NULL;
	p->ops.proc_open = F44(f44, F44_OPEN);
	p->ops.proc_read = F44(f44, F44_READ);
	p->ops.proc_write = F44(f44, F44_WRITE);
	p->ops.proc_lseek = F44(f44, F44_LLSEEK);
	p->ops.proc_release = F44(f44, F44_RELEASE);
	p->de = proc_create_data(name, mode, parent, &p->ops, data);
	if (!p->de) {
		kfree(p);
		return NULL;
	}
	p->next = a1_procs;
	a1_procs = p;
	return p->de;
}

static void a1_procs_free(void)
{
	while (a1_procs) {
		struct a1_proc *p = a1_procs;

		a1_procs = p->next;
		kfree(p);
	}
}

void private_proc_remove(struct proc_dir_entry *de)
{
	proc_remove(de);
}

void private_remove_proc_entry(const char *name, struct proc_dir_entry *parent)
{
	remove_proc_entry(name, parent);
}

/*
 * ---- miscdevice twin -------------------------------------------------------
 *
 * 4.4's miscdevice: minor (0), name (4), fops (8). The firmware's open,
 * release and ioctl read file->private_data at the 4.4 offset (136) to find
 * its own object, as misc_open() left it, so they get a stand-in struct file
 * that has only that member.
 */
#define FILE44_PRIVATE_DATA	136

struct a1_file44 {
	u8 pad[FILE44_PRIVATE_DATA];
	void *private_data;
	u8 tail[32];
};

struct a1_misc {
	void *blob;
	struct miscdevice twin;
	struct file_operations fops;
	int (*blob_open)(struct inode *, void *);
	int (*blob_release)(struct inode *, void *);
	long (*blob_ioctl)(void *, unsigned int, unsigned long);
};

static struct a1_misc *a1_misc_twin;

static int a1_misc_open(struct inode *inode, struct file *file)
{
	struct a1_file44 f = { .private_data = a1_misc_twin->blob };

	return a1_misc_twin->blob_open(inode, &f);
}

static int a1_misc_release(struct inode *inode, struct file *file)
{
	struct a1_file44 f = { .private_data = a1_misc_twin->blob };

	return a1_misc_twin->blob_release(inode, &f);
}

static long a1_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct a1_file44 f = { .private_data = a1_misc_twin->blob };

	return a1_misc_twin->blob_ioctl(&f, cmd, arg);
}

int private_misc_register(void *misc44)
{
	const void *f44 = *(void **)((char *)misc44 + 8);
	struct a1_misc *m = kzalloc(sizeof(*m), GFP_KERNEL);
	int ret;

	if (!m)
		return -ENOMEM;
	m->blob = misc44;
	m->fops.owner = THIS_MODULE;
	m->blob_open = F44(f44, F44_OPEN);
	m->blob_release = F44(f44, F44_RELEASE);
	m->blob_ioctl = F44(f44, F44_UNLOCKED_IOCTL);
	m->fops.open = a1_misc_open;
	m->fops.release = a1_misc_release;
	m->fops.unlocked_ioctl = a1_misc_ioctl;
	m->twin.minor = *(int *)misc44;
	m->twin.name = *(const char **)((char *)misc44 + 4);
	m->twin.fops = &m->fops;
	a1_misc_twin = m;
	ret = misc_register(&m->twin);
	if (ret) {
		a1_misc_twin = NULL;
		kfree(m);
	}
	return ret;
}

void private_misc_deregister(void *misc44)
{
	if (!a1_misc_twin)
		return;
	misc_deregister(&a1_misc_twin->twin);
	kfree(a1_misc_twin);
	a1_misc_twin = NULL;
}

/*
 * ---- the platform driver ---------------------------------------------------
 *
 * 4.4's platform_driver: probe (0), remove (4), ..., driver.name (20).
 */
struct a1_drv44 {
	int (*probe)(void *);
	int (*remove)(void *);
};

static struct a1_drv44 *a1_drv;

static int a1_probe(struct platform_device *pdev)
{
	int ret;

	a1_twin = kzalloc(sizeof(*a1_twin), GFP_KERNEL);
	if (!a1_twin)
		return -ENOMEM;
	a1_twin->pdev = pdev;
	a1_twin->magic = TWIN_MAGIC;
	ret = a1_drv->probe(a1_twin);
	if (ret) {
		kfree(a1_twin);
		a1_twin = NULL;
	}
	return ret;
}

static void a1_remove(struct platform_device *pdev)
{
	if (!a1_twin)
		return;
	a1_drv->remove(a1_twin);
	kfree(a1_twin);
	a1_twin = NULL;
}

static const struct of_device_id a1_of_match[] = {
	{ .compatible = "ingenic,a1-ipu" },
	{ }
};
MODULE_DEVICE_TABLE(of, a1_of_match);

static struct platform_driver a1_platform_driver = {
	.probe = a1_probe,
	.remove = a1_remove,
	.driver = {
		.name = "jz-ipu",
		.of_match_table = a1_of_match,
	},
};

int private_platform_driver_register(void *drv44)
{
	a1_drv = drv44;
	return platform_driver_register(&a1_platform_driver);
}

void private_platform_driver_unregister(void *drv44)
{
	platform_driver_unregister(&a1_platform_driver);
}

static int __init ipu_a1_init(void)
{
	return jz_ipu_init();
}

static void __exit ipu_a1_exit(void)
{
	jz_ipu_exit();
	a1_procs_free();
}

module_init(ipu_a1_init);
module_exit(ipu_a1_exit);

MODULE_DESCRIPTION("A1 IPU: vendor firmware 1.7.0 and its adapter for mainline 7.x");
MODULE_LICENSE("GPL");
