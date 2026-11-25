// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-abi.c - Linux 4.4.94 ABI adapter for the T40 ISP firmware
 *
 * libt40-firmware-4-4-94.a (2.2.0) was built against Linux 4.4.94 and has
 * no 7.x build. It reaches the kernel through three doors; this file,
 * txx-funcs.c and tx-isp-twin.c cover them:
 *
 *  1. The private_* functions below, which the firmware (and the sensor
 *     drivers) call by name; the firmware's weak stubs of them lose to these.
 *     Entries that receive an object the firmware allocated with a 4.4
 *     layout (platform_device, platform_driver, miscdevice, file_operations,
 *     resource, board_info) map it to a kernel-owned twin, and the one entry
 *     that returns a kernel object the firmware reads by offset (i2c_client)
 *     hands back a 4.4-shaped twin. The kernel never touches a firmware-owned
 *     struct and the firmware never sees a 7.x one.
 *  2. Kernel-allocated objects the firmware reads in code the compiler
 *     inlined (file, seq_file, inode, thread_info): the firmware reaches
 *     them only through its file_operations tables, so tx-isp-twin.c
 *     hands each handler a 4.4-shaped twin of the object instead.
 *  3. Direct imports whose semantics or layout changed: the Kbuild redirects
 *     them with objcopy to the t40_*_44 translators.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/xarray.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/bug.h>
#include <linux/dcache.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/ioport.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/pfn.h>
#include <linux/dma-mapping.h>
#include <linux/soc/ingenic/rmem.h>
#include <asm/thread_info.h>
#include <dt-bindings/clock/ingenic,t40-cgu.h>

#include <txx-funcs.h>
#include <tx-isp-device.h>
#include <soc/gpio.h>
#include "tx-isp-abi.h"
#include "tx-isp-twin.h"

#define BLOB(base, off, type)	(*(type *)((u8 *)(base) + (off)))

#define T40_MAX_TWINS		16

/* writers hold t40_twin_lock; readers, some in IRQ context, are lock-free */
static DEFINE_MUTEX(t40_twin_lock);

static unsigned long ispmem_base;
static unsigned long ispmem_size;
module_param(ispmem_base, ulong, 0444);
MODULE_PARM_DESC(ispmem_base, "ISP private memory physical base (3.10 ispmem=<size>@<base>)");
module_param(ispmem_size, ulong, 0444);
MODULE_PARM_DESC(ispmem_size, "ISP private memory size in bytes");

/*
 * ---- platform_device / platform_driver -------------------------------------
 *
 * The firmware allocates six platform_devices (248 bytes, 4.4) and six
 * platform_drivers (92 bytes); on 7.x they are 304 and 108 bytes with members
 * inserted mid-struct. Letting the driver core walk them scribbles past their
 * end, so each gets a kernel-owned twin; probe/remove are thunked back with
 * the firmware's own pointer, which keeps reading its own object with its own
 * layout.
 */
/* measured in the firmware's own platform_device objects, see tx_isp_abi_init() */
#define T40_PDEV44_NAME		0	/* const char * */
#define T40_PDEV44_ID		4	/* int */
#define T40_PDEV44_DEV		16	/* struct device */
#define T40_PDEV44_NUM_RES	376	/* u32 */
#define T40_PDEV44_RES		380	/* struct resource *, 28-byte stride */
#define T40_DEV44_INIT_NAME	44	/* const char * */
#define T40_DEV44_DRIVER	72	/* struct device_driver * */
#define T40_DEV44_DMA_MASK	256	/* u64 * */
#define T40_DEV44_COHERENT	264	/* u64 */
#define T40_DEV44_DRIVER_DATA	80	/* void * */
#define T40_ISP_PLATFORMS	136	/* the firmware's table of {platform_device, platform_driver} in its tx_isp_dev */
#define T40_ISP_NPLATFORMS	16
#define T40_RES44_SIZE		28
#define T40_RES44_START		0
#define T40_RES44_END		4
#define T40_RES44_NAME		8
#define T40_RES44_FLAGS		12
#define T40_PDRV44_PROBE	0	/* int (*)(struct platform_device *) */
#define T40_PDRV44_REMOVE	4
#define T40_PDRV44_DRIVER	20	/* struct device_driver; .name at +0, .owner at +8 */

typedef int (*t40_pdrv_fn)(void *);

struct t40_twin_dev {
	void *blob;			/* firmware's platform_device */
	struct platform_device *twin;
};

struct t40_twin_drv {
	void *blob;			/* firmware's platform_driver */
	struct platform_driver twin;
};

static struct t40_twin_dev t40_twin_devs[T40_MAX_TWINS];
static struct t40_twin_drv t40_twin_drvs[T40_MAX_TWINS];

static struct t40_twin_dev *t40_dev_by_blob(const void *blob)
{
	int i;

	if (!blob)
		return NULL;
	for (i = 0; i < T40_MAX_TWINS; i++)
		if (t40_twin_devs[i].blob == blob)
			return &t40_twin_devs[i];
	return NULL;
}

static struct t40_twin_dev *t40_dev_by_twin(const struct platform_device *pdev)
{
	int i;

	for (i = 0; i < T40_MAX_TWINS; i++)
		if (t40_twin_devs[i].blob && t40_twin_devs[i].twin == pdev)
			return &t40_twin_devs[i];
	return NULL;
}

/* the firmware passes &pdev->dev, which is its object + 16 */
static struct t40_twin_dev *t40_dev_by_blob_dev(const struct device *dev)
{
	return dev ? t40_dev_by_blob((const u8 *)dev - T40_PDEV44_DEV) : NULL;
}

static struct t40_twin_drv *t40_drv_by_blob(const void *blob)
{
	int i;

	if (!blob)
		return NULL;
	for (i = 0; i < T40_MAX_TWINS; i++)
		if (t40_twin_drvs[i].blob == blob)
			return &t40_twin_drvs[i];
	return NULL;
}

/* firmware device pointer for a native struct device, or NULL */
void *t40_blob_dev_of(const struct device *dev)
{
	int i;

	for (i = 0; i < T40_MAX_TWINS; i++)
		if (t40_twin_devs[i].blob && &t40_twin_devs[i].twin->dev == dev)
			return t40_twin_devs[i].blob;
	return NULL;
}

/* drvdata of a firmware-owned struct device, which the firmware reads back through dev_get_drvdata() */
void *t40_dev_get_drvdata(const struct device *dev)
{
	struct t40_twin_dev *td = t40_dev_by_blob_dev(dev);

	return dev_get_drvdata(td ? &td->twin->dev : dev);
}

/* any live twin, for the DMA API calls that need a real device */
struct device *t40_any_dev(void)
{
	int i;

	for (i = 0; i < T40_MAX_TWINS; i++)
		if (t40_twin_devs[i].blob)
			return &t40_twin_devs[i].twin->dev;
	return NULL;
}

static int t40_twin_probe(struct platform_device *pdev)
{
	struct t40_twin_dev *td = t40_dev_by_twin(pdev);
	struct t40_twin_drv *tr = container_of(to_platform_driver(pdev->dev.driver),
					       struct t40_twin_drv, twin);
	t40_pdrv_fn probe = BLOB(tr->blob, T40_PDRV44_PROBE, t40_pdrv_fn);

	if (!td || !probe)
		return -ENODEV;
	/* what 3.10 would have left in the firmware's own object */
	BLOB(td->blob, T40_PDEV44_DEV + T40_DEV44_DRIVER, void *) =
		(u8 *)tr->blob + T40_PDRV44_DRIVER;
	return probe(td->blob);
}

