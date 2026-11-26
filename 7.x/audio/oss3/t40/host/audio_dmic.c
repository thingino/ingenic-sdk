// SPDX-License-Identifier: GPL-2.0
/*
 * T40 DMIC capture for the audio.ko dsp core.
 *
 * The DMIC packs two 16-bit samples into each word of its 64-word FIFO and
 * asks the PDMA for data at the FIFO threshold; the dsp core runs the
 * cyclic transfer and polls its residue like the AIC's.
 */
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include "../include/audio_common.h"
#include "../include/audio_debug.h"
#include "../include/audio_dsp.h"
#include "../include/audio_dmic.h"

#define DMIC_CR0		0x00
#define DMIC_GCR		0x04
#define DMIC_IMR		0x08
#define DMIC_DR			0x30
#define DMIC_FTHR		0x34
#define DMIC_FSR		0x38
#define DMIC_CGDIS		0x50

#define CR0_RESET		BIT(31)
#define CR0_CHNUM_MASK		GENMASK(18, 16)
#define CR0_UNPACK_MSB		BIT(13)
#define CR0_UNPACK_DIS		BIT(12)
#define CR0_SW_LR		BIT(11)
#define CR0_PACK_EN		BIT(8)
#define CR0_SR_MASK		GENMASK(7, 6)
#define CR0_LP_MODE		BIT(3)
#define CR0_HPF1		BIT(2)
#define CR0_EN			BIT(0)
#define GCR_GAIN_MASK		GENMASK(3, 0)
#define IMR_ALL			GENMASK(5, 0)
#define FTHR_RDMS		BIT(31)
#define FTHR_THR_MASK		GENMASK(5, 0)

#define DMIC_FIFO_THR		32	/* words; also the DMA burst */
#define DMIC_DMA_BUFFER_SIZE	(16000 * 2 * 4)
#define DMIC_MAX_CHANNELS	4

struct dmic_info {
	unsigned long record_rate;
	int record_channel;
	int record_format;
	unsigned int gain;
};

struct ingenic_dmic {
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct audio_pipe pipe;
	struct dmic_info info;
};

static void dmic_update(struct ingenic_dmic *dmic, unsigned int reg, u32 mask, u32 val)
{
	u32 tmp = readl(dmic->base + reg);

	writel((tmp & ~mask) | (val & mask), dmic->base + reg);
}

static void dmic_reset(struct ingenic_dmic *dmic)
{
	u32 val;

	dmic_update(dmic, DMIC_CR0, CR0_RESET, CR0_RESET);
	if (readl_poll_timeout_atomic(dmic->base + DMIC_CR0, val, !(val & CR0_RESET), 1, 100))
		dev_warn(dmic->dev, "reset did not complete\n");
}

static void dmic_set_gain(struct ingenic_dmic *dmic, unsigned int gain)
{
	gain = min_t(unsigned int, gain, GCR_GAIN_MASK);
	dmic_update(dmic, DMIC_GCR, GCR_GAIN_MASK, gain);
	dmic->info.gain = gain;
}

static int dmic_set_param(struct ingenic_dmic *dmic, struct audio_parameter *param)
{
	u32 sr;

	switch (param->rate) {
	case 8000:
		sr = 0;
		break;
	case 16000:
		sr = 1;
		break;
	case 48000:
		sr = 2;
		break;
	default:
		audio_err_print("error dmic samplerate : %u.\n", param->rate);
		return -EINVAL;
	}
	if (param->channel < 1 || param->channel > DMIC_MAX_CHANNELS) {
		audio_err_print("error dmic channel : %d.\n", param->channel);
		return -EINVAL;
	}
	/* the samples are always 16 bits; the format is not checked */
	dmic_update(dmic, DMIC_CR0, CR0_SR_MASK | CR0_CHNUM_MASK,
		    FIELD_PREP(CR0_SR_MASK, sr) |
		    FIELD_PREP(CR0_CHNUM_MASK, param->channel - 1));
	dmic->info.record_rate = param->rate;
	dmic->info.record_channel = param->channel;
	return 0;
}

