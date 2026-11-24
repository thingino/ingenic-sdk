/**
 * xb_snd_card.c: the /dev/dsp device and the AIC platform driver (7.4)
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <mach/jzsnd.h>
#include "interface/xb_snd_dsp.h"

unsigned int snd_debug_level = 1;
module_param(snd_debug_level, int, S_IRUGO);
MODULE_PARM_DESC(snd_debug_level, "The debug level of sound driver");

int codec_type = 0;

static struct snd_dev_data *g_ddata = &i2s_data;

/********************************************************\
 * llseek
\********************************************************/
static loff_t xb_snd_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t ret = 0;
	struct snd_dev_data *ddata = g_ddata;

	switch (SND_DEV_DSP) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_llseek(file, offset, origin, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * read
\********************************************************/
static ssize_t xb_snd_read(struct file *file, char __user * buffer,
			   size_t count, loff_t * ppos)
{
	ssize_t ret = -EIO;
	struct snd_dev_data *ddata = g_ddata;

	if (ddata->is_suspend)
		return -EIO;

	if (count == 0)
		return 0;

	switch (SND_DEV_DSP) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_read(file, buffer, count, ppos, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * write
\********************************************************/
static ssize_t xb_snd_write(struct file *file, const char __user * buffer,
			    size_t count, loff_t * ppos)
{
	ssize_t ret = -EIO;
	struct snd_dev_data *ddata = g_ddata;

	if (ddata->is_suspend)
		return -EIO;

	if (count == 0)
		return 0;

	switch (SND_DEV_DSP) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_write(file, buffer, count, ppos, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * poll
\********************************************************/
static unsigned int xb_snd_poll(struct file *file, poll_table * wait)
{
	unsigned int ret = 0;
	struct snd_dev_data *ddata = g_ddata;

	switch (SND_DEV_DSP) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_poll(file, wait, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * ioctl
\********************************************************/
static long xb_snd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -EIO;
	struct snd_dev_data *ddata = g_ddata;
	switch (SND_DEV_DSP) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_ioctl(file, cmd, arg, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * open
\********************************************************/
static int xb_snd_open(struct inode *inode, struct file *file)
{
	int ret = -ENXIO;
	struct snd_dev_data *ddata = g_ddata;

	switch (SND_DEV_DSP) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_open(inode, file, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * release
\********************************************************/
static int xb_snd_release(struct inode *inode, struct file *file)
{
	int ret = -EINVAL;
	struct snd_dev_data *ddata = g_ddata;

	switch (SND_DEV_DSP) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_release(inode, file, ddata);
		break;
	}

	return ret;
}

const struct file_operations xb_snd_fops = {
	.owner = THIS_MODULE,
	.llseek = xb_snd_llseek,
	.read = xb_snd_read,
	.write = xb_snd_write,
	.poll = xb_snd_poll,
	.unlocked_ioctl = xb_snd_ioctl,
	.open = xb_snd_open,
	.release = xb_snd_release,
};

/*
 * The AIC is the platform device; /dev/dsp is a misc device that exists
 * while it is bound.
 */
static struct miscdevice xb_snd_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEV_DSP_NAME,
	.fops = &xb_snd_fops,
};

static int xb_snd_probe(struct platform_device *pdev)
{
	struct snd_dev_data *ddata = g_ddata;
	int ret;

	/* DT-registered platform devices have no preset dma_mask. */
	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "dma mask\n");

	pdev->dev.platform_data = ddata;
	ddata->dev = &pdev->dev;
	ddata->is_suspend = false;

	ret = ddata->init(pdev);
	if (ret) {
		snd_error_print("device init error!\n");
		return ret ? ret : -EINVAL;
	}

	ret = xb_snd_dsp_probe(ddata);
	if (ret < 0) {
		snd_error_print("SOUND ERROR:Snd device register error.\n");
		goto err_exit;
	}

	ret = misc_register(&xb_snd_misc);
	if (ret) {
		snd_error_print("SOUND ERROR: register /dev/dsp failed\n");
		goto err_dsp;
	}
	return 0;

err_dsp:
	xb_snd_dsp_remove(ddata);
err_exit:
	ddata->exit(pdev);
	return ret;
}

static void xb_snd_remove(struct platform_device *pdev)
{
	struct snd_dev_data *ddata = pdev->dev.platform_data;

	misc_deregister(&xb_snd_misc);
	if (ddata) {
		xb_snd_dsp_remove(ddata);
		ddata->exit(pdev);
	}
}

static void xb_snd_shutdown(struct platform_device *pdev)
{
	struct snd_dev_data *ddata = pdev->dev.platform_data;

	if (ddata && ddata->shutdown)
		ddata->shutdown(pdev);
}

static const struct of_device_id xb_snd_of_match[] = {
	{ .compatible = JZ_AIC_COMPATIBLE },
	{ }
};
MODULE_DEVICE_TABLE(of, xb_snd_of_match);

static struct platform_driver xb_snd_driver = {
	.probe = xb_snd_probe,
	.remove = xb_snd_remove,
	.shutdown = xb_snd_shutdown,
	.driver = {
		.name = "jz-aic",
		.of_match_table = xb_snd_of_match,
	},
};

#include "jz_proc.h"

static int __init xb_snd_init(void)
{
	return platform_driver_register(&xb_snd_driver);
}

static void __exit xb_snd_exit(void)
{
	platform_driver_unregister(&xb_snd_driver);
	jz_proc_cleanup();
}

module_init(xb_snd_init);
module_exit(xb_snd_exit);

MODULE_AUTHOR("Niky <xianghui.shen@ingenic.com>");
MODULE_DESCRIPTION("Ingenic T10/T20/T21/T30 OSS2 audio driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0");
