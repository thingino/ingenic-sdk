// SPDX-License-Identifier: GPL-2.0
/*
 * The vendor kernels' SADC character devices on the mainline SADC driver.
 *
 * Vendor userspace (the SDK's sample-adc, photoresistor day/night) opens
 * /dev/jz_adc_aux_N (T10 to T31, 3.10) or /dev/ingenic_adc_aux_N (T32 and
 * later, 4.4), reads an int and drives it with three ioctls. The channels
 * belong to the kernel's ingenic-sadc IIO driver; this module reads them
 * through the IIO in-kernel API and keeps the vendor arithmetic: the value
 * read is the 12-bit sample times the reference in mV over 1024 (3.10) or
 * 4096 (4.4), the reference the SADC driver's (the board's vref supply, or
 * what the vendor boards wire) until ADC_SET_VREF sets another.
 * ADC_ENABLE and ADC_DISABLE start and stop nothing here: a read converts.
 *
 * Where the SADC driver has a temperature channel (T32, T33), /dev/temp is
 * the vendor jz_temp device: TSENSOR_GET_TEMP writes the temperature in
 * millidegrees Celsius to the int its argument points at.
 */
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define ADC_ENABLE	0
#define ADC_DISABLE	1
#define ADC_SET_VREF	2

#ifdef ADC_AUX_44
#define ADC_AUX_NAME	"ingenic_adc_aux_%u"
#define ADC_AUX_DIV	4096
#else
#define ADC_AUX_NAME	"jz_adc_aux_%u"
#define ADC_AUX_DIV	1024
#endif

#define ADC_AUX_MAX	8

#define TSENSOR_GET_TEMP	_IOWR('T', 1, int)

struct adc_aux {
	struct miscdevice misc;
	struct iio_channel chan;
	char name[24];
};

static struct device *sadc_iio;
static struct adc_aux *auxs;
static unsigned int naux;
static unsigned int vref_mv = 1800;
static struct iio_channel temp_chan;
static bool temp_registered;

static const struct of_device_id sadc_of_match[] = {
	{ .compatible = "ingenic,t20-sadc" },
	{ .compatible = "ingenic,t21-sadc" },
	{ .compatible = "ingenic,t23-sadc" },
	{ .compatible = "ingenic,t30-sadc" },
	{ .compatible = "ingenic,t31-sadc" },
	{ .compatible = "ingenic,t32-sadc" },
	{ .compatible = "ingenic,t33-sadc" },
	{ .compatible = "ingenic,t40-sadc" },
	{ }
};

static ssize_t adc_aux_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
	struct adc_aux *aux = container_of(file->private_data, struct adc_aux, misc);
	int raw, ret, val;

	if (len < sizeof(val))
		return -EINVAL;
	ret = iio_read_channel_raw(&aux->chan, &raw);
	if (ret < 0)
		return ret;
	val = raw * READ_ONCE(vref_mv) / ADC_AUX_DIV;
	if (copy_to_user(buf, &val, sizeof(val)))
		return -EFAULT;
	return sizeof(val);
}

static long adc_aux_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int v;

	switch (cmd) {
	case ADC_ENABLE:
	case ADC_DISABLE:
		return 0;
	case ADC_SET_VREF:
		if (get_user(v, (unsigned int __user *)arg))
			return -EFAULT;
		WRITE_ONCE(vref_mv, v);
		return 0;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations adc_aux_fops = {
	.owner = THIS_MODULE,
	.read = adc_aux_read,
	.unlocked_ioctl = adc_aux_ioctl,
};

static long temp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret, val;

	if (cmd != TSENSOR_GET_TEMP)
		return -ENOTTY;
	ret = iio_read_channel_processed(&temp_chan, &val);
	if (ret < 0)
		return ret;
	if (copy_to_user((int __user *)arg, &val, sizeof(val)))
		return -EFAULT;
	return 0;
}

static const struct file_operations temp_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = temp_ioctl,
};

static struct miscdevice temp_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "temp",
	.fops = &temp_fops,
};

static int sadc_iio_match(struct device *dev, const void *data)
{
	/* triggers sit on the IIO bus too */
	return dev->bus == &iio_bus_type && strstarts(dev_name(dev), "iio:device");
}

static void adc_aux_put(void)
{
	if (temp_registered)
		misc_deregister(&temp_misc);
	temp_registered = false;
	while (naux)
		misc_deregister(&auxs[--naux].misc);
	kfree(auxs);
	auxs = NULL;
	put_device(sadc_iio);
}

static int __init adc_aux_init(void)
{
	struct platform_device *pdev;
	struct iio_dev *indio_dev;
	struct device_node *np;
	unsigned int i;
	int ret;

	np = of_find_matching_node(NULL, sadc_of_match);
	if (!np)
		return -ENODEV;
	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev)
		return -ENODEV;
	/* the IIO device the SADC driver registered under its platform device */
	sadc_iio = device_find_child(&pdev->dev, NULL, sadc_iio_match);
	put_device(&pdev->dev);
	if (!sadc_iio)
		return -ENODEV;
	indio_dev = dev_to_iio_dev(sadc_iio);

	if (indio_dev->num_channels) {
		struct iio_channel c = { indio_dev, &indio_dev->channels[0], NULL };
		int v, v2;

		/* IIO_VAL_FRACTIONAL_LOG2: the reference in mV over 2^bits */
		if (iio_read_channel_scale(&c, &v, &v2) == IIO_VAL_FRACTIONAL_LOG2)
			vref_mv = v;
	}

	auxs = kcalloc(ADC_AUX_MAX, sizeof(*auxs), GFP_KERNEL);
	if (!auxs) {
		put_device(sadc_iio);
		return -ENOMEM;
	}
	for (i = 0; i < indio_dev->num_channels; i++) {
		const struct iio_chan_spec *spec = &indio_dev->channels[i];
		struct adc_aux *aux = &auxs[naux];

		if (spec->type == IIO_TEMP && !temp_registered) {
			temp_chan.indio_dev = indio_dev;
			temp_chan.channel = spec;
			ret = misc_register(&temp_misc);
			if (ret) {
				adc_aux_put();
				return ret;
			}
			temp_registered = true;
			continue;
		}
		if (spec->type != IIO_VOLTAGE || naux == ADC_AUX_MAX)
			continue;
		aux->chan.indio_dev = indio_dev;
		aux->chan.channel = spec;
		snprintf(aux->name, sizeof(aux->name), ADC_AUX_NAME, spec->channel);
		aux->misc.minor = MISC_DYNAMIC_MINOR;
		aux->misc.name = aux->name;
		aux->misc.fops = &adc_aux_fops;
		ret = misc_register(&aux->misc);
		if (ret) {
			adc_aux_put();
			return ret;
		}
		naux++;
	}
	pr_info("adc-aux: %u channels of %s, vref %u mV%s\n", naux, dev_name(sadc_iio), vref_mv,
		temp_registered ? ", /dev/temp" : "");
	return 0;
}

static void __exit adc_aux_exit(void)
{
	adc_aux_put();
}

module_init(adc_aux_init);
module_exit(adc_aux_exit);

MODULE_IMPORT_NS("IIO_CONSUMER");
MODULE_SOFTDEP("pre: ingenic-sadc");
MODULE_DESCRIPTION("Vendor jz_adc_aux / ingenic_adc_aux devices on the mainline SADC driver");
MODULE_LICENSE("GPL");
