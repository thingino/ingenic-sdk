// SPDX-License-Identifier: GPL-2.0
/*
 * bscaler.ko: /dev/bscaler0 for the T40 BSCALER, after the vendor T40 4.4.94
 * kernel's drivers/video/ingenic_bscaler.
 *
 * libvenus (Magik nna1) programs the block itself through /dev/mem; this
 * driver only clocks it and lets user space wait for the completion of its
 * two parts: FRMT (resize, status at +0x80/+0x84/+0xb0) and FRMC (format
 * conversion, +0x00/+0x30).
 */
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#define IOCTL_BSCALER0_IRQ_GET_STAT	0x000
#define IOCTL_BSCALER1_IRQ_GET_STAT	0x100

#define BSCALER_NUM		2	/* 0: FRMT, 1: FRMC */
#define BSCALER_RATE		400000000
#define BSCALER_TIMEOUT_MS	300

#define REG_FRMC_STAT		0x00
#define REG_FRMC_CTRL		0x30
#define REG_FRMT_STAT		0x80
#define REG_FRMT_STAT1		0x84
#define REG_FRMT_CTRL		0xb0

#define STAT_DONE		BIT(8)
#define FRMT_STAT1_DONE		BIT(2)
#define CTRL_IRQ		(BIT(6) | BIT(4))
#define CTRL_TIMEOUT		BIT(6)

struct bscaler_ctx {
	int irq;
	struct completion done;
	u32 status;
};

struct bscaler {
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct clk *ahb0;
	struct bscaler_ctx ctx[BSCALER_NUM];
	struct miscdevice mdev;
	struct mutex lock;		/* opencnt */
	int opencnt;
};

static void bscaler_clear(struct bscaler *bs, unsigned int reg, u32 bits)
{
	u32 v = readl(bs->base + reg);

	if (v & bits)
		writel(v & ~bits, bs->base + reg);
}

static irqreturn_t bscaler_irq_frmt(int irq, void *data)
{
	struct bscaler *bs = data;

	bscaler_clear(bs, REG_FRMT_STAT, STAT_DONE);
	bscaler_clear(bs, REG_FRMT_STAT1, FRMT_STAT1_DONE);
	if (readl(bs->base + REG_FRMT_CTRL) & CTRL_TIMEOUT)
		dev_err(bs->dev, "FRMT timeout\n");
	bscaler_clear(bs, REG_FRMT_CTRL, CTRL_IRQ);

	/* both parts report +0x00, as the vendor driver */
	bs->ctx[0].status = readl(bs->base + REG_FRMC_STAT);
	complete(&bs->ctx[0].done);
	return IRQ_HANDLED;
}

static irqreturn_t bscaler_irq_frmc(int irq, void *data)
{
	struct bscaler *bs = data;

	bscaler_clear(bs, REG_FRMC_STAT, STAT_DONE);
	if (readl(bs->base + REG_FRMC_CTRL) & CTRL_TIMEOUT)
		dev_err(bs->dev, "FRMC timeout\n");
	bscaler_clear(bs, REG_FRMC_CTRL, CTRL_IRQ);

	bs->ctx[1].status = readl(bs->base + REG_FRMC_STAT);
	complete(&bs->ctx[1].done);
	return IRQ_HANDLED;
}

static int bscaler_open(struct inode *inode, struct file *file)
{
	struct bscaler *bs = container_of(file->private_data, struct bscaler, mdev);
	int i, ret = 0;

	mutex_lock(&bs->lock);
	if (!bs->opencnt) {
		ret = clk_prepare_enable(bs->ahb0);
		if (ret)
			goto out;
		ret = clk_prepare_enable(bs->clk);
		if (ret) {
			clk_disable_unprepare(bs->ahb0);
			goto out;
		}
		for (i = 0; i < BSCALER_NUM; i++) {
			reinit_completion(&bs->ctx[i].done);
			enable_irq(bs->ctx[i].irq);
		}
	}
	bs->opencnt++;
out:
	mutex_unlock(&bs->lock);
	return ret;
}

static int bscaler_release(struct inode *inode, struct file *file)
{
	struct bscaler *bs = container_of(file->private_data, struct bscaler, mdev);
	int i;

	mutex_lock(&bs->lock);
	if (--bs->opencnt == 0) {
		for (i = 0; i < BSCALER_NUM; i++)
			disable_irq(bs->ctx[i].irq);
		clk_disable_unprepare(bs->clk);
		clk_disable_unprepare(bs->ahb0);
	}
	mutex_unlock(&bs->lock);
	return 0;
}

