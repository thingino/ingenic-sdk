// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-funcs.c - module parameters and the firmware's tunable getters
 *
 * The firmware reads its clock names and rates, memory options and ISP print
 * level through the get_* functions below (it carries weak defaults), and
 * keeps the variables behind a few of the parameters itself.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/stdarg.h>

#include <txx-funcs.h>

/* -------------------debugfs interface------------------- */
static int print_level = ISP_WARNING_LEVEL;
module_param(print_level, int, 0444);
MODULE_PARM_DESC(print_level, "isp print level");

static char *clk_name = "mpll";
module_param(clk_name, charp, 0444);
MODULE_PARM_DESC(clk_name, "select the isp parent clock");

static char *clks_name = "mpll";
module_param(clks_name, charp, 0444);
MODULE_PARM_DESC(clks_name, "select the scaler parent clock");

static char *clka_name = "mpll";
module_param(clka_name, charp, 0444);
MODULE_PARM_DESC(clka_name, "select the axi bus parent clock");

static int isp_clk = 285000000;
module_param(isp_clk, int, 0444);
MODULE_PARM_DESC(isp_clk, "isp clock freq");

static int isp_clks = 550000000;
module_param(isp_clks, int, 0444);
MODULE_PARM_DESC(isp_clks, "isp scaler clock freq");

static int isp_clka = 600000000;
module_param(isp_clka, int, 0444);
MODULE_PARM_DESC(isp_clka, "isp axi bus clock freq");

/* input format:0xff */
/* main:[0-3]bit sec:[4-7]bit */
/* 0:nomal 1:bypass Lynne+BGM 2:bypass Lynne+BGM+Ass 3-f:nomal */
static int isp_memopt = 1;
module_param(isp_memopt, int, 0444);
MODULE_PARM_DESC(isp_memopt, "isp memory optimize");

extern int isp_ch0_pre_dequeue_time;
module_param(isp_ch0_pre_dequeue_time, int, 0444);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_time, "isp pre dequeue time, unit ms");

int direct_mode = 0;
module_param(direct_mode, int, 0444);
MODULE_PARM_DESC(direct_mode, "isp direct mode");

int ivdc_mem_line = 0;
module_param(ivdc_mem_line, int, 0444);
MODULE_PARM_DESC(ivdc_mem_line, "ivdc mem line");

int ivdc_threshold_line = 0;
module_param(ivdc_threshold_line, int, 0444);
MODULE_PARM_DESC(ivdc_threshold_line, "ivdc threshold line");

extern uint16_t tx_isp_core_ioctl_mask[11];
module_param_array(tx_isp_core_ioctl_mask, short, NULL, 0644);
MODULE_PARM_DESC(tx_isp_core_ioctl_mask, "isp ioctl calls mask");

char *sclk_name[3] = {"mpll", "vpll", "sclka"};

int isp_printf(unsigned int level, unsigned char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r = 0;

	if(level >= print_level){
		va_start(args, fmt);

		vaf.fmt = fmt;
		vaf.va = &args;

		r = printk("%pV",&vaf);
		va_end(args);
		if(level >= ISP_ERROR_LEVEL)
			dump_stack();
	}
	return r;
}
EXPORT_SYMBOL(isp_printf);

char *get_clk_name(void)
{
	return clk_name;
}

char *get_clks_name(void)
{
	return clks_name;
}

char *get_clka_name(void)
{
	return clka_name;
}

int get_isp_clk(void)
{
	return isp_clk;
}

int get_isp_clks(void)
{
	return isp_clks;
}

int get_isp_clka(void)
{
	return isp_clka;
}

int get_isp_memopt(void)
{
	return isp_memopt;
}
