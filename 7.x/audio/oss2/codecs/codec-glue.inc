/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Codec registration on 7.4: the codec is a DT node with a platform driver,
 * which the AIC probe registers (jz_codec_init) before it looks the codec up.
 * Included at the end of each per-SoC jz_t10_codec.c.
 */
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>

#define T10_INTERNAL_CODEC_CLOCK 12000000

static struct platform_driver jz_codec_driver;

static int jz_codec_probe(struct platform_device *pdev)
{
	struct codec_info *codec_dev;
	struct gpio_desc *desc;
	int gpio = -1, level = spk_level;

	if (codec_ope)
		return -EBUSY;

	codec_ope = devm_kzalloc(&pdev->dev, sizeof(*codec_ope), GFP_KERNEL);
	codec_dev = devm_kzalloc(&pdev->dev, sizeof(*codec_dev), GFP_KERNEL);
	if (!codec_ope || !codec_dev) {
		codec_ope = NULL;
		return -ENOMEM;
	}

	codec_ope->iomem = devm_platform_get_and_ioremap_resource(pdev, 0, &codec_ope->res);
	if (IS_ERR(codec_ope->iomem)) {
		int ret = PTR_ERR(codec_ope->iomem);

		codec_ope = NULL;
		return dev_err_probe(&pdev->dev, ret, "codec registers\n");
	}
	codec_ope->dev = &pdev->dev;

	/* A DT line wins over the legacy module parameters */
	desc = devm_gpiod_get_optional(&pdev->dev, "spk", GPIOD_OUT_LOW);
	if (IS_ERR(desc)) {
		int ret = PTR_ERR(desc);

		codec_ope = NULL;
		return dev_err_probe(&pdev->dev, ret, "spk-gpios\n");
	}
	if (desc) {
		gpio = desc_to_gpio(desc);
		level = gpiod_is_active_low(desc) ? 0 : 1;
		dev_info(&pdev->dev, "speaker enable from spk-gpios (gpio %d, active %s)\n",
			 gpio, level ? "high" : "low");
	} else if (spk_gpio >= 0) {
		if (devm_gpio_request_one(&pdev->dev, spk_gpio,
					  spk_level ? GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH,
					  "spk_en") == 0) {
			gpio = spk_gpio;
			dev_info(&pdev->dev, "speaker enable from spk_gpio=%d spk_level=%d\n",
				 spk_gpio, spk_level);
		} else {
			dev_warn(&pdev->dev, "spk_gpio=%d not available\n", spk_gpio);
		}
	}
	codec_platform_data->gpio_spk_en.gpio = gpio;
	codec_platform_data->gpio_spk_en.active_level = level;

	sprintf(codec_dev->name, "internal_codec");
	codec_dev->codec_ctl_2 = codec_codec_ctl;
	codec_dev->codec_clk = T10_INTERNAL_CODEC_CLOCK;
	codec_dev->codec_mode = CODEC_MASTER;
	codec_ope->priv = codec_dev;

	pdev->dev.platform_data = codec_platform_data;
	platform_set_drvdata(pdev, codec_ope);
	i2s_register_codec_2(codec_dev);
	return 0;
}

static void jz_codec_remove(struct platform_device *pdev)
{
	if (!codec_ope)
		return;
	if (codec_platform_data->gpio_spk_en.gpio > 0)
		gpio_direction_output(codec_platform_data->gpio_spk_en.gpio,
				      !codec_platform_data->gpio_spk_en.active_level);
	i2s_release_codec_2(codec_ope->priv);
	codec_ope = NULL;
}

static const struct of_device_id jz_codec_of_match[] = {
	{ .compatible = JZ_CODEC_COMPATIBLE },
	{ }
};

static struct platform_driver jz_codec_driver = {
	.probe = jz_codec_probe,
	.remove = jz_codec_remove,
	.driver = {
		.name = "jz-codec",
		.of_match_table = jz_codec_of_match,
	},
};

int jz_codec_init(void)
{
	return platform_driver_register(&jz_codec_driver);
}

void jz_codec_exit(void)
{
	platform_driver_unregister(&jz_codec_driver);
}
