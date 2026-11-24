// SPDX-License-Identifier: GPL-2.0
/*
 * The T10 and T20 ISP's platform devices. The vendor kernel registers these
 * from its board code (soc-t20/common/platform.c); here they follow the DT
 * node "ingenic,t20-isp" ("ingenic,t10-isp"), which also supplies the INTC
 * source, the clocks and the CIM pad state.
 */
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include <mach/tx_isp.h>
#include "tx-isp-common.h"

/* The T10 has no CSI gate (and no MIPI receiver): only the ISP clock is held */
#if defined(CONFIG_SOC_T10)
#define TX_ISP_SOC_COMPAT	"ingenic,t10-isp"
#define TX_ISP_GLUE_NAME	"tx-isp-t10-glue"
#define TX_ISP_KEEP_CLKS	1
#define TX_ISP_CORE_CGU_RATE	45000000
#define TX_ISP_BOOT_RATE	ISP_CLK_960P_MODE
#else
#define TX_ISP_SOC_COMPAT	"ingenic,t20-isp"
#define TX_ISP_GLUE_NAME	"tx-isp-t20-glue"
#define TX_ISP_KEEP_CLKS	2
#define TX_ISP_CORE_CGU_RATE	85000000
#define TX_ISP_BOOT_RATE	ISP_CLK_1080P_MODE
#endif

#define MIPI_CSI_IOBASE	0x10040000
#define ISP_VIC_IOBASE	0x13300000
#define ISP_IRQ_IOBASE	0x13320000
#define ISP_CORE_IOBASE	0x13380000

static struct device_node *tx_isp_np;
/*
 * clk_disable_unused() gates the ISP and CSI at boot, and tx-isp touches
 * their registers (its IRQ glue) before the pipeline opens them, so they are
 * taken here for as long as the module is loaded.
 */
static struct clk *tx_isp_keep_clks[TX_ISP_KEEP_CLKS];

/*
 * The vendor clk_get(NULL, "cgu_isp") style names resolve to the DT node's
 * clock-names.
 */
struct clk *tx_isp_clk_get(struct device *dev, const char *name)
{
	if (!tx_isp_np)
		return ERR_PTR(-ENODEV);
	return of_clk_get_by_name(tx_isp_np, name);
}
EXPORT_SYMBOL(tx_isp_clk_get);

/*
 * Mainline's Ingenic CGU refuses a rate its dividers cannot hit exactly; the
 * vendor driver asked for nominal rates (90 MHz from a 1 GHz MPLL).
 */
int tx_isp_clk_set_rate(struct clk *clk, unsigned long rate)
{
	long r = clk_round_rate(clk, rate);

	return clk_set_rate(clk, r > 0 ? r : rate);
}
EXPORT_SYMBOL(tx_isp_clk_set_rate);

/* "ispmem=SIZE@BASE" as the vendor kernel's command line option */
unsigned long ispmem_base;
unsigned long ispmem_size;
EXPORT_SYMBOL(ispmem_base);
EXPORT_SYMBOL(ispmem_size);

static char *ispmem;
module_param(ispmem, charp, 0444);
MODULE_PARM_DESC(ispmem, "temporal noise reduction and WDR buffers, SIZE@BASE");

static void tx_isp_pdev_release(struct device *dev)
{
}

static u64 tx_isp_dma_mask = DMA_BIT_MASK(32);

static struct resource tx_isp_csi_resource[] = {
	{
		.name = "mipi_csi",
		.start = MIPI_CSI_IOBASE,
		.end = MIPI_CSI_IOBASE + 0x1000,
		.flags = IORESOURCE_MEM,
	},
};

static struct tx_isp_subdev_clk_info tx_csi_clk_info[] = {
	{"csi", DUMMY_CLOCK_RATE},
};

static struct tx_isp_subdev_platform_data tx_isp_subdev_csi = {
	.grp_id = TX_ISP_CSI_GRP_IDX,
	.clks = tx_csi_clk_info,
	.clk_num = ARRAY_SIZE(tx_csi_clk_info),
};

static struct resource tx_isp_vic_resource[] = {
	{
		.name = "isp_vic",
		.start = ISP_VIC_IOBASE,
		.end = ISP_VIC_IOBASE + 0x10000,
		.flags = IORESOURCE_MEM,
	},
};

static struct tx_isp_subdev_platform_data tx_isp_subdev_vic = {
	.grp_id = TX_ISP_VIC_GRP_IDX,
};

static struct resource tx_isp_core_resource[] = {
	{
		.name = "isp_core",
		.start = ISP_CORE_IOBASE,
		.end = ISP_CORE_IOBASE + 0x80000,
		.flags = IORESOURCE_MEM,
	},
};

static struct tx_isp_subdev_clk_info isp_core_clk_info[] = {
	{"cgu_isp", TX_ISP_CORE_CGU_RATE},
	{"isp", DUMMY_CLOCK_RATE},
};