/*
 * tx_isp_remove() runs each registered sub-platform's remove callback through
 * its 4.4 platform_driver without testing it. One read as NULL on silicon, so
 * give a missing callback a no-op.
 */
static int t40_noop_remove(void *pdev)
{
	return 0;
}

static void t40_fix_platform_array(void *drvdata)
{
	unsigned long *e = drvdata ? (unsigned long *)((unsigned char *)drvdata + T40_ISP_PLATFORMS) : NULL;
	int i;

	for (i = 0; e && i < T40_ISP_NPLATFORMS; i++, e += 2) {
		unsigned long *drv = (unsigned long *)e[1];

		if (!drv)
			continue;
		if (!drv[1]) {
			pr_warn("t40-isp: sub-platform %d has no remove callback\n", i);
			drv[1] = (unsigned long)t40_noop_remove;
		}
	}
}

static void t40_twin_remove(struct platform_device *pdev)
{
	struct t40_twin_dev *td = t40_dev_by_twin(pdev);
	struct t40_twin_drv *tr = container_of(to_platform_driver(pdev->dev.driver),
					       struct t40_twin_drv, twin);
	t40_pdrv_fn remove = BLOB(tr->blob, T40_PDRV44_REMOVE, t40_pdrv_fn);

	if (!td)
		return;
	if (!strcmp(pdev->name, "tx-isp"))
		t40_fix_platform_array(BLOB(td->blob, T40_PDEV44_DEV + T40_DEV44_DRIVER_DATA, void *));
	if (remove)
		remove(td->blob);
	BLOB(td->blob, T40_PDEV44_DEV + T40_DEV44_DRIVER, void *) = NULL;
}

/*
 * The firmware's IRQ resources hold 3.10 INTC numbers (37, 38); the Linux
 * numbers come from the DT nodes, which are status = "disabled" so nothing
 * else claims them. The firmware's CSI device has no IRQ resource, and
 * keeps none: it never asks for one.
 */
static const struct {
	const char *dev;
	const char *compat;
	const char *irq_name;
} t40_irq_map[] = {
	{ "isp-m0", "ingenic,t40-isp", "isp" },
	{ "isp-w02", "ingenic,t40-isp", "vic" },
};

/* the n-th interrupt resource of a device is the DT interrupt "<name>" (n = 0) or "<name><n>" */
static int t40_dt_irq(const char *devname, int n)
{
	int i, irq = -ENXIO;

	for (i = 0; i < ARRAY_SIZE(t40_irq_map); i++) {
		struct device_node *np;
		char name[16];

		if (strcmp(devname, t40_irq_map[i].dev))
			continue;
		np = of_find_compatible_node(NULL, NULL, t40_irq_map[i].compat);
		if (!np)
			return -ENODEV;
		if (n)
			snprintf(name, sizeof(name), "%s%d", t40_irq_map[i].irq_name, n);
		else
			snprintf(name, sizeof(name), "%s", t40_irq_map[i].irq_name);
		irq = of_irq_get_byname(np, name);
		of_node_put(np);
		break;
	}
	return irq;
}

static int priv_platform_device_register(struct platform_device *pdev)
{
	void *blob = pdev;
	const char *name = BLOB(blob, T40_PDEV44_NAME, const char *);
	struct t40_twin_dev *td = NULL;
	struct platform_device *twin;
	struct resource *res = NULL;
	u32 nres = BLOB(blob, T40_PDEV44_NUM_RES, u32);
	void *res310 = BLOB(blob, T40_PDEV44_RES, void *);
	int i, ret, nirq = 0;

	twin = platform_device_alloc(name, BLOB(blob, T40_PDEV44_ID, int));
	if (!twin)
		return -ENOMEM;
	if (nres && res310) {
		/* 28-byte firmware stride to the 32-byte native struct resource */
		res = kcalloc(nres, sizeof(*res), GFP_KERNEL);
		if (!res) {
			ret = -ENOMEM;
			goto err_put;
		}
		for (i = 0; i < nres; i++) {
			u8 *r = (u8 *)res310 + i * T40_RES44_SIZE;

			res[i].start = BLOB(r, T40_RES44_START, u32);
			res[i].end = BLOB(r, T40_RES44_END, u32);
			res[i].name = BLOB(r, T40_RES44_NAME, const char *);
			res[i].flags = BLOB(r, T40_RES44_FLAGS, u32);
			pr_info("t40-isp: %s res %d: %#llx..%#llx flags %#lx\n", name, i,
				 (u64)res[i].start, (u64)res[i].end, res[i].flags);
			if (res[i].flags & IORESOURCE_IRQ) {
				int irq = t40_dt_irq(name, nirq++);

				if (irq <= 0) {
					pr_err("t40-isp: no DT irq for %s (%d)\n", name, irq);
					ret = irq ? irq : -ENXIO;
					goto err_put;
				}
				res[i].start = irq;
				res[i].end = irq;
			}
		}
		ret = platform_device_add_resources(twin, res, nres);
		if (ret)
			goto err_put;
	}
	/*
	 * platform_data stays NULL: the firmware reads it from its own object,
	 * and platform_device_release() would kfree() whatever we put here.
	 */
	twin->dev.dma_mask = BLOB(blob, T40_PDEV44_DEV + T40_DEV44_DMA_MASK, u64 *);
	twin->dev.coherent_dma_mask = BLOB(blob, T40_PDEV44_DEV + T40_DEV44_COHERENT, u64);
	/* keep dev_name() meaningful on the firmware's own object */
	BLOB(blob, T40_PDEV44_DEV + T40_DEV44_INIT_NAME, const char *) = name;

	mutex_lock(&t40_twin_lock);
	for (i = 0; i < T40_MAX_TWINS; i++) {
		if (!t40_twin_devs[i].blob) {
			td = &t40_twin_devs[i];
			td->twin = twin;
			td->blob = blob;	/* last: publishes the entry */
			break;
		}
	}
	mutex_unlock(&t40_twin_lock);
	if (!td) {
		ret = -ENOSPC;
		goto err_put;
	}

	ret = platform_device_add(twin);
	if (ret) {
		mutex_lock(&t40_twin_lock);
		td->blob = NULL;
		td->twin = NULL;
		mutex_unlock(&t40_twin_lock);
		goto err_put;
	}
	kfree(res);
	pr_info("t40-isp: twin %s (%u resources)\n",
		dev_name(&twin->dev), nres);
	return 0;

err_put:
	kfree(res);
	platform_device_put(twin);
	return ret;
}

static void priv_platform_device_unregister(struct platform_device *pdev)
{
	struct t40_twin_dev *td = t40_dev_by_blob(pdev);
	struct platform_device *twin;

	if (!td)
		return;
	twin = td->twin;
	platform_device_unregister(twin);
	mutex_lock(&t40_twin_lock);
	td->blob = NULL;
	td->twin = NULL;
	mutex_unlock(&t40_twin_lock);
}

/* the firmware imports __platform_driver_register(); the Kbuild redirects it here */
int t40_platform_driver_register_44(struct platform_driver *drv, struct module *owner)
{
	void *blob = drv;
	struct t40_twin_drv *tr = NULL;
	int i, ret;

	mutex_lock(&t40_twin_lock);
	for (i = 0; i < T40_MAX_TWINS; i++) {
		if (!t40_twin_drvs[i].blob) {
			tr = &t40_twin_drvs[i];
			memset(&tr->twin, 0, sizeof(tr->twin));
			tr->twin.driver.name = BLOB(blob, T40_PDRV44_DRIVER, const char *);
			tr->twin.probe = t40_twin_probe;
			tr->twin.remove = t40_twin_remove;
			tr->blob = blob;
			break;
		}
	}
	mutex_unlock(&t40_twin_lock);
	if (!tr)
		return -ENOSPC;

	/* the owner is the firmware's __this_module, i.e. this module */
	ret = __platform_driver_register(&tr->twin, owner, KBUILD_MODNAME);
	if (ret) {
		mutex_lock(&t40_twin_lock);
		tr->blob = NULL;
		mutex_unlock(&t40_twin_lock);
	}
	return ret;
}

