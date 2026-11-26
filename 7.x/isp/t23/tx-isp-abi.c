// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-abi.c - Linux 4.4.94 ABI adapter for the T23 ISP firmware
 *
 * libt23-firmware-540-4494.a (1.3.0) was built against Linux 4.4.94 and has
 * no 7.x build. It reaches the kernel through three doors; this file,
 * txx-funcs.c and tx-isp-twin.c cover them:
 *
 *  1. The jz_driver_common_interfaces table at the bottom of this file, which
 *     the firmware reads by offset. Entries that receive an object the
 *     firmware allocated with a 4.4 layout (platform_device,
 *     platform_driver, miscdevice, file_operations, resource, board_info)
 *     map it to a kernel-owned twin, and the one entry that returns a kernel
 *     object the firmware reads by offset (i2c_client) hands back a
 *     4.4-shaped twin. The kernel never touches a firmware-owned struct and
 *     the firmware never sees a 7.x one.
 *  2. Kernel-allocated objects the firmware reads in code the compiler
 *     inlined (file, seq_file, inode, thread_info): the firmware reaches
 *     them only through its file_operations tables, so tx-isp-twin.c
 *     hands each handler a 4.4-shaped twin of the object instead.
 *  3. Direct imports whose semantics or layout changed: the Kbuild redirects
 *     them with objcopy to the t23_*_310 / t23_*_44 translators.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
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
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/kthread.h>
#include <linux/pfn.h>
#include <linux/dma-mapping.h>
#include <linux/soc/ingenic/rmem.h>
#include <asm/thread_info.h>
#include <dt-bindings/clock/ingenic,t23-cgu.h>

#include <txx-funcs.h>
#include <tx-isp-device.h>
#include <tx-isp-debug.h>
#include <soc/gpio.h>
#include "tx-isp-abi.h"

#define BLOB(base, off, type)	(*(type *)((u8 *)(base) + (off)))

#define T23_MAX_TWINS		16

/* writers hold t23_twin_lock; readers, some in IRQ context, are lock-free */
static DEFINE_MUTEX(t23_twin_lock);

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
#define T23_PDEV44_NAME	0	/* const char * */
#define T23_PDEV44_ID		4	/* int */
#define T23_PDEV44_DEV		16	/* struct device, 208 bytes */
#define T23_PDEV44_NUM_RES	224	/* u32 */
#define T23_PDEV44_RES		228	/* struct resource *, 28-byte stride */
#define T23_DEV44_INIT_NAME	44	/* const char * */
#define T23_DEV44_DRIVER	68	/* struct device_driver * */
#define T23_DEV44_DMA_MASK	108	/* u64 * */
#define T23_DEV44_COHERENT	112	/* u64 */
#define T23_DEV44_DRIVER_DATA	76	/* void *, read inline by private_platform_get_drvdata */
#define T23_RES44_SIZE		28
#define T23_RES44_START	0
#define T23_RES44_END		4
#define T23_RES44_NAME		8
#define T23_RES44_FLAGS	12
#define T23_PDRV44_PROBE	0	/* int (*)(struct platform_device *) */
#define T23_PDRV44_REMOVE	4
#define T23_PDRV44_DRIVER	20	/* struct device_driver; .name at +0, .owner at +8 */

typedef int (*t23_pdrv_fn)(void *);

struct t23_twin_dev {
	void *blob;			/* firmware's platform_device */
	struct platform_device *twin;
};

struct t23_twin_drv {
	void *blob;			/* firmware's platform_driver */
	struct platform_driver twin;
};

static struct t23_twin_dev t23_twin_devs[T23_MAX_TWINS];
static struct t23_twin_drv t23_twin_drvs[T23_MAX_TWINS];

static struct t23_twin_dev *t23_dev_by_blob(const void *blob)
{
	int i;

	if (!blob)
		return NULL;
	for (i = 0; i < T23_MAX_TWINS; i++)
		if (t23_twin_devs[i].blob == blob)
			return &t23_twin_devs[i];
	return NULL;
}

static struct t23_twin_dev *t23_dev_by_twin(const struct platform_device *pdev)
{
	int i;

	for (i = 0; i < T23_MAX_TWINS; i++)
		if (t23_twin_devs[i].blob && t23_twin_devs[i].twin == pdev)
			return &t23_twin_devs[i];
	return NULL;
}

/* the firmware passes &pdev->dev, which is its object + 16 */
static struct t23_twin_dev *t23_dev_by_blob_dev(const struct device *dev)
{
	return dev ? t23_dev_by_blob((const u8 *)dev - T23_PDEV44_DEV) : NULL;
}

static struct t23_twin_drv *t23_drv_by_blob(const void *blob)
{
	int i;

	if (!blob)
		return NULL;
	for (i = 0; i < T23_MAX_TWINS; i++)
		if (t23_twin_drvs[i].blob == blob)
			return &t23_twin_drvs[i];
	return NULL;
}

/* firmware device pointer for a native struct device, or NULL */
void *t23_blob_dev_of(const struct device *dev)
{
	int i;

	for (i = 0; i < T23_MAX_TWINS; i++)
		if (t23_twin_devs[i].blob && &t23_twin_devs[i].twin->dev == dev)
			return t23_twin_devs[i].blob;
	return NULL;
}

/* drvdata of a firmware-owned struct device, which the firmware reads back through dev_get_drvdata() */
void *t23_dev_get_drvdata(const struct device *dev)
{
	struct t23_twin_dev *td = t23_dev_by_blob_dev(dev);

	return dev_get_drvdata(td ? &td->twin->dev : dev);
}

