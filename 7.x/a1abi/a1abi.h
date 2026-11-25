/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Services the A1 vendor firmware (built for 4.4.94) calls through the vendor
 * wrapper's private_* functions, written against the 7.x API. Included by
 * each A1 firmware module (vo, fb): every module carries its own copy.
 *
 * What the firmware keeps in its own memory in the 4.4 layout is never handed
 * to the kernel as it is: mutexes (7.x's is larger) are looked up by address,
 * file, seq_file and platform_device arguments are stand-ins that have the one
 * member the firmware reads, and its file_operations/miscdevice/timer_list
 * are read at their 4.4 offsets and re-registered natively.
 */
#ifndef A1ABI_H
#define A1ABI_H

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/namei.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/math64.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

/* ---- 4.4 platform_device twin ---------------------------------------------- */
#define A1_TWIN_MAGIC	0x41315055
#define A1_TWIN_DEV	16	/* pdev->dev */
#define A1_TWIN_OF_NODE	(A1_TWIN_DEV + 296)	/* pdev->dev.of_node */
#define A1_TWIN_DRVDATA	96	/* pdev->dev.driver_data */
#define A1_TWIN_SIZE	1024

struct a1_twin {
	struct platform_device *pdev;
	u32 magic;
	u8 pad[8];
	u8 dev[A1_TWIN_SIZE - 16];
};

static struct a1_twin *a1_twin;

static inline struct a1_twin *a1_twin_of_pdev(struct platform_device *p)
{
	return (struct a1_twin *)p == a1_twin ? a1_twin : NULL;
}

/* a struct device * from the firmware: the twin's dev member, or a real one */
static inline struct device *a1_real_dev(struct device *d)
{
	struct a1_twin *t = (struct a1_twin *)((char *)d - A1_TWIN_DEV);

	if (a1_twin && t == a1_twin && t->magic == A1_TWIN_MAGIC)
		return &t->pdev->dev;
	return d;
}

static inline struct platform_device *a1_real_pdev(struct platform_device *p)
{
	struct a1_twin *t = a1_twin_of_pdev(p);

	return t ? t->pdev : p;
}

static inline struct a1_twin *a1_twin_alloc(struct platform_device *pdev)
{
	struct a1_twin *t = kzalloc(sizeof(*t), GFP_KERNEL);

	if (!t)
		return NULL;
	t->pdev = pdev;
	t->magic = A1_TWIN_MAGIC;
	*(void **)((char *)t + A1_TWIN_OF_NODE) = pdev->dev.of_node;
	return t;
}

/* ---- 4.4 file / seq_file stand-ins ----------------------------------------- */
#define A1_FILE44_PRIVATE_DATA	136

struct a1_file44 {
	u8 pad[A1_FILE44_PRIVATE_DATA];
	void *private_data;
	u8 tail[32];
};

#define A1_SEQ44_PRIVATE	76
#define A1_SEQ44_MAGIC		0x41315351

struct a1_seq44 {
	u32 magic;
	struct seq_file *real;
	u8 pad[A1_SEQ44_PRIVATE - 8];
	void *private;
	u8 tail[32];
};

enum { F44_OWNER, F44_LLSEEK, F44_READ, F44_WRITE, F44_READ_ITER, F44_WRITE_ITER,
       F44_ITERATE, F44_POLL, F44_UNLOCKED_IOCTL, F44_COMPAT_IOCTL, F44_MMAP,
       F44_OPEN, F44_FLUSH, F44_RELEASE };
#define F44(f, i)	(((void * const *)(f))[i])

/* ---- mutexes, looked up by the address the firmware knows them by ------------ */
#define A1_MAX_MUTEX	512
static struct {
	void *key;
	struct mutex mu;
} a1_mutexes[A1_MAX_MUTEX];
static DEFINE_SPINLOCK(a1_mutex_lock_);

static inline struct mutex *a1_mutex_of(void *key, bool init)
{
	struct mutex *r = NULL;
	int i;

	spin_lock(&a1_mutex_lock_);
	for (i = 0; i < A1_MAX_MUTEX; i++) {
		if (a1_mutexes[i].key == key) {
			r = &a1_mutexes[i].mu;
			if (init)
				mutex_init(r);
			break;
		}
	}
	if (!r) {
		for (i = 0; i < A1_MAX_MUTEX; i++) {
			if (!a1_mutexes[i].key) {
				a1_mutexes[i].key = key;
				r = &a1_mutexes[i].mu;
				mutex_init(r);
				break;
			}
		}
	}
	spin_unlock(&a1_mutex_lock_);
	return r;
}

void private_raw_mutex_init(struct mutex *m, const char *name, struct lock_class_key *key)
{
	a1_mutex_of(m, true);
}

void private_mutex_lock(struct mutex *m)
{
	mutex_lock(a1_mutex_of(m, false));
}

void private_mutex_unlock(struct mutex *m)
{
	mutex_unlock(a1_mutex_of(m, false));
}

int private_mutex_is_locked(struct mutex *m)
{
	return mutex_is_locked(a1_mutex_of(m, false));
}

/* ---- the plain ones --------------------------------------------------------- */
struct platform_device;

void private_platform_set_drvdata(struct platform_device *p, void *data)
{
	struct a1_twin *t = a1_twin_of_pdev(p);

	if (t)
		*(void **)((char *)t + A1_TWIN_DRVDATA) = data;
	else
		platform_set_drvdata(p, data);
}

void *private_platform_get_drvdata(struct platform_device *p)
{
	struct a1_twin *t = a1_twin_of_pdev(p);

	return t ? *(void **)((char *)t + A1_TWIN_DRVDATA) : platform_get_drvdata(p);
}

struct resource *private_platform_get_resource(struct platform_device *p,
					       unsigned int type, unsigned int num)
{
	return platform_get_resource(a1_real_pdev(p), type, num);
}

int private_platform_get_irq(struct platform_device *p, unsigned int num)
{
	return platform_get_irq(a1_real_pdev(p), num);
}

void __iomem *private_devm_ioremap_resource(struct device *dev, struct resource *res)
{
	return devm_ioremap(a1_real_dev(dev), res->start, resource_size(res));
}

void __iomem *private_ioremap(phys_addr_t offset, unsigned long size)
{
	return ioremap(offset, size);
}

void private_iounmap(const volatile void __iomem *addr)
{
	iounmap(addr);
}

void *private_devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	return devm_kzalloc(a1_real_dev(dev), size, GFP_KERNEL);
}

