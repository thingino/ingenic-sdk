/*
 * Platform device support for Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <soc/gpio.h>
#include <soc/base.h>

#include <txx-isp.h>
#include <txx-funcs.h>
#include "tx-isp-frame-channel.h"
#include "compat/tx_isp_glue.h"
#include <tx-isp-common.h>

static u64 tx_isp_module_dma_mask = ~(u64)0;


static void tx_isp_release_device(struct device *dev)
{
	/*printk("isp device release!\n");*/
	return;
}

/* the widget is the controller of  all sensors */
struct tx_isp_widget_descriptor tx_isp_widget_vin = {
	.type = TX_ISP_TYPE_WIDGET,
	.subtype = TX_ISP_SUBTYPE_INPUT_TERMINAL,
	.parentid = TX_ISP_SUBDEV_ID(0),
	.unitid = TX_ISP_WIDGET_ID(0),
	.clks_num = 0,
};

struct platform_device tx_isp_vin_platform_device = {
	.name = TX_ISP_VIN_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_widget_vin,
		.release = tx_isp_release_device,
	},
	.num_resources = 0,
};


/* the controller is mipi interfaces*/
static struct resource tx_isp_csi_resource[] = {
	[0] = {
		/*.name = "mipi_csi",*/
		.name = TX_ISP_DEV_NAME,
		.start = MIPI_CSI_IOBASE,
		.end = MIPI_CSI_IOBASE + 0x1000 - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct tx_isp_device_clk isp_csi_clk_info[] = {
	{"csi", DUMMY_CLOCK_RATE},
};

struct tx_isp_widget_descriptor tx_isp_widget_csi = {
	.type = TX_ISP_TYPE_WIDGET,
	.subtype = TX_ISP_SUBTYPE_SELECTOR_UNIT,
	.parentid = TX_ISP_SUBDEV_ID(0),
	.unitid = TX_ISP_WIDGET_ID(2),
	.clks_num = ARRAY_SIZE(isp_csi_clk_info),
	.clks = isp_csi_clk_info,
};

struct platform_device tx_isp_csi_platform_device = {
	.name = TX_ISP_CSI_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_widget_csi,
		.release = tx_isp_release_device,
	},
	.num_resources = ARRAY_SIZE(tx_isp_csi_resource),
	.resource = tx_isp_csi_resource,
};

/* It's a process unit that connects several interfaces of sensor */
static struct resource tx_isp_vic_resource[] = {
	[0] = {
		.name = TX_ISP_DEV_NAME,
		.start = ISP_VIC_IOBASE,
		.end = ISP_VIC_IOBASE + 0x21000 - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct tx_isp_widget_descriptor tx_isp_widget_vic = {
	.type = TX_ISP_TYPE_WIDGET,
	.subtype = TX_ISP_SUBTYPE_CONTROLLER,
	.parentid = TX_ISP_SUBDEV_ID(0),
	.unitid = TX_ISP_WIDGET_ID(1),
	.clks_num = 0,
};

struct platform_device tx_isp_vic_platform_device = {
	.name = TX_ISP_VIC_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_widget_vic,
		.release = tx_isp_release_device,
	},
	.num_resources = ARRAY_SIZE(tx_isp_vic_resource),
	.resource = tx_isp_vic_resource,
};

/* It's the first process unit */
static struct resource tx_isp_core_resource[] = {
	[0] = {
		.name = TX_ISP_DEV_NAME,
		/*.name = "isp_device",*/
		.start = ISP_CORE_IOBASE,
		.end = ISP_CORE_IOBASE + 0x80000 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		/*.name = "isp_irq_id",*/
		.name = TX_ISP_IRQ_ID,
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_IRQ,
	},
};

static struct tx_isp_device_clk isp_core_clk_info[] = {
	{"cgu_isp", 85000000},
	{"isp", DUMMY_CLOCK_RATE},
};

struct tx_isp_pad_descriptor tx_isp_core_pads[] = {
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_LFB,
	},
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_LFB,
	},
	{
		.type = TX_ISP_PADTYPE_UNDEFINE,
	},
};

struct tx_isp_subdev_descriptor tx_isp_subdev_ispcore = {
	.type = TX_ISP_TYPE_SUBDEV,
	.subtype = TX_ISP_SUBTYPE_PROCESSING_UNIT,
	.parentid = TX_ISP_HEADER_ID(0),
	.unitid = TX_ISP_SUBDEV_ID(0),
	.clks_num = ARRAY_SIZE(isp_core_clk_info),
	.clks = isp_core_clk_info,
	.pads_num = ARRAY_SIZE(tx_isp_core_pads),
	.pads = tx_isp_core_pads,
};

