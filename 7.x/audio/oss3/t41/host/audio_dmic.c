// SPDX-License-Identifier: GPL-2.0
/*
 * T41 AudioSystem DMIC capture for the audio.ko dsp core.
 *
 * One 64 KiB window holds the audio DMA with its FIFOs, a format converter
 * per channel, the audio bus and the DMIC itself. The DMIC is bus source 0,
 * routed to DMA channel 5, which walks a ring of linked descriptors, one per
 * fragment; the dsp core polls the channel's DBA for the position.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmapool.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include "../include/audio_common.h"
#include "../include/audio_debug.h"
#include "../include/audio_dsp.h"
#include "../include/audio_dmic.h"

/* offsets of the blocks in the window */
#define AS_DMA_BASE		0x0000
#define AS_FIFO_BASE		0x1000
#define AS_FMTCOV_BASE		0x2000
#define AS_DBUS_BASE		0x4000
#define AS_DMIC_BASE		0xa000

/* DMA, per channel */
#define DMA_CH_OFF		0x18
#define DBA(ch)			(0x0 + (ch) * DMA_CH_OFF)
#define DCR(ch)			(0x8 + (ch) * DMA_CH_OFF)
#define DSR(ch)			(0xc + (ch) * DMA_CH_OFF)
#define DCM(ch)			(0x10 + (ch) * DMA_CH_OFF)
#define DDA(ch)			(0x14 + (ch) * DMA_CH_OFF)
#define DGRR			0x100

#define DCR_RESET		BIT(1)
#define DCR_CTE			BIT(0)
#define DSR_TT_INT		BIT(5)
#define DSR_LTT_INT		BIT(4)
#define DSR_TT			BIT(3)
#define DSR_LTT			BIT(2)
#define DSR_RST_EN		BIT(1)
#define DCM_TIE			BIT(1)
#define DGRR_RESET		BIT(0)
#define DSR_ALL			(DSR_TT_INT | DSR_LTT_INT | DSR_LTT | DSR_TT)

/* a descriptor is four words */
#define DESC_LINK		BIT(2)
#define DESC_TSZ_SFT		4
#define DESC_BAI		BIT(8)
#define DESC_DTC_MASK		GENMASK(23, 0)
#define DMA_BURST_MAX		4	/* 32 words */
#define DMA_BURST_MAX_WORD	(1 << (DMA_BURST_MAX + 1))
#define DESC_MAX		64

struct dmic_dma_desc {
	__le32 flags;
	__le32 dba;
	__le32 ndoa;
	__le32 dtc;
};

/* FIFO, per channel */
#define FIFO_CH_OFF		0x10
#define FAS(ch)			(0x0 + (ch) * FIFO_CH_OFF)
#define FCR(ch)			(0x4 + (ch) * FIFO_CH_OFF)
#define FFR(ch)			(0x8 + (ch) * FIFO_CH_OFF)
#define FCR_FIFO_EN		BIT(0)
#define FFR_FTH_SFT		16
#define FFR_FTH_MSK		GENMASK(28, FFR_FTH_SFT)
#define FFR_FIFO_TD		BIT(7)
#define FIFO_DEPTH		384

/* format converter */
#define DFCR(ch)		(0x0 + (ch) * 4)
#define DFCR_ENABLE		BIT(0)
#define DFCR_CHNUM_SFT		12
#define DFCR_CHNUM_MSK		GENMASK(15, DFCR_CHNUM_SFT)
#define DFCR_SS_SFT		8
#define DFCR_SS_16		(3 << DFCR_SS_SFT)
#define DFCR_SS_24		(6 << DFCR_SS_SFT)

/* audio bus */
#define BTSET			0x00
#define BTCLR			0x04
#define BFCR0			0x10
#define BFCR1			0x14
#define BFCR2			0x18
#define BST0			0x1c
#define BTT0			0x28
#define BFCR0_DEV_TF_MSK	GENMASK(31, 16)
#define BFCR0_DEV_RF_MSK	GENMASK(15, 0)
#define BFCR0_DEV_TF(dev)	BIT((dev) + 16)
#define BFCR0_DEV_RF(dev)	BIT(dev)
#define BFCR1_DEV_DBE_MSK	GENMASK(11, 0)
#define BFCR2_DEV_DBE_MSK	GENMASK(14, 0)
#define BSORTT_MSK(sft)		GENMASK((sft) + 4, (sft))
#define BSORTT(slot, sft)	(((slot) << (sft)) & BSORTT_MSK(sft))
#define BUS_DMIC_DEV		0	/* the DMIC is source 0: BST0 bits 4:0 */
#define BUS_DMIC_SFT		0
#define BUS_DMA_DEV		5	/* DMA channel 5 is target 5: BTT0 bits 30:26 */
#define BUS_DMA_SFT		26
#define BUS_SLOT		1