void private_devm_kfree(struct device *dev, void *p)
{
	devm_kfree(a1_real_dev(dev), p);
}

struct device_node *private_dev_of_node(struct device *dev)
{
	return a1_real_dev(dev)->of_node;
}

/* a driver whose firmware has 4.4 interrupt numbers builds in a translation */
#ifndef A1_IRQ_XLATE
#define A1_IRQ_XLATE(i)	(i)
#endif

/*
 * Interrupt storm guard: the firmware's handlers are written for a 4.4 kernel
 * that survives an interrupt its handler does not quiesce. Each line the
 * firmware requests goes through a wrapper that counts hard interrupts per
 * second; past the line's limit it is disabled, reported once through
 * a1_storm_hook() (a driver may dump its device's status registers) and
 * re-enabled two seconds later, so a noisy line degrades to a slow poll. The
 * hot plug line is threaded and its events are rare, so its limit is low.
 */
#define A1_STORM_LIMIT		20000
#define A1_STORM_LIMIT_THREADED	300 /* pin interrupts */
#define A1_IRQW_MAX		16

struct a1_irqw {
	unsigned int irq;
	void *dev;
	irq_handler_t handler;
	irq_handler_t thread_fn;
	unsigned int limit;
	unsigned long window;
	unsigned int count;
	bool dead;
	bool reported;
	struct delayed_work rearm;
};