struct platform_device tx_isp_core_platform_device = {
	.name = TX_ISP_CORE_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_subdev_ispcore,
		.release = tx_isp_release_device,
	},
	.num_resources = ARRAY_SIZE(tx_isp_core_resource),
	.resource = tx_isp_core_resource,
};

/* It's the second process unit */
static struct resource tx_isp_ldc_resource[] = {
	[0] = {
		.name = TX_ISP_DEV_NAME,
		/*.name = "isp_device",*/
		.start = ISP_LDC_IOBASE,
		.end = ISP_LDC_IOBASE + 0x1000 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		/*.name = "isp_irq_id",*/
		.name = TX_ISP_IRQ_ID,
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_IRQ,
	},
};

static struct tx_isp_device_clk isp_ldc_clk_info[] = {
	{"ldc", DUMMY_CLOCK_RATE},
	{"ahb1", DUMMY_CLOCK_RATE},
};

struct tx_isp_pad_descriptor tx_isp_ldc_pads[] = {
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_DDR,
	},
	{
		.type = TX_ISP_PADTYPE_INPUT,
		.links_type = TX_ISP_PADLINK_DDR,
	},
};

struct tx_isp_subdev_descriptor tx_isp_subdev_ldc = {
	.type = TX_ISP_TYPE_SUBDEV,
	.subtype = TX_ISP_SUBTYPE_PROCESSING_UNIT,
	.parentid = TX_ISP_HEADER_ID(0),
	.unitid = TX_ISP_SUBDEV_ID(1),
	.clks_num = ARRAY_SIZE(isp_ldc_clk_info),
	.clks = isp_ldc_clk_info,
	.pads_num = ARRAY_SIZE(tx_isp_ldc_pads),
	.pads = tx_isp_ldc_pads,
};

struct platform_device tx_isp_ldc_platform_device = {
	.name = TX_ISP_LDC_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_subdev_ldc,
		.release = tx_isp_release_device,
	},
	.num_resources = ARRAY_SIZE(tx_isp_ldc_resource),
	.resource = tx_isp_ldc_resource,
};

/* It's the thrid process unit */
static struct resource tx_isp_ncu_resource[] = {
	[0] = {
		.name = TX_ISP_DEV_NAME,
		.start = ISP_NCU_IOBASE,
		.end = ISP_NCU_IOBASE + 0x1000 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = TX_ISP_IRQ_ID,
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_IRQ,
	},
};

static struct tx_isp_device_clk isp_ncu_clk_info[] = {
	{"ncu", DUMMY_CLOCK_RATE},
};

struct tx_isp_pad_descriptor tx_isp_ncu_pads[] = {
	{
		.type = TX_ISP_PADTYPE_INPUT,
		.links_type = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_LFB,
	},
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_LFB,
	},
};

struct tx_isp_subdev_descriptor tx_isp_subdev_ncu = {
	.type = TX_ISP_TYPE_SUBDEV,
	.subtype = TX_ISP_SUBTYPE_PROCESSING_UNIT,
	.parentid = TX_ISP_HEADER_ID(0),
	.unitid = TX_ISP_SUBDEV_ID(2),
	.clks_num = ARRAY_SIZE(isp_ncu_clk_info),
	.clks = isp_ncu_clk_info,
	.pads_num = ARRAY_SIZE(tx_isp_ncu_pads),
	.pads = tx_isp_ncu_pads,
};

struct platform_device tx_isp_ncu_platform_device = {
	.name = TX_ISP_NCU_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_subdev_ncu,
		.release = tx_isp_release_device,
	},
	.num_resources = ARRAY_SIZE(tx_isp_ncu_resource),
	.resource = tx_isp_ncu_resource,
};

/* It's the fourth process unit */
static struct resource tx_isp_ms_resource[] = {
	[0] = {
		.name = TX_ISP_DEV_NAME,
		.start = ISP_MSCALER_IOBASE,
		.end = ISP_MSCALER_IOBASE + 0x1000 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = TX_ISP_IRQ_ID,
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_IRQ,
	},
};

static struct tx_isp_device_clk isp_ms_clk_info[] = {
	{"mscaler", DUMMY_CLOCK_RATE},
};

struct tx_isp_pad_descriptor tx_isp_ms_pads[] = {
	{
		.type = TX_ISP_PADTYPE_INPUT,
		.links_type = TX_ISP_PADLINK_LFB,
	},
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_DDR,
	},
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_DDR,
	},
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_DDR,
	},
};

struct tx_isp_subdev_descriptor tx_isp_subdev_ms = {
	.type = TX_ISP_TYPE_SUBDEV,
	.subtype = TX_ISP_SUBTYPE_PROCESSING_UNIT,
	.parentid = TX_ISP_HEADER_ID(0),
	.unitid = TX_ISP_SUBDEV_ID(3),
	.clks_num = ARRAY_SIZE(isp_ms_clk_info),
	.clks = isp_ms_clk_info,
	.pads_num = ARRAY_SIZE(tx_isp_ms_pads),
	.pads = tx_isp_ms_pads,
};

