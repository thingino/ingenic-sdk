// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-abi.c - Linux 4.4.94 ABI adapter for the T33 ISP firmware
 *
 * libt33-firmware-4-4-94.a (2.2.0) was built against Linux 4.4.94 and has
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
 *     them with objcopy to the t33_*_44 translators.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
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
#include <dt-bindings/clock/ingenic,t32-cgu.h>

#include <txx-funcs.h>
#include <tx-isp-device.h>
#include <soc/gpio.h>
#include "tx-isp-abi.h"

#define BLOB(base, off, type)	(*(type *)((u8 *)(base) + (off)))

#define T33_MAX_TWINS		16

/* writers hold t33_twin_lock; readers, some in IRQ context, are lock-free */
static DEFINE_MUTEX(t33_twin_lock);

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
#define T33_PDEV44_NAME		0	/* const char * */
#define T33_PDEV44_ID		4	/* int */
#define T33_PDEV44_DEV		16	/* struct device */
#define T33_PDEV44_NUM_RES	284	/* u32 */
#define T33_PDEV44_RES		288	/* struct resource *, 32-byte stride */
#define T33_DEV44_INIT_NAME	offsetof(struct device, init_name)
#define T33_DEV44_DRIVER	offsetof(struct device, driver)
#define T33_DEV44_DMA_MASK	144	/* u64 * */
#define T33_DEV44_COHERENT	152	/* u64 */
#define T33_DEV44_DRIVER_DATA	offsetof(struct device, driver_data)
#define T33_RES44_SIZE		sizeof(struct resource)
#define T33_RES44_START		offsetof(struct resource, start)
#define T33_RES44_END		offsetof(struct resource, end)
#define T33_RES44_NAME		offsetof(struct resource, name)
#define T33_RES44_FLAGS		offsetof(struct resource, flags)
#define T33_PDRV44_PROBE	0	/* int (*)(struct platform_device *) */
#define T33_PDRV44_REMOVE	4
#define T33_PDRV44_DRIVER	24	/* struct device_driver (5.15.170 has remove_new at 8); .name at +0, .owner at +8 */

typedef int (*t33_pdrv_fn)(void *);

struct t33_twin_dev {
	void *blob;			/* firmware's platform_device */
	struct platform_device *twin;
};

struct t33_twin_drv {
	void *blob;			/* firmware's platform_driver */
	struct platform_driver twin;
};

static struct t33_twin_dev t33_twin_devs[T33_MAX_TWINS];
static struct t33_twin_drv t33_twin_drvs[T33_MAX_TWINS];

static struct t33_twin_dev *t33_dev_by_blob(const void *blob)
{
	int i;

	if (!blob)
		return NULL;
	for (i = 0; i < T33_MAX_TWINS; i++)
		if (t33_twin_devs[i].blob == blob)
			return &t33_twin_devs[i];
	return NULL;
}

static struct t33_twin_dev *t33_dev_by_twin(const struct platform_device *pdev)
{
	int i;

	for (i = 0; i < T33_MAX_TWINS; i++)
		if (t33_twin_devs[i].blob && t33_twin_devs[i].twin == pdev)
			return &t33_twin_devs[i];
	return NULL;
}

/* the firmware passes &pdev->dev, which is its object + 16 */
static struct t33_twin_dev *t33_dev_by_blob_dev(const struct device *dev)
{
	return dev ? t33_dev_by_blob((const u8 *)dev - T33_PDEV44_DEV) : NULL;
}

static struct t33_twin_drv *t33_drv_by_blob(const void *blob)
{
	int i;

	if (!blob)
		return NULL;
	for (i = 0; i < T33_MAX_TWINS; i++)
		if (t33_twin_drvs[i].blob == blob)
			return &t33_twin_drvs[i];
	return NULL;
}

/* firmware device pointer for a native struct device, or NULL */
void *t33_blob_dev_of(const struct device *dev)
{
	int i;

	for (i = 0; i < T33_MAX_TWINS; i++)
		if (t33_twin_devs[i].blob && &t33_twin_devs[i].twin->dev == dev)
			return t33_twin_devs[i].blob;
	return NULL;
}