/* DMIC */
#define DMIC_CR0		0x0
#define DMIC_GCR		0x4
#define DMIC_RESET		BIT(31)
#define HPF2_EN			BIT(22)
#define LPF_EN			BIT(21)
#define HPF1_EN			BIT(20)
#define CHNUM_SFT		16
#define CHNUM_MASK		GENMASK(19, CHNUM_SFT)
#define OSS_SFT			12
#define OSS_MASK		GENMASK(13, OSS_SFT)
#define OSS_24BIT		(0x1 << OSS_SFT)
#define SR_SFT			6
#define SR_MASK			GENMASK(8, SR_SFT)
#define DMIC_EN			BIT(0)

#define DMA_CHANNEL		5
#define DMIC_DMA_BUFFER_SIZE	(48000 * 2 * 4)
#define DEFAULT_GAIN		4
#define MAX_GAIN		0x1f

struct dmic_info {
	unsigned long record_rate;
	int record_channel;
	int record_format;
	unsigned int gain;
};

struct ingenic_dmic {
	struct device *dev;
	struct regmap *dma, *fifo, *fmtcov, *dbus, *regs;
	void __iomem *base;
	struct clk *clk;
	struct audio_pipe pipe;
	struct dmic_info info;

	struct dma_pool *desc_pool;
	struct dmic_dma_desc *descs[DESC_MAX];
	dma_addr_t descs_phy[DESC_MAX];
	unsigned int desc_cnt;
};

static struct ingenic_dmic *globe_dmic;

struct device *audio_dmic_dev(void)
{
	return globe_dmic ? globe_dmic->dev : NULL;
}

/* DMA */

static int dmic_dma_tsz(int period_bytes, int *ts_bytes)
{
	int tsz = 0, ts_words = 1;

	if (period_bytes & ((4 << 2) - 1)) {
		*ts_bytes = ts_words << 2;
		return tsz;
	}
	period_bytes >>= 4;
	ts_words <<= 2;
	tsz++;

	while (!(period_bytes & 1) && (ts_words << 1) <= FIFO_DEPTH) {
		period_bytes >>= 1;
		ts_words <<= 1;
		tsz++;
		if (ts_words >= DMA_BURST_MAX_WORD)
			break;
	}
	*ts_bytes = ts_words << 2;
	return tsz;
}

static void dmic_dma_free_descs(struct ingenic_dmic *dmic)
{
	unsigned int i;

	for (i = 0; i < dmic->desc_cnt; i++) {
		dma_pool_free(dmic->desc_pool, dmic->descs[i], dmic->descs_phy[i]);
		dmic->descs[i] = NULL;
	}
	dmic->desc_cnt = 0;
}

int ingenic_dmic_dma_init(dma_addr_t dma_addr, unsigned int fragment_size,
			  unsigned int fragment_cnt)
{
	struct ingenic_dmic *dmic = globe_dmic;
	int tsz, ts_bytes, fth;
	unsigned int i;

	if (!dmic || fragment_cnt > DESC_MAX || !fragment_cnt)
		return -EINVAL;

	/* a route restarted without a stop still holds its old ring */
	dmic_dma_free_descs(dmic);
	for (i = 0; i < fragment_cnt; i++) {
		dmic->descs[i] = dma_pool_zalloc(dmic->desc_pool, GFP_KERNEL,
						 &dmic->descs_phy[i]);
		if (!dmic->descs[i]) {
			dmic_dma_free_descs(dmic);
			return -ENOMEM;
		}
		dmic->desc_cnt++;
	}

	tsz = dmic_dma_tsz(fragment_size, &ts_bytes);
	for (i = 0; i < dmic->desc_cnt; i++) {
		struct dmic_dma_desc *d = dmic->descs[i];

		d->flags = cpu_to_le32(DESC_LINK | DESC_BAI | (tsz << DESC_TSZ_SFT));
		d->dba = cpu_to_le32(dma_addr + i * fragment_size);
		d->ndoa = cpu_to_le32(dmic->descs_phy[(i + 1) % dmic->desc_cnt]);
		d->dtc = cpu_to_le32((fragment_size / ts_bytes) & DESC_DTC_MASK);
	}

	/* the T41 BSP's FIFO thresholds, not derived from the burst as on the T32 */
	fth = fragment_size < 640 ? 16 : 32;
	regmap_write(dmic->fifo, FFR(DMA_CHANNEL),
		     ((fth << FFR_FTH_SFT) & FFR_FTH_MSK) | FFR_FIFO_TD);
	return 0;
}