struct platform_device tx_isp_ms_platform_device = {
	.name = TX_ISP_MSCALER_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_subdev_ms,
		.release = tx_isp_release_device,
	},
	.num_resources = ARRAY_SIZE(tx_isp_ms_resource),
	.resource = tx_isp_ms_resource,
};

struct tx_isp_pad_descriptor tx_isp_fs_pads[] = {
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_DDR,
	},
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_DDR,
	},
	{
		.type = TX_ISP_PADTYPE_OUTPUT,
		.links_type = TX_ISP_PADLINK_DDR,
	},
};

struct tx_isp_subdev_descriptor tx_isp_subdev_framesource = {
	.type = TX_ISP_TYPE_SUBDEV,
	.subtype = TX_ISP_SUBTYPE_OUTPUT_TERMINAL,
	.parentid = TX_ISP_HEADER_ID(0),
	.unitid = TX_ISP_SUBDEV_ID(4),
	.clks_num = 0,
	.pads_num = ARRAY_SIZE(tx_isp_fs_pads),
	.pads = tx_isp_fs_pads,
};

struct platform_device tx_isp_fs_platform_device = {
	.name = TX_ISP_FS_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_subdev_framesource,
		.release = tx_isp_release_device,
	},
	.num_resources = 0,
};

struct platform_device *tx_isp_devices[] = {
	/* isp core and its widgets */
	&tx_isp_core_platform_device,
	&tx_isp_vic_platform_device,
	&tx_isp_csi_platform_device,
	&tx_isp_vin_platform_device,
	/* ldc module and its widgets */
	&tx_isp_ldc_platform_device,
	/* ncu module and its widgets */
	&tx_isp_ncu_platform_device,
	/* mscaler module and its widgets */
	&tx_isp_ms_platform_device,
	/* framesource module and its widgets */
	&tx_isp_fs_platform_device,
};


/* It's the root device */
struct tx_isp_device_descriptor tx_video_device = {
	.type = TX_ISP_TYPE_HEADER,
	.subtype = TX_ISP_SUBTYPE_CONTROLLER,
	.parentid = -1,
	.unitid = TX_ISP_HEADER_ID(0),
	.entity_num = ARRAY_SIZE(tx_isp_devices),
	.entities = tx_isp_devices,
};

struct platform_device tx_isp_platform_device = {
	.name = "tx-isp",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_video_device,
		.release = tx_isp_release_device,
	},
	.num_resources = 0,
};

static struct device_node *tx_isp_np;
static struct pinctrl *tx_isp_pinctrl;

/*
 * clk_disable_unused() gates the ISP and its units at boot while tx-isp
 * touches their registers before the pipeline opens them, so the glue holds
 * them for as long as the module is loaded.
 */
static const char * const tx_isp_keep_names[] = {
	"isp", "csi", "ncu", "ldc", "ahb1", "mscaler",
};
static struct clk *tx_isp_keep_clks[ARRAY_SIZE(tx_isp_keep_names)];

/* the vendor clk_get() names resolve to the DT node's clock-names */
struct clk *tx_isp_clk_get(struct device *dev, const char *name)
{
	if (!tx_isp_np)
		return ERR_PTR(-ENODEV);
	return of_clk_get_by_name(tx_isp_np, name);
}
EXPORT_SYMBOL(tx_isp_clk_get);

/*
 * Mainline's Ingenic CGU refuses a rate its dividers cannot hit exactly; the
 * vendor driver asks for nominal rates (90 MHz from a 1 GHz MPLL).
 */
int tx_isp_clk_set_rate(struct clk *clk, unsigned long rate)
{
	long r = clk_round_rate(clk, rate);

	return clk_set_rate(clk, r > 0 ? r : rate);
}
EXPORT_SYMBOL(tx_isp_clk_set_rate);

/*
 * The sensors' set_sensor_gpio_function() masks select the DVP pad groups;
 * on 7.x those are the node's pin states.
 */
int tx_isp_dvp_pins(enum gpio_port port, enum gpio_function func,
		    unsigned long pins)
{
	const char *name;
	struct pinctrl_state *st;

	switch (pins) {
	case 0x000343ff:
		name = "dvp-10bit-low";
		break;
	case 0x00034ffc:
		name = "dvp-10bit-high";
		break;
	case 0x00034fff:
		name = "dvp-12bit";
		break;
	default:
		pr_warn("tx-isp: no DVP pin group for PA mask 0x%lx\n", pins);
		return -ENOSYS;
	}
	if (IS_ERR_OR_NULL(tx_isp_pinctrl))
		return -ENODEV;
	st = pinctrl_lookup_state(tx_isp_pinctrl, name);
	if (IS_ERR(st)) {
		pr_warn("tx-isp: DT has no pin state %s\n", name);
		return PTR_ERR(st);
	}
	return pinctrl_select_state(tx_isp_pinctrl, st);
}