static int dmic_start(struct ingenic_dmic *dmic)
{
	dmic_reset(dmic);
	/* two samples per FIFO word, the first in the low half */
	dmic_update(dmic, DMIC_CR0, CR0_UNPACK_MSB | CR0_UNPACK_DIS | CR0_PACK_EN,
		    CR0_UNPACK_MSB | CR0_UNPACK_DIS | CR0_PACK_EN);
	dmic_update(dmic, DMIC_FTHR, FTHR_RDMS, FTHR_RDMS);
	/* LP_MODE runs the microphone clock at 1.2 MHz instead of 2.4 MHz */
	dmic_update(dmic, DMIC_CR0, CR0_HPF1 | CR0_LP_MODE | CR0_SW_LR,
		    CR0_HPF1 | CR0_LP_MODE);
	writel(IMR_ALL, dmic->base + DMIC_IMR);
	dmic_update(dmic, DMIC_CR0, CR0_EN, CR0_EN);
	return 0;
}

static int dmic_stop(struct ingenic_dmic *dmic)
{
	dmic_update(dmic, DMIC_FTHR, FTHR_RDMS, 0);
	dmic_update(dmic, DMIC_CR0, CR0_EN, 0);
	return 0;
}

static int dmic_ioctl(void *pipe, unsigned int cmd, void *data)
{
	struct ingenic_dmic *dmic = ((struct audio_pipe *)pipe)->priv;

	switch (cmd) {
	case AUDIO_CMD_CONFIG_PARAM:
		return dmic_set_param(dmic, data);
	case AUDIO_CMD_GET_GAIN:
		*(unsigned long *)data = dmic->info.gain;
		return 0;
	case AUDIO_CMD_SET_GAIN:
		dmic_set_gain(dmic, *(unsigned int *)data);
		return 0;
	case AUDIO_CMD_ENABLE_STREAM:
		return dmic_start(dmic);
	case AUDIO_CMD_DISABLE_STREAM:
		return dmic_stop(dmic);
	default:
		return 0;
	}
}

static int dmic_init(void *route)
{
	struct ingenic_dmic *dmic = ((struct audio_route *)route)->pipe->priv;

	dmic->info.record_rate = 0;
	dmic->info.record_channel = 0;
	dmic->info.record_format = 16;

	dmic_reset(dmic);
	dmic_set_gain(dmic, 0);
	dmic_update(dmic, DMIC_FTHR, FTHR_THR_MASK, DMIC_FIFO_THR);
	dmic_update(dmic, DMIC_CR0, CR0_SR_MASK | CR0_CHNUM_MASK, 0);
	return 0;
}

static int dmic_deinit(void *route)
{
	struct ingenic_dmic *dmic = ((struct audio_route *)route)->pipe->priv;

	dmic->info.record_rate = 0;
	dmic->info.record_channel = 0;
	dmic->info.gain = 0;
	return 0;
}

static int audio_dmic_show(struct seq_file *m, void *v)
{
	struct ingenic_dmic *dmic = m->private;
	static const u8 regs[] = { 0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c,
				   0x20, 0x34, 0x38, 0x50 };
	unsigned int i;

	seq_puts(m, "\nDmic attr list :\n");
	seq_printf(m, "The living rate of dmic : %lu\n", dmic->info.record_rate);
	seq_printf(m, "The living channel of dmic : %d\n", dmic->info.record_channel);
	seq_printf(m, "The living format of dmic : %d\n", dmic->info.record_format);
	seq_puts(m, "The living datatype of dmic : 16\n");
	seq_printf(m, "The living gain of dmic : %u\n", dmic->info.gain);
	for (i = 0; i < ARRAY_SIZE(regs); i++)
		seq_printf(m, "reg[%04x] = 0x%08x\n", regs[i], readl(dmic->base + regs[i]));
	return 0;
}

static int audio_dmic_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, audio_dmic_show, pde_data(inode), 2048);
}