int ingenic_dmic_dma_deinit(void)
{
	if (globe_dmic)
		dmic_dma_free_descs(globe_dmic);
	return 0;
}

dma_addr_t ingenic_dmic_get_dma_current_trans_addr(void)
{
	return readl_relaxed(globe_dmic->base + AS_DMA_BASE + DBA(DMA_CHANNEL));
}

static void dmic_dma_start(struct ingenic_dmic *dmic)
{
	regmap_write(dmic->dma, DSR(DMA_CHANNEL), DSR_ALL);
	/* the DMA is enabled first, then the FIFO */
	regmap_write(dmic->dma, DDA(DMA_CHANNEL), dmic->descs_phy[0]);
	regmap_write(dmic->dma, DCR(DMA_CHANNEL), DCR_CTE);
	regmap_write(dmic->fifo, FCR(DMA_CHANNEL), FCR_FIFO_EN);
}

static void dmic_dma_stop(struct ingenic_dmic *dmic)
{
	u32 val, timeout = 0xfff;

	regmap_update_bits(dmic->dma, DCM(DMA_CHANNEL), DCM_TIE, 0);
	regmap_write(dmic->dma, DSR(DMA_CHANNEL), DSR_ALL);
	regmap_write(dmic->fifo, FCR(DMA_CHANNEL), 0);
	regmap_write(dmic->dma, DCR(DMA_CHANNEL), 0);
	do {
		regmap_read(dmic->dma, DSR(DMA_CHANNEL), &val);
	} while ((val & DSR_RST_EN) && --timeout);
	regmap_write(dmic->dma, DCR(DMA_CHANNEL), DCR_RESET);
	udelay(10);
}

/* audio bus: DMIC (source 0) -> DMA channel 5 on one timeslot */

static void dmic_dbus_route(struct ingenic_dmic *dmic)
{
	regmap_update_bits(dmic->dbus, BFCR0, BFCR0_DEV_TF_MSK | BFCR0_DEV_RF_MSK,
			   BFCR0_DEV_RF(BUS_DMIC_DEV));
	regmap_update_bits(dmic->dbus, BFCR0, BFCR0_DEV_TF_MSK | BFCR0_DEV_RF_MSK,
			   BFCR0_DEV_TF(BUS_DMA_DEV));
	/* an empty transmit FIFO sends zeros, a full receive FIFO blocks */
	regmap_write(dmic->dbus, BFCR1, BFCR1_DEV_DBE_MSK);
	regmap_write(dmic->dbus, BFCR2, BFCR2_DEV_DBE_MSK);
	regmap_update_bits(dmic->dbus, BST0, BSORTT_MSK(BUS_DMIC_SFT),
			   BSORTT(BUS_SLOT, BUS_DMIC_SFT));
	regmap_update_bits(dmic->dbus, BTT0, BSORTT_MSK(BUS_DMA_SFT),
			   BSORTT(BUS_SLOT, BUS_DMA_SFT));
	regmap_write(dmic->dbus, BTSET, BIT(BUS_SLOT));
}

static void dmic_dbus_unroute(struct ingenic_dmic *dmic)
{
	regmap_update_bits(dmic->dbus, BTT0, BSORTT_MSK(BUS_DMA_SFT), 0);
	regmap_update_bits(dmic->dbus, BST0, BSORTT_MSK(BUS_DMIC_SFT), 0);
	regmap_write(dmic->dbus, BTCLR, BIT(BUS_SLOT));
}

/* format converter */

static u32 dmic_fmtcov_chnum(int ch)
{
	/* even counts and one channel are coded one lower */
	return (ch % 2 == 0 || ch == 1) ? ch - 1 : ch;
}

static void dmic_fmtcov_cfg(struct ingenic_dmic *dmic, int format, int channels)
{
	u32 ss = format <= 16 ? DFCR_SS_16 : DFCR_SS_24;

	regmap_write(dmic->fmtcov, DFCR(DMA_CHANNEL),
		     ss | ((dmic_fmtcov_chnum(channels) << DFCR_CHNUM_SFT) & DFCR_CHNUM_MSK));
}

/* DMIC */

static int dmic_set_gain(struct ingenic_dmic *dmic, unsigned int gain)
{
	gain = min_t(unsigned int, gain, MAX_GAIN);
	regmap_write(dmic->regs, DMIC_GCR, gain);
	dmic->info.gain = gain;
	return 0;
}

