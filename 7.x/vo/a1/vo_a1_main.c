// SPDX-License-Identifier: GPL-2.0
/*
 * A1 video output (/dev/vde: VDE layers, DSC timing, HDMI and its PHY, VGA,
 * TFT) on mainline 7.x: the vendor SDK 1.7.0 libvde-firmware (built for the
 * vendor 4.4.94 kernel, unmodified) plus this adapter. See a1abi.h.
 */
#include <linux/i2c.h>
#include <linux/module.h>

#include "../../a1abi/a1abi.h"

static int print_level = 1;
module_param(print_level, int, 0444);
MODULE_PARM_DESC(print_level, "vde print level");

static int vdea_clk = 700000000;
module_param(vdea_clk, int, 0444);
MODULE_PARM_DESC(vdea_clk, "vde bus clock freq");

static int vga_rgb_order = 0x102;
module_param(vga_rgb_order, int, 0444);
MODULE_PARM_DESC(vga_rgb_order, "vga rgb pins order");

static int hdmi_hpd_pin = 1 * 32 + 31;
module_param(hdmi_hpd_pin, int, 0444);
MODULE_PARM_DESC(hdmi_hpd_pin, "hdmi hot plug pin");

static int tft_fmt;
module_param(tft_fmt, int, 0444);
MODULE_PARM_DESC(tft_fmt, "tft output format");

static int backlight_pin = 2 * 32 + 18;
module_param(backlight_pin, int, 0444);
MODULE_PARM_DESC(backlight_pin, "backlight pin");

static int backlight_status = 1;
module_param(backlight_status, int, 0444);
MODULE_PARM_DESC(backlight_status, "backlight status");

static char ddc_gpio_func[32] = "1,1,0x60000000";
module_param_string(ddc_gpio_func, ddc_gpio_func, sizeof(ddc_gpio_func), 0444);
MODULE_PARM_DESC(ddc_gpio_func, "HDMI DDC GPIO group, function and pins");

/* the firmware's entry points */
extern int ingenic_vde_init(void);
extern void ingenic_vde_exit(void);

/* the firmware's exports for the fb module, native again */
extern void ingenic_vde_set_alpha(void);
extern void ingenic_vde_set_colorkey(void);
extern void ingenic_vde_get_display_size(void);
extern void ingenic_vde_mouse_pan_display(void);
extern void ingenic_vde_mouse_set_position(void);
extern void ingenic_vde_mouse_layer_disable(void);
extern void ingenic_vde_mouse_layer_enable(void);
extern void ingenic_vde_graphic_refresh(void);
extern void ingenic_vde_graphic_layer_disable(void);
extern void ingenic_vde_graphic_layer_enable(void);
EXPORT_SYMBOL(ingenic_vde_set_alpha);
EXPORT_SYMBOL(ingenic_vde_set_colorkey);
EXPORT_SYMBOL(ingenic_vde_get_display_size);
EXPORT_SYMBOL(ingenic_vde_mouse_pan_display);
EXPORT_SYMBOL(ingenic_vde_mouse_set_position);
EXPORT_SYMBOL(ingenic_vde_mouse_layer_disable);
EXPORT_SYMBOL(ingenic_vde_mouse_layer_enable);
EXPORT_SYMBOL(ingenic_vde_graphic_refresh);
EXPORT_SYMBOL(ingenic_vde_graphic_layer_disable);
EXPORT_SYMBOL(ingenic_vde_graphic_layer_enable);