static void priv_platform_driver_unregister(struct platform_driver *drv)
{
	struct t40_twin_drv *tr = t40_drv_by_blob(drv);

	if (!tr)
		return;
	platform_driver_unregister(&tr->twin);
	mutex_lock(&t40_twin_lock);
	tr->blob = NULL;
	mutex_unlock(&t40_twin_lock);
}

static struct resource *priv_platform_get_resource(struct platform_device *pdev,
						   unsigned int type, unsigned int num)
{
	struct t40_twin_dev *td = t40_dev_by_blob(pdev);

	return td ? platform_get_resource(td->twin, type, num) : NULL;
}

static int priv_platform_get_irq(struct platform_device *pdev, unsigned int num)
{
	struct t40_twin_dev *td = t40_dev_by_blob(pdev);

	return td ? platform_get_irq_optional(td->twin, num) : -ENODEV;
}

static void priv_platform_set_drvdata(struct platform_device *pdev, void *data)
{
	struct t40_twin_dev *td = t40_dev_by_blob(pdev);

	if (td) {
		platform_set_drvdata(td->twin, data);
		BLOB(pdev, T40_PDEV44_DEV + T40_DEV44_DRIVER_DATA, void *) = data;
	}
}

static void *priv_platform_get_drvdata(const struct platform_device *pdev)
{
	struct t40_twin_dev *td = t40_dev_by_blob(pdev);

	return td ? platform_get_drvdata(td->twin) : NULL;
}

static void priv_dev_set_drvdata(struct device *dev, void *data)
{
	struct t40_twin_dev *td = t40_dev_by_blob_dev(dev);

	dev_set_drvdata(td ? &td->twin->dev : dev, data);
	if (td)
		BLOB(dev, T40_DEV44_DRIVER_DATA, void *) = data;
}

static void *priv_dev_get_drvdata(const struct device *dev)
{
	return t40_dev_get_drvdata(dev);
}

static struct resource *priv_request_mem_region(resource_size_t start,
						resource_size_t n, const char *name)
{
	return __request_region(&iomem_resource, start, n, name, 0);
}

static void priv_release_mem_region(resource_size_t start, resource_size_t n)
{
	__release_region(&iomem_resource, start, n);
}

static void __iomem *priv_ioremap(phys_addr_t offset, unsigned long size)
{
	return ioremap(offset, size);
}

static void priv_iounmap(const volatile void __iomem *addr)
{
	iounmap(addr);
}

static void priv_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

/*
 * The firmware frees some vmalloc() memory with kfree(), which 4.4 let by
 * and 7.x turns into a warning and a corrupted heap.
 */
void t40_kfree_44(const void *p)
{
	if (!t40_alloc_untrack(p))
		return;
	if (is_vmalloc_addr(p))
		vfree(p);
	else
		kfree(p);
}

static void priv_kfree(void *p)
{
	t40_kfree_44(p);
}

static void priv_spin_lock_irqsave(spinlock_t *lock, unsigned long *flags)
{
	spin_lock_irqsave(lock, *flags);
}

static void priv_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
	spin_unlock_irqrestore(lock, flags);
}

static void priv_spin_lock_init(spinlock_t *lock)
{
	spin_lock_init(lock);
}

static void priv_raw_mutex_init(struct mutex *lock, const char *name,
				struct lock_class_key *key)
{
	mutex_init(lock);
}

/*
 * ---- clocks ----------------------------------------------------------------
 *
 * The firmware and the sensor drivers ask for the 3.10 tree's clock names
 * with a NULL or firmware-owned device. tx_isp_abi_init() aliases them onto
 * the T40 CGU clocks named by the DT, for the module's lifetime. 3.10 had
 * separate "cgu_isp" (divider) and "isp" (gate) clocks; the 7.x CGU has one
 * clock with all three, so both names reach it.
 */
static const struct {
	const char *alias;
	u32 cgu_index;
} t40_clk_aliases[] = {
	/* 4.4's per-stage clocks: the 7.x CGU has one clock with mux, divider and gate */
	{ "div_isp", T40_CLK_ISP }, { "mux_isp", T40_CLK_ISP }, { "gate_isp", T40_CLK_ISP },
	{ "gate_csi", T40_CLK_MIPI_CSI },
	{ "div_ahb0", T40_CLK_AHB0 }, { "gate_ahb0", T40_CLK_AHB0 },
	{ "div_cim0", T40_CLK_CIM0 }, { "mux_cim0", T40_CLK_CIM0 },
	{ "div_cim1", T40_CLK_CIM1 }, { "mux_cim1", T40_CLK_CIM1 },
	{ "div_cim2", T40_CLK_CIM2 }, { "mux_cim2", T40_CLK_CIM2 },
	{ "ext", T40_CLK_EXCLK }, { "apll", T40_CLK_APLL }, { "mpll", T40_CLK_MPLL },
	{ "vpll", T40_CLK_VPLL }, { "epll", T40_CLK_EPLL }, { "sclka", T40_CLK_SCLKA },
};

static struct clk_lookup *t40_clk_lookups[ARRAY_SIZE(t40_clk_aliases)];

static struct clk *t40_dt_clk(int i)
{
	struct of_phandle_args args = { .args_count = 1 };
	struct clk *clk;

	args.np = of_find_compatible_node(NULL, NULL, "ingenic,t40-cgu");
	if (!args.np)
		return ERR_PTR(-ENODEV);
	args.args[0] = t40_clk_aliases[i].cgu_index;
	clk = of_clk_get_from_provider(&args);
	of_node_put(args.np);
	return clk;
}

/* the handles clk_get() gave out: a second put of one is dropped, what is left is put at exit */
static DEFINE_XARRAY(t40_clks);

static struct clk *priv_clk_get(struct device *dev, const char *id)
{
	/* a firmware-owned dev would be read past the end of its object */
	struct clk *clk = clk_get(NULL, id);

	if (IS_ERR(clk))
		pr_warn("t40-isp: no clock \"%s\" (%ld)\n", id ? id : "?", PTR_ERR(clk));
	else
		xa_store(&t40_clks, (unsigned long)clk, clk, GFP_KERNEL);
	return clk;
}

/*
 * The firmware's remove path disables the clocks its activate path enabled,
 * and also when the activate never ran (a sensor that did not answer), which
 * 4.4's clock code let pass. Each handle's enables are counted here and a
 * disable without one is dropped.
 */
#define T40_CLK_TRACK	64

static struct {
	struct clk *clk;
	int enabled;
} t40_clk_track[T40_CLK_TRACK];
static DEFINE_SPINLOCK(t40_clk_track_lock);

static int t40_clk_track_add(struct clk *clk, int delta)
{
	int i, free = -1, ret = 0;

	spin_lock(&t40_clk_track_lock);
	for (i = 0; i < T40_CLK_TRACK; i++) {
		if (t40_clk_track[i].clk == clk)
			break;
		if (free < 0 && !t40_clk_track[i].clk)
			free = i;
	}
	if (i == T40_CLK_TRACK) {
		if (delta > 0 && free >= 0) {
			t40_clk_track[free].clk = clk;
			t40_clk_track[free].enabled = delta;
		} else if (delta < 0) {
			ret = -ENOENT;
		}
	} else if (delta < 0 && t40_clk_track[i].enabled <= 0) {
		ret = -ENOENT;
	} else {
		t40_clk_track[i].enabled += delta;
		if (!t40_clk_track[i].enabled)
			t40_clk_track[i].clk = NULL;
	}
	spin_unlock(&t40_clk_track_lock);
	return ret;
}