/* any live twin, for the DMA API calls that need a real device */
struct device *t23_any_dev(void)
{
	int i;

	for (i = 0; i < T23_MAX_TWINS; i++)
		if (t23_twin_devs[i].blob)
			return &t23_twin_devs[i].twin->dev;
	return NULL;
}

static int t23_twin_probe(struct platform_device *pdev)
{
	struct t23_twin_dev *td = t23_dev_by_twin(pdev);
	struct t23_twin_drv *tr = container_of(to_platform_driver(pdev->dev.driver),
					       struct t23_twin_drv, twin);
	t23_pdrv_fn probe = BLOB(tr->blob, T23_PDRV44_PROBE, t23_pdrv_fn);

	if (!td || !probe)
		return -ENODEV;
	/* what 3.10 would have left in the firmware's own object */
	BLOB(td->blob, T23_PDEV44_DEV + T23_DEV44_DRIVER, void *) =
		(u8 *)tr->blob + T23_PDRV44_DRIVER;
	return probe(td->blob);
}

/*
 * tisp_deinit() runs unconditionally from the core's remove, but the event
 * lists it pushes to are only built by tisp_init(), i.e. once a sensor was
 * attached. Unloading before that would follow NULL list pointers, so give
 * the lists their empty state first (the Kbuild exports tevent_info; the
 * heads are the busy list at word 244 and the free list at word 246).
 */
extern unsigned int tevent_info[];
#define TEVENT_BUSY	244
#define TEVENT_FREE	246

static void t23_event_lists_idle(void)
{
	if (tevent_info[TEVENT_FREE])
		return;
	tevent_info[TEVENT_BUSY] = tevent_info[TEVENT_BUSY + 1] =
		(unsigned int)&tevent_info[TEVENT_BUSY];
	tevent_info[TEVENT_FREE] = tevent_info[TEVENT_FREE + 1] =
		(unsigned int)&tevent_info[TEVENT_FREE];
}

static void t23_twin_remove(struct platform_device *pdev)
{
	struct t23_twin_dev *td = t23_dev_by_twin(pdev);
	struct t23_twin_drv *tr = container_of(to_platform_driver(pdev->dev.driver),
					       struct t23_twin_drv, twin);
	t23_pdrv_fn remove = BLOB(tr->blob, T23_PDRV44_REMOVE, t23_pdrv_fn);

	if (!td)
		return;
	/* tx-isp's remove tears the core down, so this is the first to run */
	t23_event_lists_idle();
	if (remove)
		remove(td->blob);
	BLOB(td->blob, T23_PDEV44_DEV + T23_DEV44_DRIVER, void *) = NULL;
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
} t23_irq_map[] = {
	{ "isp-m0", "ingenic,t23-isp", "isp" },
	{ "isp-w02", "ingenic,t23-isp", "vic" },
	{ "isp-ivdc", "ingenic,t23-ivdc", "ivdc" },
};

static int t23_dt_irq(const char *devname)
{
	int i, irq = -ENXIO;

	for (i = 0; i < ARRAY_SIZE(t23_irq_map); i++) {
		struct device_node *np;

		if (strcmp(devname, t23_irq_map[i].dev))
			continue;
		np = of_find_compatible_node(NULL, NULL, t23_irq_map[i].compat);
		if (!np)
			return -ENODEV;
		irq = of_irq_get_byname(np, t23_irq_map[i].irq_name);
		of_node_put(np);
		break;
	}
	return irq;
}