/* called by the firmware, which declares them itself */
int vde_printf(unsigned int level, unsigned char *fmt, ...);
int get_vdea_clk(void);
int get_vga_pin_order(void);
int get_hdmi_hpd(void);
char *get_hdmi_ddc_gpio_func(void);
int get_tft_fmt(void);
int get_backlight_pin(void);
int get_backlight_status(void);
int get_half_frame_interrupt_status(void);
void dev_err_a1(const struct device *dev, const char *fmt, ...);
int i2c_transfer_a1(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
void *private_of_match_node(const void *matches, const struct device_node *node);

int vde_printf(unsigned int level, unsigned char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r = 0;

	if (level >= print_level) {
		va_start(args, fmt);
		vaf.fmt = (const char *)fmt;
		vaf.va = &args;
		r = printk("%pV", &vaf);
		va_end(args);
		if (level >= 2)
			dump_stack();
	}
	return r;
}
EXPORT_SYMBOL(vde_printf);

int get_vdea_clk(void) { return vdea_clk; }
int get_vga_pin_order(void) { return vga_rgb_order; }
int get_hdmi_hpd(void) { return hdmi_hpd_pin; }
char *get_hdmi_ddc_gpio_func(void) { return ddc_gpio_func; }
int get_tft_fmt(void) { return tft_fmt; }
int get_backlight_pin(void) { return backlight_pin; }
int get_backlight_status(void) { return backlight_status; }

void dev_err_a1(const struct device *dev, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	dev_err(a1_real_dev((struct device *)dev), "%pV", &vaf);
	va_end(args);
}

/*
 * ---- DDC: the firmware's i2c adapter ----------------------------------------
 *
 * It is the HDMI controller's DDC master, only ever driven by the firmware
 * itself: the adapter is kept and its own master_xfer called (4.4's
 * i2c_adapter has algo at 8; i2c_algorithm starts with master_xfer).
 */
static void *a1_i2c_adap;

int private_i2c_add_adapter(void *adap44)
{
	a1_i2c_adap = adap44;
	return 0;
}

void private_i2c_del_adapter(void *adap44)
{
	a1_i2c_adap = NULL;
}

void private_i2c_set_adapdata(void *adap44, void *data)
{
	*(void **)((char *)adap44 + 40 + 292) = data;	/* dev.driver_data */
}

void *private_i2c_get_adapdata(const void *adap44)
{
	return *(void **)((const char *)adap44 + 40 + 292);
}

int i2c_transfer_a1(struct i2c_adapter *adap44, struct i2c_msg *msgs, int num)
{
	void *algo = *(void **)((char *)adap44 + 8);
	int (*xfer)(void *, struct i2c_msg *, int) = *(void **)algo;

	return xfer(adap44, msgs, num);
}

int private_i2c_transfer(void *adap44, struct i2c_msg *msgs, int num)
{
	return i2c_transfer_a1(adap44, msgs, num);
}

void *private_of_match_node(const void *matches, const struct device_node *node)
{
	return of_device_is_compatible(node, "ingenic,a1-vde") ? (void *)matches : NULL;
}

/*
 * ---- the platform driver -----------------------------------------------------
 *
 * 4.4's platform_driver: probe (0), remove (4).
 */
struct a1_drv44 {
	int (*probe)(void *);
	int (*remove)(void *);
};

static struct a1_drv44 *a1_drv;

static int a1_probe(struct platform_device *pdev)
{
	int ret;

	a1_twin = a1_twin_alloc(pdev);
	if (!a1_twin)
		return -ENOMEM;
	ret = a1_drv->probe(a1_twin);
	if (ret) {
		kfree(a1_twin);
		a1_twin = NULL;
	}
	return ret;
}

static void a1_remove(struct platform_device *pdev)
{
	if (!a1_twin)
		return;
	a1_drv->remove(a1_twin);
	kfree(a1_twin);
	a1_twin = NULL;
}

static const struct of_device_id a1_of_match[] = {
	{ .compatible = "ingenic,a1-vde" },
	{ }
};
MODULE_DEVICE_TABLE(of, a1_of_match);

static struct platform_driver a1_platform_driver = {
	.probe = a1_probe,
	.remove = a1_remove,
	.driver = {
		.name = "jz-vde",
		.of_match_table = a1_of_match,
	},
};

int private_platform_driver_register(void *drv44)
{
	a1_drv = drv44;
	return platform_driver_register(&a1_platform_driver);
}

void private_platform_driver_unregister(void *drv44)
{
	platform_driver_unregister(&a1_platform_driver);
}

static int __init vo_a1_init(void)
{
	return ingenic_vde_init();
}

static void __exit vo_a1_exit(void)
{
	ingenic_vde_exit();
	a1_timers_stop();
	a1_procs_free();
}

module_init(vo_a1_init);
module_exit(vo_a1_exit);

MODULE_DESCRIPTION("A1 video output: vendor firmware 1.7.0 and its adapter for mainline 7.x");
MODULE_LICENSE("GPL");