static int priv_clk_enable(struct clk *clk)
{
	int ret = clk_prepare_enable(clk);

	if (!ret)
		t40_clk_track_add(clk, 1);
	return ret;
}

static void priv_clk_disable(struct clk *clk)
{
	if (IS_ERR_OR_NULL(clk))
		return;
	if (t40_clk_track_add(clk, -1))
		return;
	clk_disable_unprepare(clk);
}

static unsigned long priv_clk_get_rate(struct clk *clk)
{
	return clk_get_rate(clk);
}

static void priv_clk_put(struct clk *clk)
{
	/* 4.4's clk_put() ignored an error pointer the firmware kept from a failed clk_get() */
	if (!IS_ERR_OR_NULL(clk) && xa_erase(&t40_clks, (unsigned long)clk))
		clk_put(clk);
}

/*
 * 3.10's clock code took the nearest rate it could make; the 7.x Ingenic CGU
 * refuses any rate it cannot hit exactly, which left the ISP at the boot
 * loader's 80 MHz (isp_clk=250000000 asks for an unreachable rate).
 */
static int priv_clk_set_rate(struct clk *clk, unsigned long rate)
{
	long r = clk_round_rate(clk, rate);
	int ret = clk_set_rate(clk, r > 0 ? r : rate);

	pr_debug("t40-isp: clk %s set_rate %lu -> %ld: %d (now %lu)\n",
		__clk_get_name(clk), rate, r, ret, clk_get_rate(clk));
	return ret;
}

/*
 * ---- i2c -------------------------------------------------------------------
 *
 * isp_i2c_new_subdev_board() creates the sensor's i2c client through the
 * table and then reads client->dev.driver (5.15 offset 88) and
 * driver->owner (8, the same on both) to pin the sensor module. It hands the
 * pointer back only to table entries, so it gets a 4.4-shaped twin carrying
 * the few fields it reads, with the kernel's client behind it. The
 * board_info it passes is the 44-byte 4.4 one (type[20] flags addr
 * platform_data archdata of_node acpi_node): rebuilt natively.
 */
#define T40_CLIENT44_SIZE	408
#define T40_CLIENT44_FLAGS	0	/* u16 */
#define T40_CLIENT44_ADDR	2	/* u16 */
#define T40_CLIENT44_NAME	4	/* char[20] */
#define T40_CLIENT44_ADAPTER	24	/* struct i2c_adapter * */
#define T40_CLIENT44_DEV_DRIVER	104	/* struct device_driver * */

/* stands in for the driver of a client no sensor module bound to */
static struct device_driver t40_no_driver;

struct t40_i2c_twin {
	u8 client44[T40_CLIENT44_SIZE] __aligned(4);
	struct i2c_client *real;
	struct module *pin;	/* the sensor module, held while the ISP owns the client */
};

static struct t40_i2c_twin *t40_i2c_twins[T40_MAX_TWINS];

static struct t40_i2c_twin *t40_i2c_twin_of(const void *p)
{
	int i;

	for (i = 0; i < T40_MAX_TWINS; i++) {
		struct t40_i2c_twin *tw = t40_i2c_twins[i];

		if (tw && (p == tw->client44 || p == tw->real))
			return tw;
	}
	return NULL;
}

/* the firmware's copy, or a real client the sensor driver stored itself */
static struct i2c_client *t40_i2c_real(const struct i2c_client *p)
{
	struct t40_i2c_twin *tw = t40_i2c_twin_of(p);

	return tw ? tw->real : (struct i2c_client *)p;
}

static int priv_i2c_register_driver(struct module *owner, struct i2c_driver *drv)
{
	/* the firmware's private_i2c_add_driver passes its own module */
	return i2c_register_driver(drv->driver.owner ?: owner, drv);
}

static struct i2c_client *priv_i2c_new_device(struct i2c_adapter *adap,
					      struct i2c_board_info const *info310)
{
	struct i2c_board_info info;
	struct i2c_client *real;
	struct t40_i2c_twin *tw;
	int i;

	memset(&info, 0, sizeof(info));
	memcpy(info.type, (const u8 *)info310 + 0, I2C_NAME_SIZE);
	info.flags = BLOB(info310, 20, unsigned short);
	info.addr = BLOB(info310, 22, unsigned short);
	info.platform_data = BLOB(info310, 24, void *);
	real = i2c_new_client_device(adap, &info);
	if (IS_ERR(real))
		return NULL;

	tw = kzalloc(sizeof(*tw), GFP_KERNEL);
	if (!tw)
		goto err_unregister;
	tw->real = real;
	BLOB(tw->client44, T40_CLIENT44_FLAGS, unsigned short) = real->flags;
	BLOB(tw->client44, T40_CLIENT44_ADDR, unsigned short) = real->addr;
	memcpy(tw->client44 + T40_CLIENT44_NAME, real->name, I2C_NAME_SIZE);
	BLOB(tw->client44, T40_CLIENT44_ADAPTER, struct i2c_adapter *) = real->adapter;
	/* i2c_new_client_device() probes synchronously, so the driver is bound by now */
	/*
	 * The firmware reads client->dev.driver->owner without a NULL check, so
	 * a sensor nobody drives (no module for that name) gets a driver stub:
	 * it then finds no client data, unregisters the client and fails.
	 */
	if (!real->dev.driver)
		pr_err("t40-isp: no sensor driver for %s at 0x%02x\n", real->name, real->addr);
	BLOB(tw->client44, T40_CLIENT44_DEV_DRIVER, void *) =
		real->dev.driver ? (void *)real->dev.driver : (void *)&t40_no_driver;
	/*
	 * The firmware only holds the sensor module across its own lookup, yet
	 * calls into the sensor's subdev until it drops the client: without
	 * this reference rmmod of the sensor frees them under a streaming ISP.
	 */
	if (real->dev.driver && try_module_get(real->dev.driver->owner))
		tw->pin = real->dev.driver->owner;

	mutex_lock(&t40_twin_lock);
	for (i = 0; i < T40_MAX_TWINS; i++) {
		if (!t40_i2c_twins[i]) {
			t40_i2c_twins[i] = tw;
			break;
		}
	}
	mutex_unlock(&t40_twin_lock);
	if (i == T40_MAX_TWINS) {
		module_put(tw->pin);
		kfree(tw);
		goto err_unregister;
	}
	return (struct i2c_client *)tw->client44;

err_unregister:
	i2c_unregister_device(real);
	return NULL;
}

static void *priv_i2c_get_clientdata(const struct i2c_client *client)
{
	return i2c_get_clientdata(t40_i2c_real(client));
}

static void priv_i2c_set_clientdata(struct i2c_client *client, void *data)
{
	i2c_set_clientdata(t40_i2c_real(client), data);
}

static void priv_i2c_unregister_device(struct i2c_client *client)
{
	struct t40_i2c_twin *tw = t40_i2c_twin_of(client);
	struct i2c_client *real = tw ? tw->real : client;
	struct module *pin = tw ? tw->pin : NULL;
	int i;

	if (tw) {
		mutex_lock(&t40_twin_lock);
		for (i = 0; i < T40_MAX_TWINS; i++)
			if (t40_i2c_twins[i] == tw)
				t40_i2c_twins[i] = NULL;
		mutex_unlock(&t40_twin_lock);
		kfree(tw);
	}
	i2c_unregister_device(real);
	module_put(pin);
}