static long bscaler_wait(struct bscaler *bs, int idx, u32 __user *arg)
{
	struct bscaler_ctx *ctx = &bs->ctx[idx];

	if (!wait_for_completion_timeout(&ctx->done,
					 msecs_to_jiffies(BSCALER_TIMEOUT_MS))) {
		dev_err(bs->dev, "%s wait timeout\n", idx ? "FRMC" : "FRMT");
		return -ETIMEDOUT;
	}
	return put_user(ctx->status, arg);
}

static long bscaler_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bscaler *bs = container_of(file->private_data, struct bscaler, mdev);

	switch (cmd) {
	case IOCTL_BSCALER0_IRQ_GET_STAT:
		return bscaler_wait(bs, 0, (u32 __user *)arg);
	case IOCTL_BSCALER1_IRQ_GET_STAT:
		return bscaler_wait(bs, 1, (u32 __user *)arg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations bscaler_fops = {
	.owner		= THIS_MODULE,
	.open		= bscaler_open,
	.release	= bscaler_release,
	.unlocked_ioctl	= bscaler_ioctl,
};

static int bscaler_probe(struct platform_device *pdev)
{
	static const char * const irq_names[BSCALER_NUM] = { "frmt", "frmc" };
	irq_handler_t handlers[BSCALER_NUM] = { bscaler_irq_frmt, bscaler_irq_frmc };
	struct device *dev = &pdev->dev;
	struct bscaler *bs;
	struct clk *epll;
	int i, ret;

	bs = devm_kzalloc(dev, sizeof(*bs), GFP_KERNEL);
	if (!bs)
		return -ENOMEM;
	bs->dev = dev;
	mutex_init(&bs->lock);

	bs->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(bs->base))
		return PTR_ERR(bs->base);

	bs->clk = devm_clk_get(dev, "bscaler");
	if (IS_ERR(bs->clk))
		return dev_err_probe(dev, PTR_ERR(bs->clk), "no bscaler clock\n");
	bs->ahb0 = devm_clk_get(dev, "ahb0");
	if (IS_ERR(bs->ahb0))
		return dev_err_probe(dev, PTR_ERR(bs->ahb0), "no ahb0 clock\n");

	/* the vendor driver runs it from the EPLL at 400 MHz */
	epll = clk_get(NULL, "epll");
	if (!IS_ERR(epll)) {
		clk_set_parent(bs->clk, epll);
		clk_put(epll);
	}
	clk_set_rate(bs->clk, BSCALER_RATE);

	for (i = 0; i < BSCALER_NUM; i++) {
		bs->ctx[i].irq = platform_get_irq_byname(pdev, irq_names[i]);
		if (bs->ctx[i].irq < 0)
			return bs->ctx[i].irq;
		init_completion(&bs->ctx[i].done);
		ret = devm_request_irq(dev, bs->ctx[i].irq, handlers[i],
				       IRQF_NO_AUTOEN, irq_names[i], bs);
		if (ret)
			return ret;
	}

	bs->mdev.minor = MISC_DYNAMIC_MINOR;
	bs->mdev.name = "bscaler0";
	bs->mdev.fops = &bscaler_fops;
	bs->mdev.parent = dev;
	ret = misc_register(&bs->mdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, bs);
	dev_info(dev, "BSCALER at %lu Hz\n", clk_get_rate(bs->clk));
	return 0;
}

static void bscaler_remove(struct platform_device *pdev)
{
	struct bscaler *bs = platform_get_drvdata(pdev);

	misc_deregister(&bs->mdev);
}

static const struct of_device_id bscaler_of_match[] = {
	{ .compatible = "ingenic,t40-bscaler" },
	{ }
};
MODULE_DEVICE_TABLE(of, bscaler_of_match);

static struct platform_driver bscaler_driver = {
	.probe	= bscaler_probe,
	.remove	= bscaler_remove,
	.driver	= {
		.name		= "bscaler",
		.of_match_table	= bscaler_of_match,
	},
};
module_platform_driver(bscaler_driver);

MODULE_DESCRIPTION("Ingenic T40 BSCALER completion device");
MODULE_LICENSE("GPL");