/* drvdata of a firmware-owned struct device, which the firmware reads back through dev_get_drvdata() */
void *t33_dev_get_drvdata(const struct device *dev)
{
	struct t33_twin_dev *td = t33_dev_by_blob_dev(dev);

	return dev_get_drvdata(td ? &td->twin->dev : dev);
}

/* any live twin, for the DMA API calls that need a real device */
struct device *t33_any_dev(void)
{
	int i;

	for (i = 0; i < T33_MAX_TWINS; i++)
		if (t33_twin_devs[i].blob)
			return &t33_twin_devs[i].twin->dev;
	return NULL;
}

static int t33_twin_probe(struct platform_device *pdev)
{
	struct t33_twin_dev *td = t33_dev_by_twin(pdev);
	struct t33_twin_drv *tr = container_of(to_platform_driver(pdev->dev.driver),
					       struct t33_twin_drv, twin);
	t33_pdrv_fn probe = BLOB(tr->blob, T33_PDRV44_PROBE, t33_pdrv_fn);

	if (!td || !probe)
		return -ENODEV;
	/* what 3.10 would have left in the firmware's own object */
	BLOB(td->blob, T33_PDEV44_DEV + T33_DEV44_DRIVER, void *) =
		(u8 *)tr->blob + T33_PDRV44_DRIVER;
	return probe(td->blob);
}

static void t33_twin_remove(struct platform_device *pdev)
{
	struct t33_twin_dev *td = t33_dev_by_twin(pdev);
	struct t33_twin_drv *tr = container_of(to_platform_driver(pdev->dev.driver),
					       struct t33_twin_drv, twin);
	t33_pdrv_fn remove = BLOB(tr->blob, T33_PDRV44_REMOVE, t33_pdrv_fn);

	if (!td)
		return;
	if (remove)
		remove(td->blob);
	BLOB(td->blob, T33_PDEV44_DEV + T33_DEV44_DRIVER, void *) = NULL;
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
} t33_irq_map[] = {
	{ "isp-m0", "ingenic,t33-isp", "isp" },
	{ "isp-w02", "ingenic,t33-isp", "vic" },
	{ "isp-ivdc", "ingenic,t33-ivdc", "ivdc" },
	{ "isp-m1", "ingenic,t33-ldc", "ldc" },
	{ "riscv", "ingenic,t33-riscv", "riscv" },
};

static int t33_dt_irq(const char *devname)
{
	int i, irq = -ENXIO;

	for (i = 0; i < ARRAY_SIZE(t33_irq_map); i++) {
		struct device_node *np;

		if (strcmp(devname, t33_irq_map[i].dev))
			continue;
		np = of_find_compatible_node(NULL, NULL, t33_irq_map[i].compat);
		if (!np)
			return -ENODEV;
		irq = of_irq_get_byname(np, t33_irq_map[i].irq_name);
		of_node_put(np);
		break;
	}
	return irq;
}