static int dmic_set_param(struct ingenic_dmic *dmic, struct audio_parameter *param)
{
	u32 sr, cr;

	if (param->channel < 1 || param->channel > 12) {
		audio_err_print("error dmic channel : %d.\n", param->channel);
		return -EINVAL;
	}
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
	case 96000:
		sr = 3;
		break;
	default:
		audio_err_print("error dmic samplerate : %u.\n", param->rate);
		return -EINVAL;
	}
	if (param->format != 16 && param->format != 24) {
		audio_info_print("dmic set format is illegal. DMIC support 16bits/24bit format!\n");
		return -EINVAL;
	}

	cr = ((param->channel - 1) << CHNUM_SFT) | (sr << SR_SFT) |
	     (param->format == 24 ? OSS_24BIT : 0);
	regmap_update_bits(dmic->regs, DMIC_CR0, CHNUM_MASK | SR_MASK | OSS_MASK, cr);
	dmic->info.record_rate = param->rate;
	dmic->info.record_channel = param->channel;
	dmic->info.record_format = param->format;
	dmic_fmtcov_cfg(dmic, param->format, param->channel);
	return 0;
}

static int dmic_reset(struct ingenic_dmic *dmic)
{
	unsigned int val, timeout = 0xfff;

	regmap_update_bits(dmic->regs, DMIC_CR0, DMIC_RESET, DMIC_RESET);
	do {
		regmap_read(dmic->regs, DMIC_CR0, &val);
	} while ((val & DMIC_RESET) && --timeout);
	if (!timeout) {
		dev_err(dmic->dev, "DMIC reset fail ...\n");
		return -EIO;
	}
	return 0;
}

static int dmic_start(struct ingenic_dmic *dmic)
{
	dmic_dma_start(dmic);
	regmap_update_bits(dmic->fmtcov, DFCR(DMA_CHANNEL), DFCR_ENABLE, DFCR_ENABLE);
	regmap_update_bits(dmic->regs, DMIC_CR0, DMIC_EN, DMIC_EN);
	return 0;
}

static int dmic_stop(struct ingenic_dmic *dmic)
{
	regmap_update_bits(dmic->regs, DMIC_CR0, DMIC_EN, 0);
	regmap_update_bits(dmic->fmtcov, DFCR(DMA_CHANNEL), DFCR_ENABLE, 0);
	dmic_dma_stop(dmic);
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
		return dmic_set_gain(dmic, *(unsigned int *)data);
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
	dmic->info.gain = DEFAULT_GAIN;

	dmic_reset(dmic);
	regmap_update_bits(dmic->regs, DMIC_CR0, HPF2_EN | LPF_EN | HPF1_EN,
			   HPF2_EN | LPF_EN | HPF1_EN);
	regmap_write(dmic->regs, DMIC_GCR, DEFAULT_GAIN);
	dmic_dbus_route(dmic);
	return 0;
}

static int dmic_deinit(void *route)
{
	struct ingenic_dmic *dmic = ((struct audio_route *)route)->pipe->priv;

	dmic->info.record_rate = 0;
	dmic->info.record_channel = 0;
	dmic->info.record_format = 0;
	dmic->info.gain = 0;
	dmic_dbus_unroute(dmic);
	dmic_dma_free_descs(dmic);
	return 0;
}

/* debug */

static int audio_dmic_show(struct seq_file *m, void *v)
{
	struct ingenic_dmic *dmic = m->private;

	seq_puts(m, "\nDmic Info:\n");
	seq_printf(m, "samplerate : %lu\n", dmic->info.record_rate);
	seq_printf(m, "channel : %d\n", dmic->info.record_channel);
	seq_printf(m, "format : %d\n", dmic->info.record_format);
	seq_printf(m, "gain : %u\n", dmic->info.gain);
	return 0;
}

static int audio_dmic_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, audio_dmic_show, pde_data(inode), 2048);
}

static ssize_t audio_dmic_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct ingenic_dmic *dmic = globe_dmic;
	char kbuf[16];
	unsigned int i;

	if (count >= sizeof(kbuf))
		return -EINVAL;
	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;
	kbuf[count] = '\0';
	if (!strncasecmp(kbuf, "reg", 3)) {
		pr_info("Dump Dmic Regs:\n");
		for (i = 0; i <= 0x20; i += 4)
			pr_info("reg[%04x], value = %04x\n", i,
				readl(dmic->base + AS_DMIC_BASE + i));
	}
	return count;
}

