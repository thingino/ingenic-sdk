// SPDX-License-Identifier: GPL-2.0
/*
 * Linux 7.x glue for the T32 Pro device layer: what the vendor kernel gave it
 * by fixed number or by name, taken from the device tree here.
 *
 *  - The interrupts: board/platform.c lists the vendor INTC numbers; 7.x maps
 *    the INTC linearly, so each is replaced by the Linux number of the DT
 *    node's interrupt. The nodes stay status = "disabled": nothing else binds
 *    them.
 *  - The ISP node's pinctrl default state (the sensor MCLK pin) is applied
 *    through a device of its own: as the tx-isp device's of_node, the node
 *    would also replace the interrupt resources above.
 *  - The clocks: the device layer and the sensors ask for the vendor CGU's
 *    names with any device; each is aliased to the 7.x CGU clock that has
 *    the mux, divider and gate in one.
 */
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>

#include <dt-bindings/clock/ingenic,t32-cgu.h>

#include "lib_base.h"
#include "tx-isp-7x.h"

extern struct platform_device tx_isp_platform_device;

static const struct {
	int index;
	const char *compat;
	const char *name;
} tx_isp_irqs[] = {
	{ TISP_VIC_IRQ, "ingenic,t32pro-isp", "vic" },
	{ TISP_LDC_IRQ, "ingenic,t32pro-ldc", "ldc" },
	{ TISP_ISP_IRQ, "ingenic,t32pro-isp", "isp" },
	{ TISP_IVDC_IRQ, "ingenic,t32pro-ivdc", "ivdc" },
};

static const struct {
	const char *alias;
	u32 index;
} tx_isp_clk_aliases[] = {
	{ "div_isp", T32_CLK_ISP }, { "mux_isp", T32_CLK_ISP }, { "gate_isp", T32_CLK_ISP },
	{ "div_ispa", T32_CLK_ISPA }, { "mux_ispa", T32_CLK_ISPA },
	{ "gate_csi", T32_CLK_MIPI_CSI }, { "gate_ivdc", T32_CLK_IVDC },
	{ "div_ldc", T32_CLK_LDC }, { "mux_ldc", T32_CLK_LDC }, { "gate_ldc", T32_CLK_LDC },
	{ "div_vpu", T32_CLK_VPU }, { "mux_vpu", T32_CLK_VPU }, { "gate_vpu", T32_CLK_VPU },
	{ "div_cim", T32_CLK_CIM }, { "mux_cim", T32_CLK_CIM },
	{ "ext", T32_CLK_EXCLK }, { "apll", T32_CLK_APLL }, { "mpll", T32_CLK_MPLL },
	{ "vpll", T32_CLK_VPLL }, { "sclka", T32_CLK_SCLKA },
};

static struct clk_lookup *tx_isp_clk_lookups[ARRAY_SIZE(tx_isp_clk_aliases)];
static struct platform_device *tx_isp_pins_dev;
static struct pinctrl *tx_isp_pins;

static struct clk *tx_isp_cgu_clk(struct device_node *cgu, u32 index)
{
	struct of_phandle_args args = { .np = cgu, .args_count = 1 };

	args.args[0] = index;
	return of_clk_get_from_provider(&args);
}

void tx_isp_7x_exit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tx_isp_clk_lookups); i++) {
		if (tx_isp_clk_lookups[i])
			clkdev_drop(tx_isp_clk_lookups[i]);
		tx_isp_clk_lookups[i] = NULL;
	}
	if (tx_isp_pins)
		pinctrl_put(tx_isp_pins);
	tx_isp_pins = NULL;
	if (tx_isp_pins_dev)
		platform_device_unregister(tx_isp_pins_dev);
	tx_isp_pins_dev = NULL;
}

static int tx_isp_pins_get(void)
{
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "ingenic,t32pro-isp");
	if (!np)
		return -ENODEV;
	tx_isp_pins_dev = platform_device_alloc("tx-isp-pins", PLATFORM_DEVID_NONE);
	if (!tx_isp_pins_dev) {
		of_node_put(np);
		return -ENOMEM;
	}
	tx_isp_pins_dev->dev.of_node = np;	/* the reference passes to the device */
	ret = platform_device_add(tx_isp_pins_dev);
	if (ret) {
		platform_device_put(tx_isp_pins_dev);
		tx_isp_pins_dev = NULL;
		return ret;
	}
	tx_isp_pins = pinctrl_get_select_default(&tx_isp_pins_dev->dev);
	if (IS_ERR(tx_isp_pins)) {
		ret = PTR_ERR(tx_isp_pins);
		tx_isp_pins = NULL;
		return ret;
	}
	return 0;
}

int tx_isp_7x_init(void)
{
	struct resource *res = tx_isp_platform_device.resource;
	struct device_node *np;
	int i, irq, ret = 0;

	for (i = 0; i < ARRAY_SIZE(tx_isp_irqs); i++) {
		np = of_find_compatible_node(NULL, NULL, tx_isp_irqs[i].compat);
		if (!np)
			return -ENODEV;
		irq = of_irq_get_byname(np, tx_isp_irqs[i].name);
		if (irq <= 0)
			irq = of_irq_get(np, 0);
		of_node_put(np);
		if (irq <= 0) {
			pr_err("tx-isp: no %s interrupt in the DT (%d)\n", tx_isp_irqs[i].name, irq);
			return irq ? irq : -ENXIO;
		}
		res[tx_isp_irqs[i].index].start = irq;
		res[tx_isp_irqs[i].index].end = irq;
	}

	ret = tx_isp_pins_get();
	if (ret)
		pr_warn("tx-isp: no MCLK pin state from the ISP node (%d)\n", ret);
	ret = 0;

	np = of_find_compatible_node(NULL, NULL, "ingenic,t32pro-cgu");
	if (!np) {
		ret = -ENODEV;
		goto err;
	}
	for (i = 0; i < ARRAY_SIZE(tx_isp_clk_aliases); i++) {
		struct clk *clk = tx_isp_cgu_clk(np, tx_isp_clk_aliases[i].index);

		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			break;
		}
		tx_isp_clk_lookups[i] = clkdev_create(clk, tx_isp_clk_aliases[i].alias, NULL);
		clk_put(clk);
		if (!tx_isp_clk_lookups[i]) {
			ret = -ENOMEM;
			break;
		}
	}
	of_node_put(np);
	if (!ret)
		return 0;
err:
	tx_isp_7x_exit();
	return ret;
}