static struct a1_irqw a1_irqw[A1_IRQW_MAX];
static void (*a1_storm_hook)(unsigned int irq);
/* runs before the firmware's handler, so a driver can read the status it is about to acknowledge */
static void (*a1_irq_pre_hook)(unsigned int irq);

static void a1_irq_rearm(struct work_struct *work)
{
	struct a1_irqw *w = container_of(work, struct a1_irqw, rearm.work);

	w->window = jiffies;
	w->count = 0;
	w->dead = false;
	enable_irq(w->irq);
}

static irqreturn_t a1_irq_wrap(int irq, void *p)
{
	struct a1_irqw *w = p;
	irqreturn_t r;

	if (a1_irq_pre_hook)
		a1_irq_pre_hook(irq);
	r = w->handler(irq, w->dev);

	if (time_after(jiffies, w->window + HZ)) {
		w->window = jiffies;
		w->count = 0;
	}
	if (++w->count > w->limit && !w->dead) {
		w->dead = true;
		disable_irq_nosync(irq);
		if (!w->reported) {
			w->reported = true;
			pr_err("a1: interrupt %d storm (%u in one second), line paused\n", irq, w->count);
			if (a1_storm_hook)
				a1_storm_hook(irq);
		}
		schedule_delayed_work(&w->rearm, 2 * HZ);
	}
	return r;
}

static irqreturn_t a1_irq_thread_wrap(int irq, void *p)
{
	struct a1_irqw *w = p;

	return w->thread_fn(irq, w->dev);
}

/* only pin interrupts (the hot plug line) are rare enough for the low limit */
static bool a1_irq_is_gpio(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);

	return d && d->chip && d->chip->name && !strncmp(d->chip->name, "GPIO", 4);
}

static struct a1_irqw *a1_irqw_get(unsigned int irq, void *dev, irq_handler_t h,
				   irq_handler_t t)
{
	int i;

	for (i = 0; i < A1_IRQW_MAX; i++) {
		if (!a1_irqw[i].handler) {
			a1_irqw[i] = (struct a1_irqw){
				.irq = irq, .dev = dev, .handler = h, .thread_fn = t,
				.limit = a1_irq_is_gpio(irq) ? A1_STORM_LIMIT_THREADED : A1_STORM_LIMIT,
			};
			INIT_DELAYED_WORK(&a1_irqw[i].rearm, a1_irq_rearm);
			return &a1_irqw[i];
		}
	}
	return NULL;
}

static struct a1_irqw *a1_irqw_find(unsigned int irq, void *dev)
{
	int i;

	for (i = 0; i < A1_IRQW_MAX; i++)
		if (a1_irqw[i].handler && a1_irqw[i].irq == irq && a1_irqw[i].dev == dev)
			return &a1_irqw[i];
	return NULL;
}

int private_request_threaded_irq(unsigned int irq, irq_handler_t handler,
				 irq_handler_t thread_fn, unsigned long flags,
				 const char *name, void *dev)
{
	unsigned long f = flags & (IRQF_SHARED | IRQF_TRIGGER_MASK | IRQF_ONESHOT);
	struct a1_irqw *w;
	int ret;

	irq = A1_IRQ_XLATE(irq);
	w = handler ? a1_irqw_get(irq, dev, handler, thread_fn) : NULL;
	if (!w)
		return request_threaded_irq(irq, handler, thread_fn, f, name, dev);
	ret = request_threaded_irq(irq, a1_irq_wrap, thread_fn ? a1_irq_thread_wrap : NULL,
				   f, name, w);
	if (ret)
		w->handler = NULL;
	return ret;
}

int private_request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
			const char *name, void *dev)
{
	return private_request_threaded_irq(irq, handler, NULL, flags & ~IRQF_ONESHOT, name, dev);
}

void private_enable_irq(unsigned int irq)
{
	enable_irq(A1_IRQ_XLATE(irq));
}

void private_disable_irq(unsigned int irq)
{
	disable_irq(A1_IRQ_XLATE(irq));
}