static const struct proc_ops audio_dmic_debug_fops = {
	.proc_read = seq_read,
	.proc_open = audio_dmic_open,
	.proc_write = audio_dmic_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/* platform driver */

static struct regmap *dmic_regmap(struct ingenic_dmic *dmic, const char *name,
				  unsigned int off, unsigned int max)
{
	const struct regmap_config cfg = {
		.reg_bits = 32,
		.reg_stride = 4,
		.val_bits = 32,
		.name = name,
		.max_register = max,
		.cache_type = REGCACHE_NONE,
	};

	return devm_regmap_init_mmio(dmic->dev, dmic->base + off, &cfg);
}

static int audio_dmic_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ingenic_dmic *dmic;
	struct audio_pipe *pipe;
	int ret;

	ret = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return dev_err_probe(dev, ret, "dma mask\n");
	dmic = devm_kzalloc(dev, sizeof(*dmic), GFP_KERNEL);
	if (!dmic)
		return -ENOMEM;
	dmic->dev = dev;

	dmic->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dmic->base))
		return PTR_ERR(dmic->base);

	dmic->dma = dmic_regmap(dmic, "dma", AS_DMA_BASE, 0x110);
	dmic->fifo = dmic_regmap(dmic, "fifo", AS_FIFO_BASE, 0x100);
	dmic->fmtcov = dmic_regmap(dmic, "fmtcov", AS_FMTCOV_BASE, 0x28);
	dmic->dbus = dmic_regmap(dmic, "dbus", AS_DBUS_BASE, 0x30);
	dmic->regs = dmic_regmap(dmic, "regs", AS_DMIC_BASE, 0x50);
	if (IS_ERR(dmic->dma) || IS_ERR(dmic->fifo) || IS_ERR(dmic->fmtcov) ||
	    IS_ERR(dmic->dbus) || IS_ERR(dmic->regs))
		return dev_err_probe(dev, -ENOMEM, "regmap\n");

	dmic->clk = devm_clk_get_enabled(dev, "gate_dmic");
	if (IS_ERR(dmic->clk))
		return dev_err_probe(dev, PTR_ERR(dmic->clk), "gate_dmic\n");

	dmic->desc_pool = dmam_pool_create("dmic_dma_desc_pool", dev,
					   sizeof(struct dmic_dma_desc),
					   __alignof__(struct dmic_dma_desc), 0);
	if (!dmic->desc_pool)
		return -ENOMEM;

	/* reset the audio DMA, and give channel 5 its FIFO */
	regmap_write(dmic->dma, DGRR, DGRR_RESET);
	regmap_write(dmic->fifo, FAS(DMA_CHANNEL), FIFO_DEPTH);

	pipe = &dmic->pipe;
	spin_lock_init(&pipe->pipe_lock);
	pipe->reservesize = PAGE_ALIGN(DMIC_DMA_BUFFER_SIZE);
	pipe->vaddr = dma_alloc_noncoherent(dev, pipe->reservesize, &pipe->paddr,
					    DMA_FROM_DEVICE, GFP_KERNEL);
	if (!pipe->vaddr)
		return -ENOMEM;
	pipe->is_trans = false;
	pipe->priv = dmic;
	pipe->init = dmic_init;
	pipe->ioctl = dmic_ioctl;
	pipe->deinit = dmic_deinit;

	globe_dmic = dmic;
	platform_set_drvdata(pdev, dmic);

	if (register_audio_pipe(pipe, AUDIO_ROUTE_DMIC_ID) != AUDIO_SUCCESS) {
		audio_err_print("failed to register dmic pipe.\n");
		ret = -EBUSY;
		goto err_free;
	}
	if (register_audio_debug_ops("dmic_info", &audio_dmic_debug_fops, dmic) != AUDIO_SUCCESS) {
		audio_err_print("failed to register audio dmic debug proc file!\n");
		ret = -ENOMEM;
		goto err_pipe;
	}
	dev_info(dev, "dmic init success.\n");
	return 0;

err_pipe:
	release_audio_pipe(pipe);
err_free:
	globe_dmic = NULL;
	dma_free_noncoherent(dev, pipe->reservesize, pipe->vaddr, pipe->paddr, DMA_FROM_DEVICE);
	return ret;
}

static void audio_dmic_remove(struct platform_device *pdev)
{
	struct ingenic_dmic *dmic = platform_get_drvdata(pdev);

	release_audio_pipe(&dmic->pipe);
	globe_dmic = NULL;
	dma_free_noncoherent(dmic->dev, dmic->pipe.reservesize, dmic->pipe.vaddr,
			     dmic->pipe.paddr, DMA_FROM_DEVICE);
}

static const struct of_device_id audio_dmic_of_match[] = {
	{ .compatible = "ingenic,t41-dmic" },
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