/* ---- gpio: 3.10's PA..PD numbering is the pinctrl-ingenic bank * 32 ---------- */

static int priv_gpio_request(unsigned gpio, const char *label)
{
	return gpio_request(gpio, label);
}

static void priv_gpio_free(unsigned gpio)
{
	gpio_free(gpio);
}

static int priv_gpio_direction_output(unsigned gpio, int value)
{
	return gpio_direction_output(gpio, value);
}

static int priv_gpio_direction_input(unsigned gpio)
{
	return gpio_direction_input(gpio);
}

static int priv_gpio_set_debounce(unsigned gpio, unsigned debounce)
{
	return -ENOSYS;
}

/* pin function and pull belong to pinctrl on 7.x; the DT carries them */
static int priv_jzgpio_set_func(int port, enum gpio_function func,
				unsigned long pins)
{
	return 0;
}

/* ---- system, wait ----------------------------------------------------------- */

static void priv_msleep(unsigned int msecs)
{
	msleep(msecs);
}

static bool priv_capable(int cap)
{
	return capable(cap);
}

static unsigned long long priv_sched_clock(void)
{
	return ktime_get_ns();
}

static bool priv_try_module_get(struct module *module)
{
	return try_module_get(module);
}

static void priv_module_put(struct module *module)
{
	module_put(module);
}

static int priv_request_module(bool wait, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int ret;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	ret = __request_module(true, "%pV", &vaf);
	va_end(args);
	return ret;
}

static void priv_init_completion(struct completion *x)
{
	init_completion(x);
}

static void priv_complete(struct completion *x)
{
	complete(x);
}

static int priv_wait_for_completion_interruptible(struct completion *x)
{
	return wait_for_completion_interruptible(x);
}

static int priv_wait_event_interruptible(wait_queue_head_t *q, int (*state)(void *), void *data)
{
	return wait_event_interruptible(*q, state(data));
}

static void priv_wake_up_all(wait_queue_head_t *q)
{
	wake_up_all(q);
}

static void priv_wake_up(wait_queue_head_t *q)
{
	wake_up(q);
}

static void priv_init_waitqueue_head(wait_queue_head_t *q)
{
	init_waitqueue_head(q);
}

/* the firmware's jiffies are 3.10's HZ=100 */
static unsigned long priv_wait_for_completion_timeout(struct completion *x,
						      unsigned long timeout)
{
	return wait_for_completion_timeout(x, t40_jiffies_from_44(timeout));
}

/*
 * ---- miscdevice ------------------------------------------------------------
 *
 * The firmware embeds 4.4 miscdevices (40 bytes, the same layout as 7.x) in
 * its own objects and recovers those objects from file->private_data in its
 * fops. Register a native twin whose fops are the firmware's, each slot
 * wrapped by tx-isp-twin.c, so that file->private_data is what 4.4's
 * misc_open() left there: the firmware's own miscdevice. minor and
 * this_device are written back.
 */
#define T40_MISC44_MINOR	0	/* int */
#define T40_MISC44_NAME	4	/* const char * */
#define T40_MISC44_FOPS	8	/* const struct file_operations * */
#define T40_MISC44_PARENT	20	/* struct device * */
#define T40_MISC44_THIS_DEVICE	24	/* struct device * */
#define T40_MISC44_NODENAME	32	/* const char * */
#define T40_MISC44_MODE	36	/* umode_t */

struct t40_misc_twin {
	void *blob;				/* firmware's miscdevice */
	struct miscdevice twin;
	struct fw_fops fw;
};

static struct t40_misc_twin *t40_misc_twins[T40_MAX_TWINS];

static struct t40_misc_twin *t40_misc_twin_of(const void *blob)
{
	int i;

	for (i = 0; i < T40_MAX_TWINS; i++)
		if (t40_misc_twins[i] && t40_misc_twins[i]->blob == blob)
			return t40_misc_twins[i];
	return NULL;
}

static int priv_misc_register(struct miscdevice *misc44)
{
	struct t40_misc_twin *m;
	const void *blob_fops = BLOB(misc44, T40_MISC44_FOPS, const void *);
	struct device *parent = BLOB(misc44, T40_MISC44_PARENT, struct device *);
	struct t40_twin_dev *td = t40_dev_by_blob_dev(parent);
	int i, ret;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->blob = misc44;
	if (blob_fops)
		fw_fops_init(&m->fw, blob_fops, misc44);
	m->twin.minor = BLOB(misc44, T40_MISC44_MINOR, int);
	m->twin.name = BLOB(misc44, T40_MISC44_NAME, const char *);
	m->twin.fops = blob_fops ? &m->fw.fops : NULL;
	m->twin.parent = td ? &td->twin->dev : NULL;
	m->twin.nodename = BLOB(misc44, T40_MISC44_NODENAME, const char *);
	m->twin.mode = BLOB(misc44, T40_MISC44_MODE, umode_t);

	mutex_lock(&t40_twin_lock);
	for (i = 0; i < T40_MAX_TWINS; i++) {
		if (!t40_misc_twins[i]) {
			t40_misc_twins[i] = m;
			break;
		}
	}
	mutex_unlock(&t40_twin_lock);
	if (i == T40_MAX_TWINS) {
		kfree(m);
		return -ENOSPC;
	}

	ret = misc_register(&m->twin);
	if (ret) {
		mutex_lock(&t40_twin_lock);
		t40_misc_twins[i] = NULL;
		mutex_unlock(&t40_twin_lock);
		kfree(m);
		return ret;
	}
	BLOB(misc44, T40_MISC44_MINOR, int) = m->twin.minor;
	BLOB(misc44, T40_MISC44_THIS_DEVICE, struct device *) = m->twin.this_device;
	return 0;
}

static void priv_misc_deregister(struct miscdevice *misc44)
{
	struct t40_misc_twin *m = t40_misc_twin_of(misc44);
	int i;

	if (!m)
		return;
	misc_deregister(&m->twin);
	mutex_lock(&t40_twin_lock);
	for (i = 0; i < T40_MAX_TWINS; i++)
		if (t40_misc_twins[i] == m)
			t40_misc_twins[i] = NULL;
	mutex_unlock(&t40_twin_lock);
	kfree(m);
}

/* the firmware's own miscdevice reaches misc_deregister() directly */
void t40_misc_deregister_44(struct miscdevice *misc44)
{
	priv_misc_deregister(misc44);
}

/*
 * ---- cdev: the tuning node -------------------------------------------------
 *
 * The firmware calls cdev_init() directly with its 108-byte fops; the objcopy
 * redirects cdev_init/cdev_del here.
 */
struct t40_cdev_twin {
	struct cdev *cdev;
	struct fw_fops fw;
};

static struct t40_cdev_twin *t40_cdev_twins[T40_MAX_TWINS];

void t40_cdev_init_44(struct cdev *cdev, const void *fops44)
{
	struct t40_cdev_twin *tw = kzalloc(sizeof(*tw), GFP_KERNEL);
	int i;

	if (!tw) {
		cdev_init(cdev, NULL);
		return;
	}
	tw->cdev = cdev;
	fw_fops_init(&tw->fw, fops44, NULL);
	mutex_lock(&t40_twin_lock);
	for (i = 0; i < T40_MAX_TWINS; i++) {
		if (!t40_cdev_twins[i]) {
			t40_cdev_twins[i] = tw;
			break;
		}
	}
	mutex_unlock(&t40_twin_lock);
	if (i == T40_MAX_TWINS) {
		kfree(tw);
		cdev_init(cdev, NULL);
		return;
	}
	cdev_init(cdev, &tw->fw.fops);
}