static const struct proc_ops audio_dmic_debug_fops = {
	.proc_read = seq_read,
	.proc_open = audio_dmic_open,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int audio_dmic_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ingenic_dmic *dmic;
	struct audio_pipe *pipe;
	struct resource *res;
	int ret;

	ret = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return dev_err_probe(dev, ret, "dma mask\n");
	dmic = devm_kzalloc(dev, sizeof(*dmic), GFP_KERNEL);
	if (!dmic)
		return -ENOMEM;
	dmic->dev = dev;

	dmic->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(dmic->base))
		return PTR_ERR(dmic->base);

	dmic->clk = devm_clk_get_enabled(dev, "gate_dmic");
	if (IS_ERR(dmic->clk))
		return dev_err_probe(dev, PTR_ERR(dmic->clk), "gate_dmic\n");

	pipe = &dmic->pipe;
	spin_lock_init(&pipe->pipe_lock);
	pipe->dma_config.direction = DMA_DEV_TO_MEM;
	pipe->dma_config.src_addr = res->start + DMIC_DR;
	pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	pipe->dma_config.src_maxburst = DMIC_FIFO_THR;
	pipe->dma_config.dst_maxburst = DMIC_FIFO_THR;

	pipe->dma_chan = dma_request_chan(dev, "rx");
	if (IS_ERR(pipe->dma_chan))
		return dev_err_probe(dev, PTR_ERR(pipe->dma_chan), "rx dma\n");
	dmaengine_slave_config(pipe->dma_chan, &pipe->dma_config);

	pipe->reservesize = PAGE_ALIGN(DMIC_DMA_BUFFER_SIZE);
	pipe->vaddr = dma_alloc_noncoherent(dev, pipe->reservesize, &pipe->paddr,
					    DMA_FROM_DEVICE, GFP_KERNEL);
	if (!pipe->vaddr) {
		ret = -ENOMEM;
		goto err_chan;
	}
	pipe->is_trans = false;
	pipe->priv = dmic;
	pipe->init = dmic_init;
	pipe->ioctl = dmic_ioctl;
	pipe->deinit = dmic_deinit;
	dmic->info.record_format = 16;
	platform_set_drvdata(pdev, dmic);

	if (register_audio_pipe(pipe, AUDIO_ROUTE_DMIC_ID) != AUDIO_SUCCESS) {
		audio_err_print("failed to register dmic pipe.\n");
		ret = -EBUSY;
		goto err_free;
	}
	if (register_audio_debug_ops("audio_dmic_info", &audio_dmic_debug_fops, dmic) !=
	    AUDIO_SUCCESS) {
		audio_err_print("failed to register audio dmic debug proc file!\n");
		ret = -ENOMEM;
		goto err_pipe;
	}
	dev_info(dev, "dmic init success.\n");
	return 0;

err_pipe:
	release_audio_pipe(pipe);
err_free:
	dma_free_noncoherent(dev, pipe->reservesize, pipe->vaddr, pipe->paddr, DMA_FROM_DEVICE);
err_chan:
	dma_release_channel(pipe->dma_chan);
	return ret;
}

static void audio_dmic_remove(struct platform_device *pdev)
{
	struct ingenic_dmic *dmic = platform_get_drvdata(pdev);
	struct audio_pipe *pipe = &dmic->pipe;

	release_audio_pipe(pipe);
	dmaengine_terminate_sync(pipe->dma_chan);
	dma_release_channel(pipe->dma_chan);
	dma_free_noncoherent(dmic->dev, pipe->reservesize, pipe->vaddr, pipe->paddr,
			     DMA_FROM_DEVICE);
}

static const struct of_device_id audio_dmic_of_match[] = {
	{ .compatible = "ingenic,t40-dmic" },
	{ }
};
MODULE_DEVICE_TABLE(of, audio_dmic_of_match);

static struct platform_driver audio_dmic_driver = {
	.probe = audio_dmic_probe,
	.remove = audio_dmic_remove,
	.driver = {
		.name = "jz-dmic",
		.of_match_table = audio_dmic_of_match,
	},
};

int audio_dmic_register(void)
{
	return platform_driver_register(&audio_dmic_driver);
}

void audio_dmic_unregister(void)
{
	platform_driver_unregister(&audio_dmic_driver);
}