static int priv_platform_device_register(struct platform_device *pdev)
{
	void *blob = pdev;
	const char *name = BLOB(blob, T33_PDEV44_NAME, const char *);
	struct t33_twin_dev *td = NULL;
	struct platform_device *twin;
	struct resource *res = NULL;
	u32 nres = BLOB(blob, T33_PDEV44_NUM_RES, u32);
	void *res310 = BLOB(blob, T33_PDEV44_RES, void *);
	int i, ret;

	twin = platform_device_alloc(name, BLOB(blob, T33_PDEV44_ID, int));
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
			u8 *r = (u8 *)res310 + i * T33_RES44_SIZE;

			res[i].start = BLOB(r, T33_RES44_START, u32);
			res[i].end = BLOB(r, T33_RES44_END, u32);
			res[i].name = BLOB(r, T33_RES44_NAME, const char *);
			res[i].flags = BLOB(r, T33_RES44_FLAGS, u32);
			pr_info("t33-isp: %s res %d: %#llx..%#llx flags %#lx\n", name, i,
				 (u64)res[i].start, (u64)res[i].end, res[i].flags);
			if (res[i].flags & IORESOURCE_IRQ) {
				int irq = t33_dt_irq(name);

				if (irq <= 0) {
					pr_err("t33-isp: no DT irq for %s (%d)\n", name, irq);
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
	twin->dev.dma_mask = BLOB(blob, T33_PDEV44_DEV + T33_DEV44_DMA_MASK, u64 *);
	twin->dev.coherent_dma_mask = BLOB(blob, T33_PDEV44_DEV + T33_DEV44_COHERENT, u64);
	/* keep dev_name() meaningful on the firmware's own object */
	BLOB(blob, T33_PDEV44_DEV + T33_DEV44_INIT_NAME, const char *) = name;

	mutex_lock(&t33_twin_lock);
	for (i = 0; i < T33_MAX_TWINS; i++) {
		if (!t33_twin_devs[i].blob) {
			td = &t33_twin_devs[i];
			td->twin = twin;
			td->blob = blob;	/* last: publishes the entry */
			break;
		}
	}
	mutex_unlock(&t33_twin_lock);
	if (!td) {
		ret = -ENOSPC;
		goto err_put;
	}

	ret = platform_device_add(twin);
	if (ret) {
		mutex_lock(&t33_twin_lock);
		td->blob = NULL;
		td->twin = NULL;
		mutex_unlock(&t33_twin_lock);
		goto err_put;
	}
	kfree(res);
	pr_info("t33-isp: twin %s (%u resources)\n",
		dev_name(&twin->dev), nres);
	return 0;

err_put:
	kfree(res);
	platform_device_put(twin);
	return ret;
}

static void priv_platform_device_unregister(struct platform_device *pdev)
{
	struct t33_twin_dev *td = t33_dev_by_blob(pdev);
	struct platform_device *twin;

	if (!td)
		return;
	twin = td->twin;
	platform_device_unregister(twin);
	mutex_lock(&t33_twin_lock);
	td->blob = NULL;
	td->twin = NULL;
	mutex_unlock(&t33_twin_lock);
}

/* the firmware imports __platform_driver_register(); the Kbuild redirects it here */
int t33_platform_driver_register_44(struct platform_driver *drv, struct module *owner)
{
	void *blob = drv;
	struct t33_twin_drv *tr = NULL;
	int i, ret;

	mutex_lock(&t33_twin_lock);
	for (i = 0; i < T33_MAX_TWINS; i++) {
		if (!t33_twin_drvs[i].blob) {
			tr = &t33_twin_drvs[i];
			memset(&tr->twin, 0, sizeof(tr->twin));
			tr->twin.driver.name = BLOB(blob, T33_PDRV44_DRIVER, const char *);
			tr->twin.probe = t33_twin_probe;
			tr->twin.remove = t33_twin_remove;
			tr->blob = blob;
			break;
		}
	}
	mutex_unlock(&t33_twin_lock);
	if (!tr)
		return -ENOSPC;

	/* the owner is the firmware's __this_module, i.e. this module */
	ret = __platform_driver_register(&tr->twin, owner, KBUILD_MODNAME);
	if (ret) {
		mutex_lock(&t33_twin_lock);
		tr->blob = NULL;
		mutex_unlock(&t33_twin_lock);
	}
	return ret;
}

static void priv_platform_driver_unregister(struct platform_driver *drv)
{
	struct t33_twin_drv *tr = t33_drv_by_blob(drv);

	if (!tr)
		return;
	platform_driver_unregister(&tr->twin);
	mutex_lock(&t33_twin_lock);
	tr->blob = NULL;
	mutex_unlock(&t33_twin_lock);
}

static struct resource *priv_platform_get_resource(struct platform_device *pdev,
						   unsigned int type, unsigned int num)
{
	struct t33_twin_dev *td = t33_dev_by_blob(pdev);

	return td ? platform_get_resource(td->twin, type, num) : NULL;
}

static int priv_platform_get_irq(struct platform_device *pdev, unsigned int num)
{
	struct t33_twin_dev *td = t33_dev_by_blob(pdev);

	return td ? platform_get_irq_optional(td->twin, num) : -ENODEV;
}

static void priv_platform_set_drvdata(struct platform_device *pdev, void *data)
{
	struct t33_twin_dev *td = t33_dev_by_blob(pdev);

	if (td) {
		platform_set_drvdata(td->twin, data);
		BLOB(pdev, T33_PDEV44_DEV + T33_DEV44_DRIVER_DATA, void *) = data;
	}
}

static void *priv_platform_get_drvdata(const struct platform_device *pdev)
{
	struct t33_twin_dev *td = t33_dev_by_blob(pdev);

	return td ? platform_get_drvdata(td->twin) : NULL;
}

static void priv_dev_set_drvdata(struct device *dev, void *data)
{
	struct t33_twin_dev *td = t33_dev_by_blob_dev(dev);

	dev_set_drvdata(td ? &td->twin->dev : dev, data);
	if (td)
		BLOB(dev, T33_DEV44_DRIVER_DATA, void *) = data;
}

static void *priv_dev_get_drvdata(const struct device *dev)
{
	return t33_dev_get_drvdata(dev);
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
 * the T33 CGU clocks named by the DT, for the module's lifetime. 3.10 had
 * separate "cgu_isp" (divider) and "isp" (gate) clocks; the 7.x CGU has one
 * clock with all three, so both names reach it.
 */
static const struct {
	const char *alias;
	u32 cgu_index;
} t33_clk_aliases[] = {
	/* 4.4's per-stage clocks: the 7.x CGU has one clock with mux, divider and gate */
	{ "div_isp", T32_CLK_ISP }, { "mux_isp", T32_CLK_ISP }, { "gate_isp", T32_CLK_ISP },
	{ "div_isps", T32_CLK_ISPS }, { "mux_isps", T32_CLK_ISPS },
	{ "div_ispa", T32_CLK_ISPA }, { "mux_ispa", T32_CLK_ISPA },
	{ "gate_csi", T32_CLK_MIPI_CSI }, { "gate_ivdc", T32_CLK_IVDC },
	{ "div_ldc", T32_CLK_LDC }, { "mux_ldc", T32_CLK_LDC }, { "gate_ldc", T32_CLK_LDC },
	{ "div_vpu", T32_CLK_VPU }, { "mux_vpu", T32_CLK_VPU }, { "gate_vpu", T32_CLK_VPU },
	{ "div_cim", T32_CLK_CIM }, { "mux_cim", T32_CLK_CIM },
	{ "div_riscv", T33_CLK_RISCV }, { "mux_riscv", T33_CLK_RISCV }, { "gate_riscv", T33_CLK_RISCV },
	{ "ext", T32_CLK_EXCLK }, { "apll", T32_CLK_APLL }, { "mpll", T32_CLK_MPLL },
	{ "sclka", T32_CLK_SCLKA },
};

static struct clk_lookup *t33_clk_lookups[ARRAY_SIZE(t33_clk_aliases)];

static struct clk *t33_dt_clk(int i)
{
	struct of_phandle_args args = { .args_count = 1 };
	struct clk *clk;

	args.np = of_find_compatible_node(NULL, NULL, "ingenic,t33-cgu");
	if (!args.np)
		return ERR_PTR(-ENODEV);
	args.args[0] = t33_clk_aliases[i].cgu_index;
	clk = of_clk_get_from_provider(&args);
	of_node_put(args.np);
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

	pr_debug("t33-isp: clk %s set_rate %lu -> %ld: %d (now %lu)\n",
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
#define T33_CLIENT44_SIZE	256
#define T33_CLIENT44_FLAGS	0	/* u16 */
#define T33_CLIENT44_ADDR	2	/* u16 */
#define T33_CLIENT44_NAME	4	/* char[20] */
#define T33_CLIENT44_ADAPTER	24	/* struct i2c_adapter * */
#define T33_CLIENT44_DEV_DRIVER	88	/* struct device_driver * */

/* stands in for the driver of a client no sensor module bound to */
static struct device_driver t33_no_driver;

struct t33_i2c_twin {
	u8 client44[T33_CLIENT44_SIZE] __aligned(4);
	struct i2c_client *real;
};

static struct t33_i2c_twin *t33_i2c_twins[T33_MAX_TWINS];

static struct t33_i2c_twin *t33_i2c_twin_of(const void *p)
{
	int i;

	for (i = 0; i < T33_MAX_TWINS; i++) {
		struct t33_i2c_twin *tw = t33_i2c_twins[i];

		if (tw && (p == tw->client44 || p == tw->real))
			return tw;
	}
	return NULL;
}

/* the firmware's copy, or a real client the sensor driver stored itself */
static struct i2c_client *t33_i2c_real(const struct i2c_client *p)
{
	struct t33_i2c_twin *tw = t33_i2c_twin_of(p);

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
	struct t33_i2c_twin *tw;
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
	BLOB(tw->client44, T33_CLIENT44_FLAGS, unsigned short) = real->flags;
	BLOB(tw->client44, T33_CLIENT44_ADDR, unsigned short) = real->addr;
	memcpy(tw->client44 + T33_CLIENT44_NAME, real->name, I2C_NAME_SIZE);
	BLOB(tw->client44, T33_CLIENT44_ADAPTER, struct i2c_adapter *) = real->adapter;
	/* i2c_new_client_device() probes synchronously, so the driver is bound by now */
	/*
	 * The firmware reads client->dev.driver->owner without a NULL check, so
	 * a sensor nobody drives (no module for that name) gets a driver stub:
	 * it then finds no client data, unregisters the client and fails.
	 */
	if (!real->dev.driver)
		pr_err("t33-isp: no sensor driver for %s at 0x%02x\n", real->name, real->addr);
	BLOB(tw->client44, T33_CLIENT44_DEV_DRIVER, void *) =
		real->dev.driver ? (void *)real->dev.driver : (void *)&t33_no_driver;

	mutex_lock(&t33_twin_lock);
	for (i = 0; i < T33_MAX_TWINS; i++) {
		if (!t33_i2c_twins[i]) {
			t33_i2c_twins[i] = tw;
			break;
		}
	}
	mutex_unlock(&t33_twin_lock);
	if (i == T33_MAX_TWINS) {
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
	return i2c_get_clientdata(t33_i2c_real(client));
}

static void priv_i2c_set_clientdata(struct i2c_client *client, void *data)
{
	i2c_set_clientdata(t33_i2c_real(client), data);
}

static void priv_i2c_unregister_device(struct i2c_client *client)
{
	struct t33_i2c_twin *tw = t33_i2c_twin_of(client);
	struct i2c_client *real = tw ? tw->real : client;
	int i;

	if (tw) {
		mutex_lock(&t33_twin_lock);
		for (i = 0; i < T33_MAX_TWINS; i++)
			if (t33_i2c_twins[i] == tw)
				t33_i2c_twins[i] = NULL;
		mutex_unlock(&t33_twin_lock);
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
	return wait_for_completion_timeout(x, t33_jiffies_from_44(timeout));
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
#define T33_MISC44_MINOR	0	/* int */
#define T33_MISC44_NAME	4	/* const char * */
#define T33_MISC44_FOPS	8	/* const struct file_operations * */
#define T33_MISC44_PARENT	20	/* struct device * */
#define T33_MISC44_THIS_DEVICE	24	/* struct device * */
#define T33_MISC44_NODENAME	32	/* const char * */
#define T33_MISC44_MODE	36	/* umode_t */

struct t33_misc_twin {
	void *blob;				/* firmware's miscdevice */
	struct miscdevice twin;
	struct fw_fops fw;
};

static struct t33_misc_twin *t33_misc_twins[T33_MAX_TWINS];

static struct t33_misc_twin *t33_misc_twin_of(const void *blob)
{
	int i;

	for (i = 0; i < T33_MAX_TWINS; i++)
		if (t33_misc_twins[i] && t33_misc_twins[i]->blob == blob)
			return t33_misc_twins[i];
	return NULL;
}

static int priv_misc_register(struct miscdevice *misc44)
{
	struct t33_misc_twin *m;
	const void *blob_fops = BLOB(misc44, T33_MISC44_FOPS, const void *);
	struct device *parent = BLOB(misc44, T33_MISC44_PARENT, struct device *);
	struct t33_twin_dev *td = t33_dev_by_blob_dev(parent);
	int i, ret;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->blob = misc44;
	if (blob_fops)
		fw_fops_init(&m->fw, blob_fops, misc44);
	m->twin.minor = BLOB(misc44, T33_MISC44_MINOR, int);
	m->twin.name = BLOB(misc44, T33_MISC44_NAME, const char *);
	m->twin.fops = blob_fops ? &m->fw.fops : NULL;
	m->twin.parent = td ? &td->twin->dev : NULL;
	m->twin.nodename = BLOB(misc44, T33_MISC44_NODENAME, const char *);
	m->twin.mode = BLOB(misc44, T33_MISC44_MODE, umode_t);

	mutex_lock(&t33_twin_lock);
	for (i = 0; i < T33_MAX_TWINS; i++) {
		if (!t33_misc_twins[i]) {
			t33_misc_twins[i] = m;
			break;
		}
	}
	mutex_unlock(&t33_twin_lock);
	if (i == T33_MAX_TWINS) {
		kfree(m);
		return -ENOSPC;
	}

	ret = misc_register(&m->twin);
	if (ret) {
		mutex_lock(&t33_twin_lock);
		t33_misc_twins[i] = NULL;
		mutex_unlock(&t33_twin_lock);
		kfree(m);
		return ret;
	}
	BLOB(misc44, T33_MISC44_MINOR, int) = m->twin.minor;
	BLOB(misc44, T33_MISC44_THIS_DEVICE, struct device *) = m->twin.this_device;
	return 0;
}

static void priv_misc_deregister(struct miscdevice *misc44)
{
	struct t33_misc_twin *m = t33_misc_twin_of(misc44);
	int i;

	if (!m)
		return;
	misc_deregister(&m->twin);
	mutex_lock(&t33_twin_lock);
	for (i = 0; i < T33_MAX_TWINS; i++)
		if (t33_misc_twins[i] == m)
			t33_misc_twins[i] = NULL;
	mutex_unlock(&t33_twin_lock);
	kfree(m);
}

/*
 * ---- cdev: the tuning node -------------------------------------------------
 *
 * The firmware calls cdev_init() directly with its 108-byte fops; the objcopy
 * redirects cdev_init/cdev_del here.
 */
struct t33_cdev_twin {
	struct cdev *cdev;
	struct fw_fops fw;
};

static struct t33_cdev_twin *t33_cdev_twins[T33_MAX_TWINS];

void t33_cdev_init_44(struct cdev *cdev, const void *fops44)
{
	struct t33_cdev_twin *tw = kzalloc(sizeof(*tw), GFP_KERNEL);
	int i;

	if (!tw) {
		cdev_init(cdev, NULL);
		return;
	}
	tw->cdev = cdev;
	fw_fops_init(&tw->fw, fops44, NULL);
	mutex_lock(&t33_twin_lock);
	for (i = 0; i < T33_MAX_TWINS; i++) {
		if (!t33_cdev_twins[i]) {
			t33_cdev_twins[i] = tw;
			break;
		}
	}
	mutex_unlock(&t33_twin_lock);
	if (i == T33_MAX_TWINS) {
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
void t33_cdev_del_44(struct cdev *cdev)
{
	int i;
	bool live = false;

	mutex_lock(&t33_twin_lock);
	for (i = 0; i < T33_MAX_TWINS; i++) {
		if (t33_cdev_twins[i] && t33_cdev_twins[i]->cdev == cdev) {
			kfree(t33_cdev_twins[i]);
			t33_cdev_twins[i] = NULL;
			live = true;
		}
	}
	mutex_unlock(&t33_twin_lock);
	if (live)
		cdev_del(cdev);
}

/*
 * ---- /proc -----------------------------------------------------------------
 *
 * proc_create_data() takes a proc_ops since 5.6; tx-isp-twin.c builds one
 * from the firmware's fops. Its open handlers read their data back with
 * PDE_DATA(), redirected to t33_pde_data_44().
 */
static struct proc_dir_entry *t33_jz_root;

struct proc_dir_entry *jz_proc_mkdir(char *s)
{
	return t33_jz_root ? proc_mkdir(s, t33_jz_root) : NULL;
}

static struct proc_dir_entry *priv_proc_create_data(const char *name, umode_t mode,
						    struct proc_dir_entry *parent,
						    const void *fops44,
						    void *data)
{
	return t33_proc_create_data(name, mode, parent, fops44, data);
}

static void priv_proc_remove(struct proc_dir_entry *de)
{
	t33_proc_remove(de);
}

static struct proc_dir_entry *priv_jz_proc_mkdir(char *s)
{
	return jz_proc_mkdir(s);
}

static int priv_single_open_size(struct file *file, int (*show)(struct seq_file *, void *),
				 void *data, size_t size)
{
	return t33_single_open_size(file, show, data, size);
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
	return t33_platform_driver_register_44(drv, THIS_MODULE);
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

int private_request_threaded_irq(unsigned int irq, irq_handler_t handler,
				 irq_handler_t thread_fn, unsigned long flags,
				 const char *name, void *dev)
{
	return request_threaded_irq(irq, handler, thread_fn, flags, name, dev);
}
void private_enable_irq(unsigned int irq) { enable_irq(irq); }
void private_disable_irq(unsigned int irq) { disable_irq(irq); }
PRIVV(free_irq, (unsigned int irq, void *dev_id), (irq, dev_id))

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
						const struct proc_ops *fops44,
						void *data)
{
	return priv_proc_create_data(name, mode, parent, fops44, data);
}
struct proc_dir_entry *private_proc_create(const char *name, umode_t mode,
					   struct proc_dir_entry *parent,
					   const struct proc_ops *fops44)
{
	return priv_proc_create_data(name, mode, parent, fops44, NULL);
}
void private_remove_proc_entry(const char *name, struct proc_dir_entry *parent)
{
	remove_proc_entry(name, parent);
}
ssize_t private_seq_read(struct file *f, char __user *buf, size_t size, loff_t *ppos)
{
	return t33_seq_read(f, buf, size, ppos);
}
loff_t private_seq_lseek(struct file *f, loff_t offset, int whence)
{
	return t33_seq_lseek(f, offset, whence);
}
int private_single_release(struct inode *inode, struct file *f)
{
	return t33_single_release(inode, f);
}
PRIV(int, single_open_size, (struct file *file, int (*show)(struct seq_file *, void *),
			   void *data, size_t size), (file, show, data, size))
PRIV(struct proc_dir_entry *, jz_proc_mkdir, (char *s), (s))
PRIVV(proc_remove, (struct proc_dir_entry *de), (de))
void private_seq_printf(struct seq_file *m, const char *fmt, ...)
{
	struct seq_file *real = t33_seq_real(m);
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
	return size >= PAGE_SIZE ? vmalloc(size) : kmalloc(size, GFP_KERNEL);
}
void private_vfree(const void *addr)
{
	if (is_vmalloc_addr(addr))
		vfree(addr);
	else
		kfree(addr);
}
void *private_kmalloc(size_t s, gfp_t gfp) { return t33_priv_kmalloc(s, gfp); }
void private_kfree(void *p) { kfree(p); }
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
	return t33_filp_open(name, flags, mode);
}
int private_filp_close(struct file *f, fl_owner_t id) { return t33_filp_close(f, id); }
ssize_t private_vfs_read(struct file *f, void *buf, size_t count, loff_t *pos)
{
	return t33_vfs_read(f, (char __user *)buf, count, pos);
}
ssize_t private_vfs_write(struct file *f, const void *buf, size_t count, loff_t *pos)
{
	return t33_vfs_write(f, (const char __user *)buf, count, pos);
}
loff_t private_vfs_llseek(struct file *f, loff_t offset, int whence)
{
	return t33_vfs_llseek(f, offset, whence);
}
void private_dma_cache_sync(struct device *dev, void *vaddr, size_t size,
			    enum dma_data_direction dir)
{
	t33_priv_dma_cache_sync(dev, vaddr, size, dir);
}
void private_getrawmonotonic(struct timespec64 *ts)
{
	ktime_get_raw_ts64(ts);
}
struct net *private_get_init_net(void) { return t33_priv_get_init_net(); }
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
	return t33_queue_work_44(work);
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
static struct platform_device *t33_pins_dev;
static struct pinctrl *t33_pins;

static int t33_pins_get(void)
{
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "ingenic,t33-isp");
	if (!np)
		return -ENODEV;
	t33_pins_dev = platform_device_alloc("t33-isp-pins", PLATFORM_DEVID_NONE);
	if (!t33_pins_dev) {
		of_node_put(np);
		return -ENOMEM;
	}
	t33_pins_dev->dev.of_node = np;		/* reference passes to the device */
	ret = platform_device_add(t33_pins_dev);
	if (ret)
		goto err_put;
	t33_pins = pinctrl_get_select_default(&t33_pins_dev->dev);
	if (IS_ERR(t33_pins)) {
		ret = PTR_ERR(t33_pins);
		t33_pins = NULL;
		platform_device_unregister(t33_pins_dev);
		t33_pins_dev = NULL;
		return ret;
	}
	return 0;

err_put:
	platform_device_put(t33_pins_dev);
	t33_pins_dev = NULL;
	return ret;
}

static void t33_pins_put(void)
{
	if (t33_pins) {
		pinctrl_put(t33_pins);
		t33_pins = NULL;
	}
	if (t33_pins_dev) {
		platform_device_unregister(t33_pins_dev);
		t33_pins_dev = NULL;
	}
}

void tx_isp_abi_exit(void)
{
	int i;

	t33_pins_put();

	for (i = 0; i < ARRAY_SIZE(t33_clk_lookups); i++) {
		struct clk_lookup *cl = t33_clk_lookups[i];

		if (!cl)
			continue;
		t33_clk_lookups[i] = NULL;
		clkdev_drop(cl);
	}
	if (t33_jz_root) {
		proc_remove(t33_jz_root);
		t33_jz_root = NULL;
	}
}

int tx_isp_abi_init(void)
{
	int i, ret;

	/* the layouts the 5.15 firmware objects were built with (5.15.170, PRJ008_goat_defconfig) */
	BUILD_BUG_ON(sizeof(struct platform_device) != 304);
	BUILD_BUG_ON(sizeof(struct platform_driver) != 108);
	BUILD_BUG_ON(sizeof(struct resource) != 32);
	BUILD_BUG_ON(offsetof(struct platform_device, name) != T33_PDEV44_NAME);
	BUILD_BUG_ON(offsetof(struct platform_device, id) != T33_PDEV44_ID);
	BUILD_BUG_ON(offsetof(struct platform_device, dev) != T33_PDEV44_DEV);
	BUILD_BUG_ON(offsetof(struct platform_device, dev.platform_data) != 76);
	BUILD_BUG_ON(offsetof(struct platform_device, dev.coherent_dma_mask) != T33_PDEV44_DEV + T33_DEV44_COHERENT);
	BUILD_BUG_ON(offsetof(struct platform_device, num_resources) != T33_PDEV44_NUM_RES);
	BUILD_BUG_ON(offsetof(struct platform_device, resource) != T33_PDEV44_RES);
	BUILD_BUG_ON(offsetof(struct platform_driver, probe) != T33_PDRV44_PROBE);
	BUILD_BUG_ON(offsetof(struct platform_driver, remove) != T33_PDRV44_REMOVE);
	BUILD_BUG_ON(sizeof(struct miscdevice) != 40);
	BUILD_BUG_ON(offsetof(struct miscdevice, mode) != T33_MISC44_MODE);
	BUILD_BUG_ON(offsetof(struct miscdevice, nodename) != T33_MISC44_NODENAME);
	BUILD_BUG_ON(offsetof(struct miscdevice, this_device) != T33_MISC44_THIS_DEVICE);
	BUILD_BUG_ON(offsetof(struct miscdevice, parent) != T33_MISC44_PARENT);
	BUILD_BUG_ON(offsetof(struct device_driver, owner) != 8);

	ret = t33_pins_get();
	if (ret)
		pr_warn("t33-isp: no MCLK pin state from the ISP node (%d)\n", ret);

	t33_jz_root = proc_mkdir("jz", NULL);
	if (!t33_jz_root)
		pr_warn("t33-isp: /proc/jz exists or cannot be created\n");

	for (i = 0; i < ARRAY_SIZE(t33_clk_aliases); i++) {
		struct clk *clk = t33_dt_clk(i);
		struct clk_lookup *cl;

		if (IS_ERR(clk)) {
			pr_err("t33-isp: no clock for alias %s (%ld)\n",
			       t33_clk_aliases[i].alias, PTR_ERR(clk));
			ret = PTR_ERR(clk);
			goto err;
		}
		cl = clkdev_create(clk, t33_clk_aliases[i].alias, NULL);
		clk_put(clk);
		if (!cl) {
			ret = -ENOMEM;
			goto err;
		}
		t33_clk_lookups[i] = cl;
	}
	return 0;

err:
	tx_isp_abi_exit();
	return ret;
}