/*
 * The firmware tears the tuning node down from both its slake and its remove
 * path, so the second call finds nothing alive; 4.4 tolerated that by
 * accident. Only a cdev that cdev_init() registered is deleted.
 */
void t40_cdev_del_44(struct cdev *cdev)
{
	int i;
	bool live = false;

	mutex_lock(&t40_twin_lock);
	for (i = 0; i < T40_MAX_TWINS; i++) {
		if (t40_cdev_twins[i] && t40_cdev_twins[i]->cdev == cdev) {
			kfree(t40_cdev_twins[i]);
			t40_cdev_twins[i] = NULL;
			live = true;
		}
	}
	mutex_unlock(&t40_twin_lock);
	if (live)
		cdev_del(cdev);
}

/*
 * ---- /proc -----------------------------------------------------------------
 *
 * proc_create_data() takes a proc_ops since 5.6; tx-isp-twin.c builds one
 * from the firmware's fops. Its open handlers read their data back with
 * PDE_DATA(), redirected to t40_pde_data_44().
 */
static struct proc_dir_entry *t40_jz_root;

struct proc_dir_entry *jz_proc_mkdir(char *s)
{
	return t40_jz_root ? proc_mkdir(s, t40_jz_root) : NULL;
}

static struct proc_dir_entry *priv_proc_create_data(const char *name, umode_t mode,
						    struct proc_dir_entry *parent,
						    const void *fops44,
						    void *data)
{
	return t40_proc_create_data(name, mode, parent, fops44, data);
}

static void priv_proc_remove(struct proc_dir_entry *de)
{
	t40_proc_remove(de);
}

static struct proc_dir_entry *priv_jz_proc_mkdir(char *s)
{
	return jz_proc_mkdir(s);
}

static int priv_single_open_size(struct file *file, int (*show)(struct seq_file *, void *),
				 void *data, size_t size)
{
	return t40_single_open_size(file, show, data, size);
}

static unsigned long long priv_simple_strtoull(const char *cp, char **endp,
					       unsigned int base)
{
	return simple_strtoull(cp, endp, base);
}

static bool priv_kthread_should_stop(void)
{
	return kthread_should_stop();
}

static bool fw_rt = true;
module_param(fw_rt, bool, 0444);
MODULE_PARM_DESC(fw_rt, "run the firmware's event thread at the lowest RT priority");

static struct task_struct *priv_kthread_run(int (*threadfn)(void *data), void *data,
					    const char namefmt[])
{
	struct task_struct *k = kthread_run(threadfn, data, "%s", namefmt);

	/*
	 * The firmware's thread drains the event pool the ISP interrupt fills
	 * (20 events, one per AE/AWB/WDR update). Left at SCHED_OTHER it loses
	 * the CPU to the RT irq threads and the console for long enough on a
	 * slow machine to run the pool dry; below the irq threads it still
	 * runs ahead of every user task.
	 */
	if (!IS_ERR(k) && fw_rt)
		sched_set_fifo_low(k);
	return k;
}

static int priv_kthread_stop(struct task_struct *k)
{
	return kthread_stop(k);
}

static long priv_copy_from_user(void *to, const void __user *from, long size)
{
	return copy_from_user(to, from, size);
}

static long priv_copy_to_user(void __user *to, const void *from, long size)
{
	return copy_to_user(to, from, size);
}

static void priv_get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size)
{
	*phyaddr = ispmem_base;
	*size = ispmem_size;
}

/*
 * ---- the private_* interface ----------------------------------------------
 *
 * The firmware and the sensor drivers call these by name; the firmware
 * carries weak no-op copies that these definitions replace at link time.
 * Entries that take an object the firmware allocated with a 4.4 layout go
 * through the twins above.
 */
#define PRIV(ret, name, args, call) \
	ret private_##name args { return priv_##name call; }
#define PRIVV(name, args, call) \
	void private_##name args { priv_##name call; }

int private_platform_driver_register(struct platform_driver *drv)
{
	return t40_platform_driver_register_44(drv, THIS_MODULE);
}
PRIVV(platform_driver_unregister, (struct platform_driver *drv), (drv))
PRIVV(platform_set_drvdata, (struct platform_device *pdev, void *data), (pdev, data))
void *private_platform_get_drvdata(struct platform_device *pdev)
{
	return priv_platform_get_drvdata(pdev);
}
PRIV(int, platform_device_register, (struct platform_device *pdev), (pdev))
PRIVV(platform_device_unregister, (struct platform_device *pdev), (pdev))
PRIV(struct resource *, platform_get_resource,
     (struct platform_device *pdev, unsigned int type, unsigned int num), (pdev, type, num))
PRIVV(dev_set_drvdata, (struct device *dev, void *data), (dev, data))
void *private_dev_get_drvdata(const struct device *dev)
{
	return priv_dev_get_drvdata(dev);
}
PRIV(int, platform_get_irq, (struct platform_device *pdev, unsigned int num), (pdev, num))
PRIV(struct resource *, request_mem_region,
     (resource_size_t start, resource_size_t n, const char *name), (start, n, name))
PRIVV(release_mem_region, (resource_size_t start, resource_size_t n), (start, n))
PRIV(void __iomem *, ioremap, (phys_addr_t offset, unsigned long size), (offset, size))
PRIVV(iounmap, (const volatile void __iomem *addr), (addr))

/*
 * The firmware requests an interrupt before the object its handler reads is
 * set up and disables it right after the request. A line the hardware still
 * asserts (IVDC after an unload) fires in between and the handler
 * dereferences NULL. Request it disabled instead and let the firmware's
 * first disable_irq() of that line, which would unbalance the count, pass.
 */
static DEFINE_SPINLOCK(t40_irq_lock);
static unsigned int t40_irq_quiet[8];

static bool t40_irq_quiet_take(unsigned int irq, bool take)
{
	bool found = false;
	int i;

	spin_lock(&t40_irq_lock);
	for (i = 0; i < ARRAY_SIZE(t40_irq_quiet); i++) {
		if (t40_irq_quiet[i] == irq) {
			found = true;
			if (take)
				t40_irq_quiet[i] = 0;
			break;
		}
	}
	spin_unlock(&t40_irq_lock);
	return found;
}

static void t40_irq_quiet_add(unsigned int irq)
{
	int i;

	spin_lock(&t40_irq_lock);
	for (i = 0; i < ARRAY_SIZE(t40_irq_quiet); i++) {
		if (!t40_irq_quiet[i]) {
			t40_irq_quiet[i] = irq;
			break;
		}
	}
	spin_unlock(&t40_irq_lock);
}

/*
 * The firmware frees an interrupt only when a flag it sets elsewhere is
 * set, so a module that never got to that point leaves its handlers
 * registered, and the next load cannot request the line. The requests are
 * recorded here, dropped by the firmware's own free_irq() and whatever is
 * left is freed when the module exits.
 */
#define T40_IRQ_TRACK	16

static struct {
	unsigned int irq;
	void *dev;
} t40_irq_track[T40_IRQ_TRACK];

static void t40_irq_track_set(unsigned int irq, void *dev, bool add)
{
	int i;

	spin_lock(&t40_irq_lock);
	for (i = 0; i < T40_IRQ_TRACK; i++) {
		if (add && !t40_irq_track[i].irq) {
			t40_irq_track[i].irq = irq;
			t40_irq_track[i].dev = dev;
			break;
		}
		if (!add && t40_irq_track[i].irq == irq && t40_irq_track[i].dev == dev) {
			t40_irq_track[i].irq = 0;
			break;
		}
	}
	spin_unlock(&t40_irq_lock);
}