void private_disable_irq_nosync(unsigned int irq)
{
	disable_irq_nosync(irq);
}

void private_free_irq(unsigned int irq, void *dev)
{
	struct a1_irqw *w = a1_irqw_find(A1_IRQ_XLATE(irq), dev);

	if (w) {
		free_irq(w->irq, w);
		cancel_delayed_work_sync(&w->rearm);
		w->handler = NULL;
		return;
	}
	free_irq(A1_IRQ_XLATE(irq), dev);
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

struct clk *private_devm_clk_get(struct device *dev, const char *id)
{
	return devm_clk_get(a1_real_dev(dev), id);
}

void private_devm_clk_put(struct device *dev, struct clk *clk)
{
	devm_clk_put(a1_real_dev(dev), clk);
}

/* the 7.x CGU only takes rates it can make exactly; 3.10/4.4 rounded */
int private_clk_set_rate(struct clk *clk, unsigned long rate)
{
	long r = clk_round_rate(clk, rate);

	return clk_set_rate(clk, r > 0 ? r : rate);
}

long private_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return clk_round_rate(clk, rate);
}

unsigned long private_clk_get_rate(struct clk *clk)
{
	return clk_get_rate(clk);
}

struct clk *private_clk_get_parent(struct clk *clk)
{
	return clk_get_parent(clk);
}

/*
 * The firmware only ever prepares its gates and dividers: on the vendor kernel's
 * clock driver that was what switched them on. Here prepare does both steps.
 */
int private_clk_prepare(struct clk *clk)
{
	return clk_prepare_enable(clk);
}