static struct tx_isp_subdev_platform_data tx_isp_subdev_core = {
	.grp_id = TX_ISP_CORE_GRP_IDX,
	.clks = isp_core_clk_info,
	.clk_num = ARRAY_SIZE(isp_core_clk_info),
};

static struct resource tx_isp_device_resource[] = {
	{
		.name = "isp_device_irq",
		.start = ISP_IRQ_IOBASE,
		.end = ISP_IRQ_IOBASE + 0x10000,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = "isp_irq_id",
		.flags = IORESOURCE_IRQ,
	},
};

static struct tx_isp_subdev_platform_data tx_isp_subdev_video_in = {
	.grp_id = TX_ISP_VIDEO_IN_GRP_IDX,
};

#define TX_ISP_PDEV(_name, _pdata, _res)				\
	{								\
		.name = _name,						\
		.id = -1,						\
		.dev = {						\
			.dma_mask = &tx_isp_dma_mask,			\
			.coherent_dma_mask = DMA_BIT_MASK(32),		\
			.platform_data = _pdata,			\
			.release = tx_isp_pdev_release,			\
		},							\
		.num_resources = ARRAY_SIZE(_res),			\
		.resource = _res,					\
	}

/* the core, VIC and CSI are only data holders for tx-isp, which binds last */
static struct platform_device tx_isp_pdevs[] = {
	TX_ISP_PDEV(TX_ISP_CORE_NAME, &tx_isp_subdev_core, tx_isp_core_resource),
	TX_ISP_PDEV(TX_ISP_VIC_NAME, &tx_isp_subdev_vic, tx_isp_vic_resource),
	TX_ISP_PDEV(TX_ISP_CSI_NAME, &tx_isp_subdev_csi, tx_isp_csi_resource),
	TX_ISP_PDEV("tx-isp", &tx_isp_subdev_video_in, tx_isp_device_resource),
};

static void tx_isp_release_keep_clks(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tx_isp_keep_clks); i++) {
		if (!tx_isp_keep_clks[i])
			continue;
		clk_disable_unprepare(tx_isp_keep_clks[i]);
		clk_put(tx_isp_keep_clks[i]);
		tx_isp_keep_clks[i] = NULL;
	}
}

static int tx_isp_glue_probe(struct platform_device *pdev)
{
	int irq, i, ret;
	char *rest;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;
	tx_isp_device_resource[1].start = irq;
	tx_isp_device_resource[1].end = irq;

	if (ispmem) {
		ispmem_size = memparse(ispmem, &rest);
		if (*rest == '@')
			ispmem_base = memparse(rest + 1, NULL);
	}

	tx_isp_np = of_node_get(pdev->dev.of_node);
	for (i = 0; i < ARRAY_SIZE(tx_isp_keep_clks); i++) {
		tx_isp_keep_clks[i] = of_clk_get_by_name(tx_isp_np, i ? "csi" : "isp");
		if (IS_ERR(tx_isp_keep_clks[i])) {
			ret = PTR_ERR(tx_isp_keep_clks[i]);
			tx_isp_keep_clks[i] = NULL;
			goto err_clks;
		}
		/* the ISP gate must not open at the bare MPLL rate: silicon hangs */
		if (!i) {
			ret = tx_isp_clk_set_rate(tx_isp_keep_clks[i], TX_ISP_BOOT_RATE);
			if (ret)
				pr_warn("tx-isp: ISP clock rate: %d\n", ret);
		}
		ret = clk_prepare_enable(tx_isp_keep_clks[i]);
		if (ret) {
			clk_put(tx_isp_keep_clks[i]);
			tx_isp_keep_clks[i] = NULL;
			goto err_clks;
		}
	}
	for (i = 0; i < ARRAY_SIZE(tx_isp_pdevs); i++) {
		tx_isp_pdevs[i].dev.parent = &pdev->dev;
		ret = platform_device_register(&tx_isp_pdevs[i]);
		if (ret)
			goto err;
	}
	return 0;
err:
	while (--i >= 0)
		platform_device_unregister(&tx_isp_pdevs[i]);
err_clks:
	tx_isp_release_keep_clks();
	of_node_put(tx_isp_np);
	tx_isp_np = NULL;
	return ret;
}

static void tx_isp_glue_remove(struct platform_device *pdev)
{
	int i;

	for (i = ARRAY_SIZE(tx_isp_pdevs) - 1; i >= 0; i--)
		platform_device_unregister(&tx_isp_pdevs[i]);
	tx_isp_release_keep_clks();
	of_node_put(tx_isp_np);
	tx_isp_np = NULL;
}

static const struct of_device_id tx_isp_glue_of_match[] = {
	{ .compatible = TX_ISP_SOC_COMPAT },
	{ }
};
MODULE_DEVICE_TABLE(of, tx_isp_glue_of_match);

struct platform_driver tx_isp_glue_driver = {
	.probe = tx_isp_glue_probe,
	.remove = tx_isp_glue_remove,
	.driver = {
		.name = TX_ISP_GLUE_NAME,
		.of_match_table = tx_isp_glue_of_match,
	},
};