static void t40_irq_track_free_all(void)
{
	int i;

	for (i = 0; i < T40_IRQ_TRACK; i++) {
		if (t40_irq_track[i].irq) {
			pr_warn("t40-isp: freeing irq %u the firmware left\n", t40_irq_track[i].irq);
			free_irq(t40_irq_track[i].irq, t40_irq_track[i].dev);
			t40_irq_track[i].irq = 0;
		}
	}
}

int private_request_threaded_irq(unsigned int irq, irq_handler_t handler,
				 irq_handler_t thread_fn, unsigned long flags,
				 const char *name, void *dev)
{
	int ret;

	t40_irq_quiet_add(irq);
	ret = request_threaded_irq(irq, handler, thread_fn, flags | IRQF_NO_AUTOEN, name, dev);
	if (ret)
		t40_irq_quiet_take(irq, true);
	else
		t40_irq_track_set(irq, dev, true);
	return ret;
}
void private_enable_irq(unsigned int irq) { enable_irq(irq); }
void private_disable_irq(unsigned int irq)
{
	if (t40_irq_quiet_take(irq, true))
		return;
	disable_irq(irq);
}
void private_free_irq(unsigned int irq, void *dev_id)
{
	t40_irq_track_set(irq, dev_id, false);
	priv_free_irq(irq, dev_id);
}

void __private_spin_lock_irqsave(spinlock_t *lock, unsigned long *flags)
{
	priv_spin_lock_irqsave(lock, flags);
}
PRIVV(spin_unlock_irqrestore, (spinlock_t *lock, unsigned long flags), (lock, flags))
PRIVV(spin_lock_init, (spinlock_t *lock), (lock))
void private_mutex_lock(struct mutex *lock) { mutex_lock(lock); }
void private_mutex_unlock(struct mutex *lock) { mutex_unlock(lock); }
PRIVV(raw_mutex_init, (struct mutex *lock, const char *name, struct lock_class_key *key),
      (lock, name, key))

PRIV(struct clk *, clk_get, (struct device *dev, const char *id), (dev, id))
struct clk *private_devm_clk_get(struct device *dev, const char *id)
{
	return priv_clk_get(dev, id);
}
PRIV(int, clk_enable, (struct clk *clk), (clk))
int private_clk_prepare_enable(struct clk *clk) { return priv_clk_enable(clk); }
PRIVV(clk_disable, (struct clk *clk), (clk))
void private_clk_disable_unprepare(struct clk *clk) { priv_clk_disable(clk); }
PRIV(unsigned long, clk_get_rate, (struct clk *clk), (clk))
PRIVV(clk_put, (struct clk *clk), (clk))
void private_devm_clk_put(struct device *dev, struct clk *clk) { priv_clk_put(clk); }
PRIV(int, clk_set_rate, (struct clk *clk, unsigned long rate), (clk, rate))

struct i2c_adapter *private_i2c_get_adapter(int nr) { return i2c_get_adapter(nr); }
void private_i2c_put_adapter(struct i2c_adapter *adap) { i2c_put_adapter(adap); }
int private_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	return i2c_transfer(adap, msgs, num);
}
int private_i2c_register_driver(struct module *owner, struct i2c_driver *drv)
{
	return priv_i2c_register_driver(owner, drv);
}
int private_i2c_add_driver(struct i2c_driver *drv)
{
	return priv_i2c_register_driver(THIS_MODULE, drv);
}
void private_i2c_del_driver(struct i2c_driver *drv) { i2c_del_driver(drv); }
PRIV(struct i2c_client *, i2c_new_device,
     (struct i2c_adapter *adap, struct i2c_board_info const *info), (adap, info))
PRIVV(i2c_set_clientdata, (struct i2c_client *c, void *data), (c, data))
void *private_i2c_get_clientdata(const struct i2c_client *c)
{
	return priv_i2c_get_clientdata(c);
}
PRIVV(i2c_unregister_device, (struct i2c_client *c), (c))

PRIV(int, gpio_request, (unsigned gpio, const char *label), (gpio, label))
PRIVV(gpio_free, (unsigned gpio), (gpio))
PRIV(int, gpio_direction_output, (unsigned gpio, int value), (gpio, value))
PRIV(int, gpio_direction_input, (unsigned gpio), (gpio))
PRIV(int, gpio_set_debounce, (unsigned gpio, unsigned debounce), (gpio, debounce))
int private_jzgpio_set_func(enum gpio_port port, enum gpio_function func,
			    unsigned long pins)
{
	return priv_jzgpio_set_func(port, func, pins);
}

PRIVV(msleep, (unsigned int msecs), (msecs))
void private_mdelay(unsigned int msecs) { mdelay(msecs); }
PRIV(bool, capable, (int cap), (cap))
PRIV(unsigned long long, sched_clock, (void), ())
PRIV(bool, try_module_get, (struct module *m), (m))
PRIVV(module_put, (struct module *m), (m))
int private_request_module(bool wait, const char *fmt, ...)
{
	return 0;
}

PRIVV(init_completion, (struct completion *x), (x))
PRIVV(complete, (struct completion *x), (x))
PRIV(int, wait_for_completion_interruptible, (struct completion *x), (x))
PRIV(unsigned long, wait_for_completion_timeout,
     (struct completion *x, unsigned long timeout), (x, timeout))
PRIV(int, wait_event_interruptible,
     (wait_queue_head_t *q, int (*state)(void *), void *data), (q, state, data))
PRIVV(wake_up_all, (wait_queue_head_t *q), (q))
PRIVV(wake_up, (wait_queue_head_t *q), (q))
PRIVV(init_waitqueue_head, (wait_queue_head_t *q), (q))

PRIV(int, misc_register, (struct miscdevice *m), (m))
PRIVV(misc_deregister, (struct miscdevice *m), (m))

struct proc_dir_entry *private_proc_create_data(const char *name, umode_t mode,
						struct proc_dir_entry *parent,
						const struct file_operations *fops44,
						void *data)
{
	return priv_proc_create_data(name, mode, parent, fops44, data);
}
struct proc_dir_entry *private_proc_create(const char *name, umode_t mode,
					   struct proc_dir_entry *parent,
					   const struct file_operations *fops44)
{
	return priv_proc_create_data(name, mode, parent, fops44, NULL);
}
void private_remove_proc_entry(const char *name, struct proc_dir_entry *parent)
{
	remove_proc_entry(name, parent);
}
ssize_t private_seq_read(struct file *f, char __user *buf, size_t size, loff_t *ppos)
{
	return t40_seq_read(f, buf, size, ppos);
}
loff_t private_seq_lseek(struct file *f, loff_t offset, int whence)
{
	return t40_seq_lseek(f, offset, whence);
}
int private_single_release(struct inode *inode, struct file *f)
{
	return t40_single_release(inode, f);
}
PRIV(int, single_open_size, (struct file *file, int (*show)(struct seq_file *, void *),
			   void *data, size_t size), (file, show, data, size))
PRIV(struct proc_dir_entry *, jz_proc_mkdir, (char *s), (s))
PRIVV(proc_remove, (struct proc_dir_entry *de), (de))
void private_seq_printf(struct seq_file *m, const char *fmt, ...)
{
	struct seq_file *real = t40_seq_real(m);
	va_list args;

	va_start(args, fmt);
	seq_vprintf(real, fmt, args);
	va_end(args);
}
unsigned long long private_simple_strtoull(const char *cp, char **endp, unsigned int base)
{
	return simple_strtoull(cp, endp, base);
}