void private_clk_unprepare(struct clk *clk)
{
	clk_disable_unprepare(clk);
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

/* without it the firmware's polling loops never sleep */
int private_schedule_hrtimeout(ktime_t *ex, const enum hrtimer_mode mode)
{
	return schedule_hrtimeout(ex, mode);
}

void private_init_completion(struct completion *x)
{
	init_completion(x);
}

void private_reinit_completion(struct completion *x)
{
	reinit_completion(x);
}

void private_complete(struct completion *x)
{
	complete(x);
}

void private_complete_all(struct completion *x)
{
	complete_all(x);
}

int private_wait_for_completion_interruptible(struct completion *x)
{
	return wait_for_completion_interruptible(x);
}

unsigned long private_wait_for_completion_timeout(struct completion *x, unsigned long t)
{
	return wait_for_completion_timeout(x, t);
}

long private_wait_for_completion_interruptible_timeout(struct completion *x, unsigned long t)
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

void *private_vmalloc(unsigned long size)
{
	return vzalloc(size);
}

void private_vfree(const void *p)
{
	vfree(p);
}

long private_copy_from_user(void *to, const void __user *from, long size)
{
	return copy_from_user(to, from, size);
}

long private_copy_to_user(void __user *to, const void *from, long size)
{
	return copy_to_user(to, from, size);
}

/* ---- gpio: the vendor's numbers are port * 32 + pin, as the 7.x chips' bases ---- */
int private_gpio_request(unsigned int gpio, const char *label)
{
	return gpio_request(gpio, label);
}

void private_gpio_free(unsigned int gpio)
{
	gpio_free(gpio);
}

int private_gpio_direction_output(unsigned int gpio, int value)
{
	return gpiod_direction_output_raw(gpio_to_desc(gpio), value);
}

int private_gpio_direction_input(unsigned int gpio)
{
	return gpiod_direction_input(gpio_to_desc(gpio));
}

int private_gpio_get_value(unsigned int gpio)
{
	return gpiod_get_raw_value(gpio_to_desc(gpio));
}

int private_gpio_to_irq(unsigned int gpio)
{
	return gpiod_to_irq(gpio_to_desc(gpio));
}

/*
 * jzgpio_set_func(): the A1 port's INT, MASK, PAT1, PAT0 registers
 * (0x10, 0x20, 0x30, 0x40 of the port, set at +4 and clear at +8), no shadow.
 * The low four bits of the function are the values for INT, MASK, PAT1, PAT0.
 */
int jzgpio_set_func(int port, int func, unsigned long pins)
{
	static void __iomem *gpio;
	static const unsigned int reg[4] = { 0x10, 0x20, 0x30, 0x40 };
	int i;

	if (!gpio)
		gpio = ioremap(0x10010000, 0x5000);
	if (!gpio || port < 0 || port > 4)
		return -EINVAL;
	for (i = 0; i < 4; i++) {
		unsigned int bit = (func >> (3 - i)) & 1;

		writel(pins, gpio + port * 0x1000 + reg[i] + (bit ? 4 : 8));
	}
	return 0;
}

/* ---- seq_file, proc ----------------------------------------------------------- */
struct a1_seq_ctx {
	int (*show)(struct seq_file *, void *);
	void *data;
};

static int a1_show(struct seq_file *m, void *v)
{
	struct a1_seq_ctx *c = m->private;
	struct a1_seq44 f = { .magic = A1_SEQ44_MAGIC, .real = m, .private = c->data };

	return c->show((struct seq_file *)&f, v);
}

void private_seq_printf(struct seq_file *m, const char *fmt, ...)
{
	struct a1_seq44 *f = (struct a1_seq44 *)m;
	struct va_format vaf;
	va_list args;

	if (f->magic == A1_SEQ44_MAGIC)
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
	struct a1_seq_ctx *c = kzalloc(sizeof(*c), GFP_KERNEL);
	int ret;

	if (!c)
		return -ENOMEM;
	c->show = show;
	c->data = data;
	ret = single_open_size(file, a1_show, c, size);
	if (ret)
		kfree(c);
	return ret;
}

int private_single_open(struct file *file, int (*show)(struct seq_file *, void *), void *data)
{
	return private_single_open_size(file, show, data, PAGE_SIZE);
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
	struct seq_file *m = f->private_data;

	if (m)
		kfree(m->private);
	return single_release(i, f);
}

void *pde_data_a1(const struct inode *inode)
{
	return pde_data(inode);
}

struct a1_proc {
	struct proc_dir_entry *de;
	struct proc_ops ops;
	struct a1_proc *next;
};

static struct a1_proc *a1_procs;

/* /proc/jz/<s>: the root is shared by the A1 modules, whichever loads first makes it */
struct proc_dir_entry *private_jz_proc_mkdir(char *s)
{
	char path[64];
	struct path p;

	if (kern_path("/proc/jz", 0, &p)) {
		if (!proc_mkdir("jz", NULL))
			return NULL;
	} else {
		path_put(&p);
	}
	snprintf(path, sizeof(path), "jz/%s", s);
	return proc_mkdir(path, NULL);
}

struct proc_dir_entry *proc_create_data_a1(const char *name, umode_t mode,
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

struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode,
						struct proc_dir_entry *parent,
						const void *f44, void *data)
{
	return proc_create_data_a1(name, mode, parent, f44, data);
}

static inline void a1_procs_free(void)
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

/* ---- miscdevice twin ---------------------------------------------------------- */
struct a1_misc {
	void *blob;
	struct miscdevice twin;
	struct file_operations fops;
	int (*blob_open)(struct inode *, void *);
	int (*blob_release)(struct inode *, void *);
	long (*blob_ioctl)(void *, unsigned int, unsigned long);
};

static struct a1_misc *a1_misc_twin;

/*
 * One 4.4 file stand-in per open file, kept in the real file's private_data:
 * the firmware stores its per-open state in file->private_data@136 at open
 * and reads it back in every later call.
 */
static int a1_misc_open(struct inode *inode, struct file *file)
{
	struct a1_file44 *f = kzalloc(sizeof(*f), GFP_KERNEL);
	int ret = 0;

	if (!f)
		return -ENOMEM;
	f->private_data = a1_misc_twin->blob;
	if (a1_misc_twin->blob_open)
		ret = a1_misc_twin->blob_open(inode, f);
	if (ret)
		kfree(f);
	else
		file->private_data = f;
	return ret;
}

static int a1_misc_release(struct inode *inode, struct file *file)
{
	struct a1_file44 *f = file->private_data;
	int ret = 0;

	if (a1_misc_twin->blob_release)
		ret = a1_misc_twin->blob_release(inode, f);
	kfree(f);
	return ret;
}

static long a1_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct thread_info *ti = current_thread_info();
	long saved = ti->syscall;
	long ret;

	/* the firmware's inlined access_ok() reads 4.4's addr_limit here */
	ti->syscall = 0;
	ret = a1_misc_twin->blob_ioctl(file->private_data, cmd, arg);
	ti->syscall = saved;
	return ret;
}