/* "ispmem=SIZE@BASE" as the vendor kernel's command line option */
unsigned long ispmem_base;
unsigned long ispmem_size;
EXPORT_SYMBOL(ispmem_base);
EXPORT_SYMBOL(ispmem_size);

static char *ispmem;
module_param(ispmem, charp, 0444);
MODULE_PARM_DESC(ispmem, "temporal noise reduction, WDR and LDC buffers, SIZE@BASE");

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
	static const struct {
		const char *name;
		struct resource *res;
	} irqs[] = {
		{ "isp", &tx_isp_core_resource[1] },
		{ "ldc", &tx_isp_ldc_resource[1] },
		{ "ncu", &tx_isp_ncu_resource[1] },
		{ "mscaler", &tx_isp_ms_resource[1] },
	};
	char *rest;
	int i, irq, ret;

	for (i = 0; i < ARRAY_SIZE(irqs); i++) {
		irq = platform_get_irq_byname(pdev, irqs[i].name);
		if (irq < 0)
			return irq;
		irqs[i].res->start = irq;
		irqs[i].res->end = irq;
	}

	if (ispmem) {
		ispmem_size = memparse(ispmem, &rest);
		if (*rest == '@')
			ispmem_base = memparse(rest + 1, NULL);
	}

	tx_isp_np = of_node_get(pdev->dev.of_node);
	tx_isp_pinctrl = pinctrl_get(&pdev->dev);
	for (i = 0; i < ARRAY_SIZE(tx_isp_keep_clks); i++) {
		tx_isp_keep_clks[i] = of_clk_get_by_name(tx_isp_np,
							 tx_isp_keep_names[i]);
		if (IS_ERR(tx_isp_keep_clks[i])) {
			ret = PTR_ERR(tx_isp_keep_clks[i]);
			tx_isp_keep_clks[i] = NULL;
			goto err_clks;
		}
		/* the ISP gate must not open at the bare PLL rate: silicon hangs */
		if (!i) {
			ret = tx_isp_clk_set_rate(tx_isp_keep_clks[i],
						  ISP_CLK_1080P_MODE);
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

	for (i = 0; i < ARRAY_SIZE(tx_isp_devices); i++)
		tx_isp_devices[i]->dev.parent = &pdev->dev;
	tx_isp_platform_device.dev.parent = &pdev->dev;

	ret = tx_isp_start();
	if (ret)
		goto err_clks;
	return 0;
err_clks:
	tx_isp_release_keep_clks();
	if (!IS_ERR_OR_NULL(tx_isp_pinctrl))
		pinctrl_put(tx_isp_pinctrl);
	tx_isp_pinctrl = NULL;
	of_node_put(tx_isp_np);
	tx_isp_np = NULL;
	return ret;
}

static void tx_isp_glue_remove(struct platform_device *pdev)
{
	tx_isp_stop();
	/* the vendor kernel never unloads /proc/jz; a module has to take it down */
	jz_proc_cleanup();
	tx_isp_release_keep_clks();
	if (!IS_ERR_OR_NULL(tx_isp_pinctrl))
		pinctrl_put(tx_isp_pinctrl);
	tx_isp_pinctrl = NULL;
	of_node_put(tx_isp_np);
	tx_isp_np = NULL;
}

static const struct of_device_id tx_isp_glue_of_match[] = {
	{ .compatible = "ingenic,t30-isp" },
	{ }
};
MODULE_DEVICE_TABLE(of, tx_isp_glue_of_match);

static struct platform_driver tx_isp_glue_driver = {
	.probe = tx_isp_glue_probe,
	.remove = tx_isp_glue_remove,
	.driver = {
		.name = "tx-isp-t30-glue",
		.of_match_table = tx_isp_glue_of_match,
	},
};
module_platform_driver(tx_isp_glue_driver);

/* the ioctl numbers libimp 1.0.5 sends encode these sizes */
static_assert(sizeof(struct frame_image_format) == 76);
static_assert(sizeof(struct tx_v4l2_buffer) == 68);
static_assert(sizeof(struct v4l2_requestbuffers) == 20);
static_assert(sizeof(struct isp_image_tuning_default_ctrl) == 12);
static_assert(sizeof(struct v4l2_input) == 80);
static_assert(sizeof(struct v4l2_control) == 8);