bool private_kthread_should_stop(void) { return kthread_should_stop(); }
struct task_struct *private_kthread_run(int (*threadfn)(void *data), void *data,
					const char namefmt[])
{
	return priv_kthread_run(threadfn, data, namefmt);
}
int private_kthread_stop(struct task_struct *k) { return kthread_stop(k); }

void *private_vmalloc(unsigned long size)
{
	if (!size)
		return NULL;
	return t40_alloc_track_ext(size >= PAGE_SIZE ? vmalloc(size) : kmalloc(size, GFP_KERNEL));
}
void private_vfree(const void *addr)
{
	if (!t40_alloc_untrack(addr))
		return;
	if (is_vmalloc_addr(addr))
		vfree(addr);
	else
		kfree(addr);
}
void *private_kmalloc(size_t s, gfp_t gfp) { return t40_priv_kmalloc(s, gfp); }
void private_kfree(void *p) { t40_kfree_44(p); }
long private_copy_from_user(void *to, const void __user *from, long size)
{
	return copy_from_user(to, from, size);
}
long private_copy_to_user(void __user *to, const void *from, long size)
{
	return copy_to_user(to, from, size);
}

struct file *private_filp_open(const char *name, int flags, umode_t mode)
{
	return t40_filp_open(name, flags, mode);
}
int private_filp_close(struct file *f, fl_owner_t id) { return t40_filp_close(f, id); }
/* the firmware's file is tx-isp-twin.c's 4.4-shaped twin; its inode carries i_size at 48 */
struct inode *private_file_inode(struct file *f)
{
	return FW(f, FILE44_F_INODE, struct inode *);
}
loff_t private_i_size_read(const struct inode *inode)
{
	return FW(inode, INODE44_I_SIZE, loff_t);
}
ssize_t private_vfs_read(struct file *f, char __user *buf, size_t count, loff_t *pos)
{
	return t40_vfs_read(f, buf, count, pos);
}
ssize_t private_vfs_write(struct file *f, const char __user *buf, size_t count, loff_t *pos)
{
	return t40_vfs_write(f, buf, count, pos);
}
loff_t private_vfs_llseek(struct file *f, loff_t offset, int whence)
{
	return t40_vfs_llseek(f, offset, whence);
}
void private_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
			    enum dma_data_direction dir)
{
	t40_priv_dma_cache_sync(dev, vaddr, size, dir);
}
void private_getrawmonotonic(struct timespec *ts)
{
	struct timespec64 t;

	ktime_get_raw_ts64(&t);
	ts->tv_sec = t.tv_sec;
	ts->tv_nsec = t.tv_nsec;
}
struct net *private_get_init_net(void) { return t40_priv_get_init_net(); }
ktime_t private_ktime_set(const long secs, const unsigned long nsecs)
{
	return ktime_set(secs, nsecs);
}
void private_set_current_state(unsigned int state) { __set_current_state(state); }
int private_schedule_hrtimeout(ktime_t *ex, const enum hrtimer_mode mode)
{
	return schedule_hrtimeout(ex, mode);
}
bool private_schedule_work(struct work_struct *work)
{
	return t40_queue_work_44(work);
}
bool private_flush_work(struct work_struct *work) { return flush_work(work); }
void private_do_gettimeofday(struct timeval *tv)
{
	struct timespec64 t;

	ktime_get_real_ts64(&t);
	tv->tv_sec = t.tv_sec;
	tv->tv_usec = t.tv_nsec / 1000;
}
void private_get_isp_priv_mem(unsigned int *phyaddr, unsigned int *size)
{
	priv_get_isp_priv_mem(phyaddr, size);
}

/* ---- init ------------------------------------------------------------------- */

/*
 * The ISP node is never bound by a driver, so nothing applies its pin state
 * (the CIM MCLK pad). A placeholder device on that node holds it while the
 * module is loaded; the twins stay off the node so their IRQs keep coming
 * from their own resources.
 */
static struct platform_device *t40_pins_dev;
static struct pinctrl *t40_pins;

static int t40_pins_get(void)
{
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "ingenic,t40-isp");
	if (!np)
		return -ENODEV;
	t40_pins_dev = platform_device_alloc("t40-isp-pins", PLATFORM_DEVID_NONE);
	if (!t40_pins_dev) {
		of_node_put(np);
		return -ENOMEM;
	}
	t40_pins_dev->dev.of_node = np;		/* reference passes to the device */
	ret = platform_device_add(t40_pins_dev);
	if (ret)
		goto err_put;
	t40_pins = pinctrl_get_select_default(&t40_pins_dev->dev);
	if (IS_ERR(t40_pins)) {
		ret = PTR_ERR(t40_pins);
		t40_pins = NULL;
		platform_device_unregister(t40_pins_dev);
		t40_pins_dev = NULL;
		return ret;
	}
	return 0;

err_put:
	platform_device_put(t40_pins_dev);
	t40_pins_dev = NULL;
	return ret;
}

static void t40_pins_put(void)
{
	if (t40_pins) {
		pinctrl_put(t40_pins);
		t40_pins = NULL;
	}
	if (t40_pins_dev) {
		platform_device_unregister(t40_pins_dev);
		t40_pins_dev = NULL;
	}
}

void tx_isp_abi_exit(void)
{
	int i;

	t40_irq_track_free_all();
	t40_alloc_free_all();
	{
		unsigned long idx;
		void *c;

		xa_for_each(&t40_clks, idx, c)
			clk_put(c);
		xa_destroy(&t40_clks);
	}
	t40_pins_put();

	for (i = 0; i < ARRAY_SIZE(t40_clk_lookups); i++) {
		struct clk_lookup *cl = t40_clk_lookups[i];

		if (!cl)
			continue;
		t40_clk_lookups[i] = NULL;
		clkdev_drop(cl);
	}
	if (t40_jz_root) {
		proc_remove(t40_jz_root);
		t40_jz_root = NULL;
	}
}

int tx_isp_abi_init(void)
{
	int i, ret;

	BUILD_BUG_ON(sizeof(struct miscdevice) != 40);
	BUILD_BUG_ON(offsetof(struct miscdevice, mode) != T40_MISC44_MODE);
	BUILD_BUG_ON(offsetof(struct miscdevice, nodename) != T40_MISC44_NODENAME);
	BUILD_BUG_ON(offsetof(struct miscdevice, this_device) != T40_MISC44_THIS_DEVICE);
	BUILD_BUG_ON(offsetof(struct miscdevice, parent) != T40_MISC44_PARENT);
	BUILD_BUG_ON(offsetof(struct device_driver, owner) != 8);

	ret = t40_pins_get();
	if (ret)
		pr_warn("t40-isp: no MCLK pin state from the ISP node (%d)\n", ret);

	t40_jz_root = proc_mkdir("jz", NULL);
	if (!t40_jz_root)
		pr_warn("t40-isp: /proc/jz exists or cannot be created\n");

	for (i = 0; i < ARRAY_SIZE(t40_clk_aliases); i++) {
		struct clk *clk = t40_dt_clk(i);
		struct clk_lookup *cl;

		if (IS_ERR(clk)) {
			pr_err("t40-isp: no clock for alias %s (%ld)\n",
			       t40_clk_aliases[i].alias, PTR_ERR(clk));
			ret = PTR_ERR(clk);
			goto err;
		}
		cl = clkdev_create(clk, t40_clk_aliases[i].alias, NULL);
		clk_put(clk);
		if (!cl) {
			ret = -ENOMEM;
			goto err;
		}
		t40_clk_lookups[i] = cl;
	}
	return 0;

err:
	tx_isp_abi_exit();
	return ret;
}