/* 4.4's miscdevice: minor (0), name (4), fops (8) */
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

/* ---- timers: 4.4's timer_list has function at 12 and data at 16 ---------------- */
struct a1_timer {
	void *key;
	struct timer_list tl;
	void (*func)(unsigned long);
	unsigned long data;
	unsigned long window;
	unsigned int count;
};

static struct a1_timer a1_timers[4];

static void a1_timer_cb(struct timer_list *t)
{
	struct a1_timer *tw = container_of(t, struct a1_timer, tl);

	if (time_after(jiffies, tw->window + HZ)) {
		tw->window = jiffies;
		tw->count = 0;
	}
	/* a callback that re-arms itself for "now" would livelock the timer softirq */
	if (++tw->count > 2000) {
		if (tw->count == 2001)
			pr_err("a1: timer %ps re-armed %u times in a second, stopped\n", tw->func, tw->count);
		return;
	}
	tw->func(tw->data);
}

static struct a1_timer *a1_timer_of(void *key)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(a1_timers); i++)
		if (a1_timers[i].key == key)
			return &a1_timers[i];
	for (i = 0; i < ARRAY_SIZE(a1_timers); i++) {
		if (!a1_timers[i].key) {
			a1_timers[i].key = key;
			timer_setup(&a1_timers[i].tl, a1_timer_cb, 0);
			return &a1_timers[i];
		}
	}
	return NULL;
}

int private_mod_timer(void *t44, unsigned long expires)
{
	struct a1_timer *tw = a1_timer_of(t44);

	if (!tw)
		return -ENOMEM;
	tw->func = *(void **)((char *)t44 + 12);
	tw->data = *(unsigned long *)((char *)t44 + 16);
	return mod_timer(&tw->tl, expires);
}

int private_del_timer(void *t44)
{
	struct a1_timer *tw = a1_timer_of(t44);

	return tw ? timer_delete(&tw->tl) : 0;
}

static inline void a1_timers_stop(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(a1_timers); i++)
		if (a1_timers[i].key)
			timer_delete_sync(&a1_timers[i].tl);
}

/* ---- string and file helpers the firmware imports directly -------------------------- */
size_t strlcpy_a1(char *dest, const char *src, size_t size)
{
	size_t len = strlen(src);

	if (size) {
		size_t n = len >= size ? size - 1 : len;

		memcpy(dest, src, n);
		dest[n] = 0;
	}
	return len;
}

/* 4.4: kernel_read(file, offset, addr, count) */
int kernel_read_a1(struct file *file, loff_t offset, char *addr, unsigned long count)
{
	return kernel_read(file, addr, count, &offset);
}

ssize_t vfs_read_a1(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	return kernel_read(file, (void __force *)buf, count, pos);
}

/* the firmware's 64-bit divisions: the quotient replaces *n, the remainder is returned */
uint32_t div64_32_a1(uint64_t *n, uint32_t base)
{
	return do_div(*n, base);
}

void warn_slowpath_null_a1(const char *file, int line)
{
	pr_warn("a1: warning from the firmware at %s:%d\n", file, line);
}

#endif