static int priv_platform_device_register(struct platform_device *pdev)
{
	void *blob = pdev;
	const char *name = BLOB(blob, T23_PDEV44_NAME, const char *);
	struct t23_twin_dev *td = NULL;
	struct platform_device *twin;
	struct resource *res = NULL;
	u32 nres = BLOB(blob, T23_PDEV44_NUM_RES, u32);
	void *res310 = BLOB(blob, T23_PDEV44_RES, void *);
	int i, ret;

	twin = platform_device_alloc(name, BLOB(blob, T23_PDEV44_ID, int));
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
			u8 *r = (u8 *)res310 + i * T23_RES44_SIZE;

			res[i].start = BLOB(r, T23_RES44_START, u32);
			res[i].end = BLOB(r, T23_RES44_END, u32);
			res[i].name = BLOB(r, T23_RES44_NAME, const char *);
			res[i].flags = BLOB(r, T23_RES44_FLAGS, u32);
			pr_debug("t23-isp: %s res %d: %#llx..%#llx flags %#lx\n", name, i,
				 (u64)res[i].start, (u64)res[i].end, res[i].flags);
			if (res[i].flags & IORESOURCE_IRQ) {
				int irq = t23_dt_irq(name);

				if (irq <= 0) {
					pr_err("t23-isp: no DT irq for %s (%d)\n", name, irq);
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
	twin->dev.dma_mask = BLOB(blob, T23_PDEV44_DEV + T23_DEV44_DMA_MASK, u64 *);
	twin->dev.coherent_dma_mask = BLOB(blob, T23_PDEV44_DEV + T23_DEV44_COHERENT, u64);
	/* keep dev_name() meaningful on the firmware's own object */
	BLOB(blob, T23_PDEV44_DEV + T23_DEV44_INIT_NAME, const char *) = name;

	mutex_lock(&t23_twin_lock);
	for (i = 0; i < T23_MAX_TWINS; i++) {
		if (!t23_twin_devs[i].blob) {
			td = &t23_twin_devs[i];
			td->twin = twin;
			td->blob = blob;	/* last: publishes the entry */
			break;
		}
	}
	mutex_unlock(&t23_twin_lock);
	if (!td) {
		ret = -ENOSPC;
		goto err_put;
	}

	ret = platform_device_add(twin);
	if (ret) {
		mutex_lock(&t23_twin_lock);
		td->blob = NULL;
		td->twin = NULL;
		mutex_unlock(&t23_twin_lock);
		goto err_put;
	}
	kfree(res);
	pr_info("t23-isp: twin %s (%u resources)\n",
		dev_name(&twin->dev), nres);
	return 0;

err_put:
	kfree(res);
	platform_device_put(twin);
	return ret;
}

static void priv_platform_device_unregister(struct platform_device *pdev)
{
	struct t23_twin_dev *td = t23_dev_by_blob(pdev);
	struct platform_device *twin;

	if (!td)
		return;
	twin = td->twin;
	platform_device_unregister(twin);
	mutex_lock(&t23_twin_lock);
	td->blob = NULL;
	td->twin = NULL;
	mutex_unlock(&t23_twin_lock);
}

/* the firmware imports __platform_driver_register(); the Kbuild redirects it here */
int t23_platform_driver_register_44(struct platform_driver *drv, struct module *owner)
{
	void *blob = drv;
	struct t23_twin_drv *tr = NULL;
	int i, ret;

	mutex_lock(&t23_twin_lock);
	for (i = 0; i < T23_MAX_TWINS; i++) {
		if (!t23_twin_drvs[i].blob) {
			tr = &t23_twin_drvs[i];
			memset(&tr->twin, 0, sizeof(tr->twin));
			tr->twin.driver.name = BLOB(blob, T23_PDRV44_DRIVER, const char *);
			tr->twin.probe = t23_twin_probe;
			tr->twin.remove = t23_twin_remove;
			tr->blob = blob;
			break;
		}
	}
	mutex_unlock(&t23_twin_lock);
	if (!tr)
		return -ENOSPC;

	/* the owner is the firmware's __this_module, i.e. this module */
	ret = __platform_driver_register(&tr->twin, owner, KBUILD_MODNAME);
	if (ret) {
		mutex_lock(&t23_twin_lock);
		tr->blob = NULL;
		mutex_unlock(&t23_twin_lock);
	}
	return ret;
}

static void priv_platform_driver_unregister(struct platform_driver *drv)
{
	struct t23_twin_drv *tr = t23_drv_by_blob(drv);

	if (!tr)
		return;
	platform_driver_unregister(&tr->twin);
	mutex_lock(&t23_twin_lock);
	tr->blob = NULL;
	mutex_unlock(&t23_twin_lock);
}

static struct resource *priv_platform_get_resource(struct platform_device *pdev,
						   unsigned int type, unsigned int num)
{
	struct t23_twin_dev *td = t23_dev_by_blob(pdev);

	return td ? platform_get_resource(td->twin, type, num) : NULL;
}

static int priv_platform_get_irq(struct platform_device *pdev, unsigned int num)
{
	struct t23_twin_dev *td = t23_dev_by_blob(pdev);

	return td ? platform_get_irq_optional(td->twin, num) : -ENODEV;
}

static void priv_platform_set_drvdata(struct platform_device *pdev, void *data)
{
	struct t23_twin_dev *td = t23_dev_by_blob(pdev);

	if (td) {
		platform_set_drvdata(td->twin, data);
		BLOB(pdev, T23_PDEV44_DEV + T23_DEV44_DRIVER_DATA, void *) = data;
	}
}

static void *priv_platform_get_drvdata(const struct platform_device *pdev)
{
	struct t23_twin_dev *td = t23_dev_by_blob(pdev);

	return td ? platform_get_drvdata(td->twin) : NULL;
}

static void priv_dev_set_drvdata(struct device *dev, void *data)
{
	struct t23_twin_dev *td = t23_dev_by_blob_dev(dev);

	dev_set_drvdata(td ? &td->twin->dev : dev, data);
	if (td)
		BLOB(dev, T23_DEV44_DRIVER_DATA, void *) = data;
}

static void *priv_dev_get_drvdata(const struct device *dev)
{
	return t23_dev_get_drvdata(dev);
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

static void __iomem *priv_ioremap(phys_t offset, unsigned long size)
{
	return ioremap(offset, size);
}

static void priv_iounmap(const volatile void __iomem *addr)
{
	iounmap(addr);
}

/*
 * The firmware requests a line, enabled, and disables it right after
 * (tx_isp_request_irq), before the subdev's private data is set. A line
 * still asserted by the previous load of the module (the IVDC's status
 * stays raised and enabled when the ISP stops) runs the new handler in
 * that window, and it oopses on the NULL private data. The line is
 * requested disabled here, and the firmware's own disable that follows is
 * dropped, which leaves the depth where the firmware expects it.
 */
#define T23_IRQ_FRESH	8

static unsigned int t23_irq_fresh[T23_IRQ_FRESH];
static DEFINE_SPINLOCK(t23_irq_fresh_lock);

static int priv_request_threaded_irq(unsigned int irq, irq_handler_t handler,
				     irq_handler_t thread_fn, unsigned long flags,
				     const char *name, void *dev)
{
	unsigned long lf;
	int i, ret;

	ret = request_threaded_irq(irq, handler, thread_fn, flags | IRQF_NO_AUTOEN, name, dev);
	if (ret)
		return ret;
	spin_lock_irqsave(&t23_irq_fresh_lock, lf);
	for (i = 0; i < T23_IRQ_FRESH; i++) {
		if (!t23_irq_fresh[i]) {
			t23_irq_fresh[i] = irq;
			break;
		}
	}
	spin_unlock_irqrestore(&t23_irq_fresh_lock, lf);
	/* no slot: let it run, as the firmware asked */
	if (i == T23_IRQ_FRESH)
		enable_irq(irq);
	return 0;
}

static bool t23_irq_fresh_take(unsigned int irq)
{
	unsigned long lf;
	bool fresh = false;
	int i;

	spin_lock_irqsave(&t23_irq_fresh_lock, lf);
	for (i = 0; i < T23_IRQ_FRESH; i++) {
		if (t23_irq_fresh[i] == irq) {
			t23_irq_fresh[i] = 0;
			fresh = true;
			break;
		}
	}
	spin_unlock_irqrestore(&t23_irq_fresh_lock, lf);
	return fresh;
}

static void priv_disable_irq(unsigned int irq)
{
	if (!t23_irq_fresh_take(irq))
		disable_irq(irq);
}

static void priv_enable_irq(unsigned int irq)
{
	t23_irq_fresh_take(irq);
	enable_irq(irq);
}

static void priv_free_irq(unsigned int irq, void *dev_id)
{
	t23_irq_fresh_take(irq);
	free_irq(irq, dev_id);
}

static void priv_kfree(void *p)
{
	kfree(p);
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
 * the T23 CGU clocks named by the DT, for the module's lifetime. 3.10 had
 * separate "cgu_isp" (divider) and "isp" (gate) clocks; the 7.x CGU has one
 * clock with all three, so both names reach it.
 */
static const struct {
	const char *alias;
	const char *node_compat;	/* NULL: the CGU provider itself */
	const char *clock_name;
	u32 cgu_index;
} t23_clk_aliases[] = {
	/* 4.4's per-stage clocks: the 7.x CGU has one clock with mux, divider and gate */
	{ "div_isp", "ingenic,t23-isp", "isp", 0 },
	{ "gate_isp", "ingenic,t23-isp", "isp", 0 },
	{ "mux_isp", "ingenic,t23-isp", "isp", 0 },
	{ "cgu_isp", "ingenic,t23-isp", "isp", 0 },
	{ "isp", "ingenic,t23-isp", "isp", 0 },
	{ "cgu_cim", "ingenic,t23-isp", "cim", 0 },
	{ "gate_csi", "ingenic,t23-csi", "csi", 0 },
	{ "csi", "ingenic,t23-csi", "csi", 0 },
	{ "gate_ivdc", NULL, NULL, T23_CLK_IVDC },
	{ "div_ahb0", NULL, NULL, T23_CLK_AHB0 },
	{ "div_vpu", NULL, NULL, T23_CLK_VPU },
	{ "mux_vpu", NULL, NULL, T23_CLK_VPU },
};

static struct clk_lookup *t23_clk_lookups[ARRAY_SIZE(t23_clk_aliases)];

static struct clk *t23_dt_clk(int i)
{
	struct device_node *np;
	struct clk *clk;

	if (t23_clk_aliases[i].node_compat) {
		np = of_find_compatible_node(NULL, NULL, t23_clk_aliases[i].node_compat);
		if (!np)
			return ERR_PTR(-ENODEV);
		clk = of_clk_get_by_name(np, t23_clk_aliases[i].clock_name);
	} else {
		struct of_phandle_args args = { .args_count = 1 };

		np = of_find_compatible_node(NULL, NULL, "ingenic,t23-cgu");
		if (!np)
			return ERR_PTR(-ENODEV);
		args.np = np;
		args.args[0] = t23_clk_aliases[i].cgu_index;
		clk = of_clk_get_from_provider(&args);
	}
	of_node_put(np);
	return clk;
}

static struct clk *priv_clk_get(struct device *dev, const char *id)
{
	/* a firmware-owned dev would be read past the end of its object */
	return clk_get(NULL, id);
}

static int priv_clk_enable(struct clk *clk)
{
	return clk_prepare_enable(clk);
}

static void priv_clk_disable(struct clk *clk)
{
	clk_disable_unprepare(clk);
}

static unsigned long priv_clk_get_rate(struct clk *clk)
{
	return clk_get_rate(clk);
}

static void priv_clk_put(struct clk *clk)
{
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

	pr_debug("t23-isp: clk %s set_rate %lu -> %ld: %d (now %lu)\n",
		__clk_get_name(clk), rate, r, ret, clk_get_rate(clk));
	return ret;
}

/*
 * ---- i2c -------------------------------------------------------------------
 *
 * isp_i2c_new_subdev_board() creates the sensor's i2c client through the
 * table and then reads client->dev.driver (4.4 offset 100; 88 on 7.x) and
 * driver->owner (8, the same on both) to pin the sensor module. It hands the
 * pointer back only to table entries, so it gets a 4.4-shaped twin carrying
 * the few fields it reads, with the kernel's client behind it. The
 * board_info it passes is the 44-byte 4.4 one (type[20] flags addr
 * platform_data archdata of_node acpi_node): rebuilt natively.
 */
#define T23_CLIENT44_SIZE	256
#define T23_CLIENT44_FLAGS	0	/* u16 */
#define T23_CLIENT44_ADDR	2	/* u16 */
#define T23_CLIENT44_NAME	4	/* char[20] */
#define T23_CLIENT44_ADAPTER	24	/* struct i2c_adapter * */
#define T23_CLIENT44_DEV_DRIVER	100	/* struct device_driver * */

/*
 * stands in for the driver of a client no sensor module bound to: the
 * firmware reads client->dev.driver->owner before it looks at the client
 * (the second sensor a sample registers without its module loaded)
 */
static struct device_driver t23_no_driver;

struct t23_i2c_twin {
	u8 client44[T23_CLIENT44_SIZE] __aligned(4);
	struct i2c_client *real;
};

static struct t23_i2c_twin *t23_i2c_twins[T23_MAX_TWINS];

static struct t23_i2c_twin *t23_i2c_twin_of(const void *p)
{
	int i;

	for (i = 0; i < T23_MAX_TWINS; i++) {
		struct t23_i2c_twin *tw = t23_i2c_twins[i];

		if (tw && (p == tw->client44 || p == tw->real))
			return tw;
	}
	return NULL;
}

/* the firmware's copy, or a real client the sensor driver stored itself */
static struct i2c_client *t23_i2c_real(const struct i2c_client *p)
{
	struct t23_i2c_twin *tw = t23_i2c_twin_of(p);

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
	struct t23_i2c_twin *tw;
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
	BLOB(tw->client44, T23_CLIENT44_FLAGS, unsigned short) = real->flags;
	BLOB(tw->client44, T23_CLIENT44_ADDR, unsigned short) = real->addr;
	memcpy(tw->client44 + T23_CLIENT44_NAME, real->name, I2C_NAME_SIZE);
	BLOB(tw->client44, T23_CLIENT44_ADAPTER, struct i2c_adapter *) = real->adapter;
	/* i2c_new_client_device() probes synchronously, so the driver is bound by now */
	BLOB(tw->client44, T23_CLIENT44_DEV_DRIVER, void *) =
		real->dev.driver ? (void *)real->dev.driver : (void *)&t23_no_driver;

	mutex_lock(&t23_twin_lock);
	for (i = 0; i < T23_MAX_TWINS; i++) {
		if (!t23_i2c_twins[i]) {
			t23_i2c_twins[i] = tw;
			break;
		}
	}
	mutex_unlock(&t23_twin_lock);
	if (i == T23_MAX_TWINS) {
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
	return i2c_get_clientdata(t23_i2c_real(client));
}

static void priv_i2c_set_clientdata(struct i2c_client *client, void *data)
{
	i2c_set_clientdata(t23_i2c_real(client), data);
}

static void priv_i2c_unregister_device(struct i2c_client *client)
{
	struct t23_i2c_twin *tw = t23_i2c_twin_of(client);
	struct i2c_client *real = tw ? tw->real : client;
	int i;

	if (tw) {
		mutex_lock(&t23_twin_lock);
		for (i = 0; i < T23_MAX_TWINS; i++)
			if (t23_i2c_twins[i] == tw)
				t23_i2c_twins[i] = NULL;
		mutex_unlock(&t23_twin_lock);
		kfree(tw);
	}
	i2c_unregister_device(real);
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

static int t23_pins_select(const char *name);

/*
 * Pin function and pull belong to pinctrl on 7.x; the DT carries them. The
 * one the firmware sets at run time is the MIPI switch pad it hands to the
 * VIC when it starts more than one sensor: that is the ISP node's
 * "mipi-switch" pin state.
 */
static int priv_jzgpio_set_func(int port, enum gpio_function func,
				unsigned long pins)
{
#ifdef CONFIG_MULTI_SENSOR
	if (pins && port * 32 + __ffs(pins) == get_mipi_switch_gpio())
		t23_pins_select("mipi-switch");
#endif
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
	return wait_for_completion_timeout(x, t23_jiffies_from_310(timeout));
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
#define T23_MISC44_MINOR	0	/* int */
#define T23_MISC44_NAME	4	/* const char * */
#define T23_MISC44_FOPS	8	/* const struct file_operations * */
#define T23_MISC44_PARENT	20	/* struct device * */
#define T23_MISC44_THIS_DEVICE	24	/* struct device * */
#define T23_MISC44_NODENAME	32	/* const char * */
#define T23_MISC44_MODE	36	/* umode_t */

struct t23_misc_twin {
	void *blob;				/* firmware's miscdevice */
	struct miscdevice twin;
	struct fw_fops fw;
};

static struct t23_misc_twin *t23_misc_twins[T23_MAX_TWINS];

static struct t23_misc_twin *t23_misc_twin_of(const void *blob)
{
	int i;

	for (i = 0; i < T23_MAX_TWINS; i++)
		if (t23_misc_twins[i] && t23_misc_twins[i]->blob == blob)
			return t23_misc_twins[i];
	return NULL;
}

static int priv_misc_register(struct miscdevice *misc44)
{
	struct t23_misc_twin *m;
	const void *blob_fops = BLOB(misc44, T23_MISC44_FOPS, const void *);
	struct device *parent = BLOB(misc44, T23_MISC44_PARENT, struct device *);
	struct t23_twin_dev *td = t23_dev_by_blob_dev(parent);
	int i, ret;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->blob = misc44;
	if (blob_fops)
		fw_fops_init(&m->fw, blob_fops, misc44);
	m->twin.minor = BLOB(misc44, T23_MISC44_MINOR, int);
	m->twin.name = BLOB(misc44, T23_MISC44_NAME, const char *);
	m->twin.fops = blob_fops ? &m->fw.fops : NULL;
	m->twin.parent = td ? &td->twin->dev : NULL;
	m->twin.nodename = BLOB(misc44, T23_MISC44_NODENAME, const char *);
	m->twin.mode = BLOB(misc44, T23_MISC44_MODE, umode_t);

	mutex_lock(&t23_twin_lock);
	for (i = 0; i < T23_MAX_TWINS; i++) {
		if (!t23_misc_twins[i]) {
			t23_misc_twins[i] = m;
			break;
		}
	}
	mutex_unlock(&t23_twin_lock);
	if (i == T23_MAX_TWINS) {
		kfree(m);
		return -ENOSPC;
	}

	ret = misc_register(&m->twin);
	if (ret) {
		mutex_lock(&t23_twin_lock);
		t23_misc_twins[i] = NULL;
		mutex_unlock(&t23_twin_lock);
		kfree(m);
		return ret;
	}
	BLOB(misc44, T23_MISC44_MINOR, int) = m->twin.minor;
	BLOB(misc44, T23_MISC44_THIS_DEVICE, struct device *) = m->twin.this_device;
	return 0;
}

static void priv_misc_deregister(struct miscdevice *misc44)
{
	struct t23_misc_twin *m = t23_misc_twin_of(misc44);
	int i;

	if (!m)
		return;
	misc_deregister(&m->twin);
	mutex_lock(&t23_twin_lock);
	for (i = 0; i < T23_MAX_TWINS; i++)
		if (t23_misc_twins[i] == m)
			t23_misc_twins[i] = NULL;
	mutex_unlock(&t23_twin_lock);
	kfree(m);
}

/*
 * ---- cdev: the tuning node -------------------------------------------------
 *
 * The firmware calls cdev_init() directly with its 108-byte fops; the objcopy
 * redirects cdev_init/cdev_del here.
 */
struct t23_cdev_twin {
	struct cdev *cdev;
	struct fw_fops fw;
};

static struct t23_cdev_twin *t23_cdev_twins[T23_MAX_TWINS];

void t23_cdev_init_310(struct cdev *cdev, const void *fops44)
{
	struct t23_cdev_twin *tw = kzalloc(sizeof(*tw), GFP_KERNEL);
	int i;

	if (!tw) {
		cdev_init(cdev, NULL);
		return;
	}
	tw->cdev = cdev;
	fw_fops_init(&tw->fw, fops44, NULL);
	mutex_lock(&t23_twin_lock);
	for (i = 0; i < T23_MAX_TWINS; i++) {
		if (!t23_cdev_twins[i]) {
			t23_cdev_twins[i] = tw;
			break;
		}
	}
	mutex_unlock(&t23_twin_lock);
	if (i == T23_MAX_TWINS) {
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
void t23_cdev_del_310(struct cdev *cdev)
{
	int i;
	bool live = false;

	mutex_lock(&t23_twin_lock);
	for (i = 0; i < T23_MAX_TWINS; i++) {
		if (t23_cdev_twins[i] && t23_cdev_twins[i]->cdev == cdev) {
			kfree(t23_cdev_twins[i]);
			t23_cdev_twins[i] = NULL;
			live = true;
		}
	}
	mutex_unlock(&t23_twin_lock);
	if (live)
		cdev_del(cdev);
}

/*
 * ---- /proc -----------------------------------------------------------------
 *
 * proc_create_data() takes a proc_ops since 5.6; tx-isp-twin.c builds one
 * from the firmware's fops. Its open handlers read their data back with
 * PDE_DATA(), redirected to t23_pde_data_310().
 */
static struct proc_dir_entry *t23_jz_root;

struct proc_dir_entry *jz_proc_mkdir(char *s)
{
	return t23_jz_root ? proc_mkdir(s, t23_jz_root) : NULL;
}

static struct proc_dir_entry *priv_proc_create_data(const char *name, umode_t mode,
						    struct proc_dir_entry *parent,
						    const struct file_operations *fops44,
						    void *data)
{
	return t23_proc_create_data(name, mode, parent, fops44, data);
}

static void priv_proc_remove(struct proc_dir_entry *de)
{
	t23_proc_remove(de);
}

static struct proc_dir_entry *priv_jz_proc_mkdir(char *s)
{
	return jz_proc_mkdir(s);
}

static int priv_single_open_size(struct file *file, int (*show)(struct seq_file *, void *),
				 void *data, size_t size)
{
	return t23_single_open_size(file, show, data, size);
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
 * The interface table, in the 95-member layout the T23 firmware was built
 * against (include/txx-funcs.h). The firmware checks flags_0/flags_1 against
 * &printk before using it (private_get_driver_interface in tx-isp-debug.c).
 * The netlink, vfs, kmalloc, nlmsg and dma members are in txx-funcs.c.
 */
static struct jz_driver_common_interfaces private_funcs = {
	.flags_0 = (unsigned int)&_printk,
	/* platform interface */
	.priv_platform_driver_unregister = priv_platform_driver_unregister,
	.priv_platform_set_drvdata = priv_platform_set_drvdata,
	.priv_platform_get_drvdata = priv_platform_get_drvdata,
	.priv_platform_device_register = priv_platform_device_register,
	.priv_platform_device_unregister = priv_platform_device_unregister,
	.priv_platform_get_resource = priv_platform_get_resource,
	.priv_dev_set_drvdata = priv_dev_set_drvdata,
	.priv_dev_get_drvdata = priv_dev_get_drvdata,
	.priv_platform_get_irq = priv_platform_get_irq,
	.priv_request_mem_region = priv_request_mem_region,
	.priv_release_mem_region = priv_release_mem_region,
	.priv_ioremap = priv_ioremap,
	.priv_iounmap = priv_iounmap,

	/* interrupt interface */
	.priv_request_threaded_irq = priv_request_threaded_irq,
	.priv_enable_irq = priv_enable_irq,
	.priv_disable_irq = priv_disable_irq,
	.priv_free_irq = priv_free_irq,

	/* lock and mutex interface */
	.priv_spin_lock_irqsave = priv_spin_lock_irqsave,
	.priv_spin_unlock_irqrestore = priv_spin_unlock_irqrestore,
	.priv_spin_lock_init = priv_spin_lock_init,
	.priv_mutex_lock = mutex_lock,
	.priv_mutex_unlock = mutex_unlock,
	.priv_raw_mutex_init = priv_raw_mutex_init,

	/* clock interfaces */
	.priv_clk_get = priv_clk_get,
	/* 3.10's clk API had no prepare step; the common clock framework needs it */
	.priv_clk_enable = priv_clk_enable,
	.priv_clk_disable = priv_clk_disable,
	.priv_clk_get_rate = priv_clk_get_rate,
	.priv_clk_put = priv_clk_put,
	.priv_clk_set_rate = priv_clk_set_rate,

	/* i2c interfaces */
	.priv_i2c_get_adapter = i2c_get_adapter,
	.priv_i2c_put_adapter = i2c_put_adapter,
	.priv_i2c_transfer = i2c_transfer,
	.priv_i2c_register_driver = priv_i2c_register_driver,
	.priv_i2c_del_driver = i2c_del_driver,
	.priv_i2c_new_device = priv_i2c_new_device,
	.priv_i2c_get_clientdata = priv_i2c_get_clientdata,
	.priv_i2c_set_clientdata = priv_i2c_set_clientdata,
	.priv_i2c_unregister_device = priv_i2c_unregister_device,

	/* gpio interfaces */
	.priv_gpio_request = priv_gpio_request,
	.priv_gpio_free = priv_gpio_free,
	.priv_gpio_direction_output = priv_gpio_direction_output,
	.priv_gpio_direction_input = priv_gpio_direction_input,
	.priv_gpio_set_debounce = priv_gpio_set_debounce,
	.priv_jzgpio_set_func = priv_jzgpio_set_func,

	/* system interface */
	.priv_msleep = priv_msleep,
	.priv_capable = priv_capable,
	.priv_sched_clock = priv_sched_clock,
	.priv_try_module_get = priv_try_module_get,
	.priv_request_module = priv_request_module,
	.priv_module_put = priv_module_put,

	/* wait */
	.priv_init_completion = priv_init_completion,
	.priv_complete = priv_complete,
	.priv_wait_for_completion_interruptible = priv_wait_for_completion_interruptible,
	.priv_wait_event_interruptible = priv_wait_event_interruptible,
	.priv_wake_up_all = priv_wake_up_all,
	.priv_wake_up = priv_wake_up,
	.priv_init_waitqueue_head = priv_init_waitqueue_head,
	.priv_wait_for_completion_timeout = priv_wait_for_completion_timeout,

	/* misc */
	.priv_misc_register = priv_misc_register,
	.priv_misc_deregister = priv_misc_deregister,
	/* proc */
	.priv_proc_create_data = priv_proc_create_data,
	.priv_seq_read = t23_seq_read,
	.priv_seq_lseek = t23_seq_lseek,
	.priv_single_release = t23_single_release,
	.priv_single_open_size = priv_single_open_size,
	.priv_jz_proc_mkdir = priv_jz_proc_mkdir,
	.priv_proc_remove = priv_proc_remove,
	.priv_seq_printf = t23_seq_printf_310,
	.priv_simple_strtoull = priv_simple_strtoull,

	/* kthread */
	.priv_kthread_should_stop = priv_kthread_should_stop,
	.priv_kthread_run = priv_kthread_run,
	.priv_kthread_stop = priv_kthread_stop,

	.priv_kmalloc = t23_priv_kmalloc,
	.priv_kfree = priv_kfree,
	.priv_copy_from_user = priv_copy_from_user,
	.priv_copy_to_user = priv_copy_to_user,

	/* netlink */
	.priv_nlmsg_new = t23_priv_nlmsg_new,
	.priv_nlmsg_put = t23_priv_nlmsg_put,
	.priv_netlink_unicast = netlink_unicast,
	.priv_netlink_kernel_create = t23_priv_netlink_kernel_create,
	.priv_sock_release = t23_priv_sock_release,

	.priv_filp_open = t23_filp_open,
	.priv_filp_close = t23_filp_close,
	.priv_vfs_read = t23_vfs_read,
	.priv_vfs_write = t23_vfs_write,
	.priv_vfs_llseek = t23_vfs_llseek,
	.priv_get_fs = t23_priv_get_fs,
	.priv_set_fs = t23_priv_set_fs,
	.priv_dma_cache_sync = t23_priv_dma_cache_sync,

	.priv_getrawmonotonic = t23_priv_getrawmonotonic,
	.priv_get_init_net = t23_priv_get_init_net,

	/* isp driver interface */
	.get_isp_priv_mem = priv_get_isp_priv_mem,
	.flags_1 = (unsigned int)&_printk,
};

/*
 * The firmware's private_get_driver_interface (weak, overridden in
 * tx-isp-debug.c) asks for the table here.
 */
void *get_driver_common_interfaces(void)
{
	return (void *)&private_funcs;
}

/* ---- init ------------------------------------------------------------------- */

/*
 * The ISP node is never bound by a driver, so nothing applies its pin state
 * (the CIM MCLK pad). A placeholder device on that node holds it while the
 * module is loaded; the twins stay off the node so their IRQs keep coming
 * from their own resources.
 */
static struct platform_device *t23_pins_dev;
static struct pinctrl *t23_pins;

static int t23_pins_get(void)
{
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "ingenic,t23-isp");
	if (!np)
		return -ENODEV;
	t23_pins_dev = platform_device_alloc("t23-isp-pins", PLATFORM_DEVID_NONE);
	if (!t23_pins_dev) {
		of_node_put(np);
		return -ENOMEM;
	}
	t23_pins_dev->dev.of_node = np;		/* reference passes to the device */
	ret = platform_device_add(t23_pins_dev);
	if (ret)
		goto err_put;
	t23_pins = pinctrl_get_select_default(&t23_pins_dev->dev);
	if (IS_ERR(t23_pins)) {
		ret = PTR_ERR(t23_pins);
		t23_pins = NULL;
		platform_device_unregister(t23_pins_dev);
		t23_pins_dev = NULL;
		return ret;
	}
	return 0;

err_put:
	platform_device_put(t23_pins_dev);
	t23_pins_dev = NULL;
	return ret;
}

static int t23_pins_select(const char *name)
{
	struct pinctrl_state *st;
	int ret;

	if (!t23_pins)
		return -ENODEV;
	st = pinctrl_lookup_state(t23_pins, name);
	if (IS_ERR(st)) {
		pr_warn_once("t23-isp: no \"%s\" pin state on the ISP node\n", name);
		return PTR_ERR(st);
	}
	ret = pinctrl_select_state(t23_pins, st);
	if (ret)
		pr_err("t23-isp: pin state %s: %d\n", name, ret);
	return ret;
}

static void t23_pins_put(void)
{
	if (t23_pins) {
		pinctrl_put(t23_pins);
		t23_pins = NULL;
	}
	if (t23_pins_dev) {
		platform_device_unregister(t23_pins_dev);
		t23_pins_dev = NULL;
	}
}

void tx_isp_abi_exit(void)
{
	int i;

	t23_pins_put();

	for (i = 0; i < ARRAY_SIZE(t23_clk_lookups); i++) {
		struct clk_lookup *cl = t23_clk_lookups[i];

		if (!cl)
			continue;
		t23_clk_lookups[i] = NULL;
		clkdev_drop(cl);
	}
	if (t23_jz_root) {
		proc_remove(t23_jz_root);
		t23_jz_root = NULL;
	}
}

int tx_isp_abi_init(void)
{
	int i, ret;

	/* firmware-embedded kernel objects must not outgrow their 4.4 size */
	BUILD_BUG_ON(sizeof(spinlock_t) != 0);
	BUILD_BUG_ON(sizeof(struct mutex) > 12);
	BUILD_BUG_ON(sizeof(struct completion) > 12);
	BUILD_BUG_ON(sizeof(wait_queue_head_t) > 8);
	BUILD_BUG_ON(sizeof(struct work_struct) > 16);
	BUILD_BUG_ON(sizeof(struct cdev) > 60);
	/* the sensor drivers' platform_device twin */
	BUILD_BUG_ON(sizeof(struct tx_isp_pdev44) != 248);
	BUILD_BUG_ON(offsetof(struct tx_isp_pdev44, platform_data) != T23_PDEV44_DEV + 72);
	BUILD_BUG_ON(offsetof(struct tx_isp_pdev44, dma_mask) != T23_PDEV44_DEV + T23_DEV44_DMA_MASK);
	BUILD_BUG_ON(offsetof(struct tx_isp_pdev44, coherent_dma_mask) != T23_PDEV44_DEV + T23_DEV44_COHERENT);
	BUILD_BUG_ON(offsetof(struct tx_isp_pdev44, num_resources) != T23_PDEV44_NUM_RES);
	BUILD_BUG_ON(offsetof(struct tx_isp_pdev44, resource) != T23_PDEV44_RES);
	/* the layouts shared with the firmware unchanged */
	BUILD_BUG_ON(sizeof(struct miscdevice) != 40);
	BUILD_BUG_ON(offsetof(struct miscdevice, mode) != T23_MISC44_MODE);
	BUILD_BUG_ON(offsetof(struct miscdevice, nodename) != T23_MISC44_NODENAME);
	BUILD_BUG_ON(offsetof(struct miscdevice, this_device) != T23_MISC44_THIS_DEVICE);
	BUILD_BUG_ON(offsetof(struct miscdevice, parent) != T23_MISC44_PARENT);
	BUILD_BUG_ON(offsetof(struct platform_driver, probe) != T23_PDRV44_PROBE);
	BUILD_BUG_ON(offsetof(struct platform_driver, remove) != T23_PDRV44_REMOVE);
	BUILD_BUG_ON(offsetof(struct platform_driver, driver) != T23_PDRV44_DRIVER);
	BUILD_BUG_ON(offsetof(struct device_driver, owner) != 8);
	BUILD_BUG_ON(offsetof(struct thread_info, flags) != 4);
	BUILD_BUG_ON(offsetof(struct thread_info, preempt_count) != 16);
	BUILD_BUG_ON(offsetof(struct thread_info, syscall) != 24);

	ret = t23_pins_get();
	if (ret)
		pr_warn("t23-isp: no MCLK pin state from the ISP node (%d)\n", ret);

	t23_jz_root = proc_mkdir("jz", NULL);
	if (!t23_jz_root)
		pr_warn("t23-isp: /proc/jz exists or cannot be created\n");

	for (i = 0; i < ARRAY_SIZE(t23_clk_aliases); i++) {
		struct clk *clk = t23_dt_clk(i);
		struct clk_lookup *cl;

		if (IS_ERR(clk)) {
			pr_err("t23-isp: no clock for alias %s (%ld)\n",
			       t23_clk_aliases[i].alias, PTR_ERR(clk));
			ret = PTR_ERR(clk);
			goto err;
		}
		cl = clkdev_create(clk, t23_clk_aliases[i].alias, NULL);
		clk_put(clk);
		if (!cl) {
			ret = -ENOMEM;
			goto err;
		}
		t23_clk_lookups[i] = cl;
	}
	return 0;

err:
	tx_isp_abi_exit();
	return ret;
}
