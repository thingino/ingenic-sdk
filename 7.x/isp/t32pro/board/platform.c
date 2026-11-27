#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <soc/base.h>

#include "lib_base.h"
#include "tx-isp-device.h"

static u64 tx_isp_module_dma_mask = ~(u64)0;

static void tx_isp_release_device(struct device *dev)
{
	/*ISP_INFO("isp device release!\n");*/
	return;
}

static struct resource tx_isp_resource[] = {
	[TISP_VIC_IRQ] = /* vic intp */
	{
		.name = TX_ISP_IRQ_ID,
		.start = 30 + 8,
		.end = 30 + 8,
		.flags = IORESOURCE_IRQ,
	},
	[TISP_LDC_IRQ] = /* ldc intp */
	{
		.name = TX_ISP_IRQ_ID,
		.start = 38 + 8,
		.end = 38 + 8,
		.flags = IORESOURCE_IRQ,
	},
	[TISP_ISP_IRQ] = /* isp intp */
	{
		.name = TX_ISP_IRQ_ID,
		.start = 29 + 8,
		.end = 29 + 8,
		.flags = IORESOURCE_IRQ,
	},
	[TISP_IVDC_IRQ] = /* ivdc intp*/
	{
		.name = TX_ISP_IRQ_ID,
		.start = 57 + 8,
		.end = 57 + 8,
		.flags = IORESOURCE_IRQ,
	},

	[TISP_MAX_IRQ + 0] = /* tiziano */
	{
		.name = TX_ISP_DEV_NAME,
		.start = ISP_CORE_IOBASE,
		.end = ISP_CORE_IOBASE + 0x100000 - 1,
		.flags = IORESOURCE_MEM,
	},
	[TISP_MAX_IRQ + 1] = /* csi main / single-sensor host */
	{
		.name = TX_ISP_DEV_NAME,
		.start = MIPI_RX_4L_IOBASE, /* 0x10054000 */
		.end = MIPI_RX_4L_IOBASE + 0x1000 - 1,
		.flags = IORESOURCE_MEM,
	},
	[TISP_MAX_IRQ + 2] = /* csi sub host */
	{
		.name = TX_ISP_DEV_NAME,
		.start = MIPI_CSI_IOBASE, /* 0x10023000 */
		.end = MIPI_CSI_IOBASE + 0x1000 - 1,
		.flags = IORESOURCE_MEM,
	},
	[TISP_MAX_IRQ + 3] = /* ivdc */
	{
		.name = TX_ISP_DEV_NAME,
		.start = IVDC_IOBASE,
		.end = IVDC_IOBASE + 0x10000 - 1,
		.flags = IORESOURCE_MEM,
	},
	[TISP_MAX_IRQ + 4] = /* mipi phy */
	{
		.name = TX_ISP_DEV_NAME,
		.start = MIPI_PHY_IOBASE, /* 0x10022000 */
		.end = MIPI_PHY_IOBASE + 0x1000 - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device tx_isp_platform_device = {
	.name = "tx-isp",
	.id = -1,
	.dev =
		{
			.dma_mask = &tx_isp_module_dma_mask,
			.coherent_dma_mask = 0xffffffff,
			// .platform_data = NULL,
			.release = tx_isp_release_device,
		},
	.num_resources = ARRAY_SIZE(tx_isp_resource),
	.resource = tx_isp_resource,
};
