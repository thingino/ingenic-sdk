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

static int print_level = ISP_WARNING_LEVEL;
module_param(print_level, int, 0444);
MODULE_PARM_DESC(print_level, "isp print level");

static char *clk_name = "mpll";
module_param(clk_name, charp, 0444);
MODULE_PARM_DESC(clk_name, "chose parent clk");

static char *clks_name = "mpll";
module_param(clks_name, charp, 0444);
MODULE_PARM_DESC(clks_name, "chose scaler parent clk");

static char *clka_name = "sclka";
module_param(clka_name, charp, 0444);
MODULE_PARM_DESC(clka_name, "chose axi parent clk");

static char *clkv_name = "sclka";
module_param(clkv_name, charp, 0444);
MODULE_PARM_DESC(clkv_name, "chose axi parent clk");

static char *clkl_name = "mpll";
module_param(clkl_name, charp, 0444);
MODULE_PARM_DESC(clkl_name, "chose ldc parent clk");

static int isp_clk = 219000000;
module_param(isp_clk, int, 0444);
MODULE_PARM_DESC(isp_clk, "isp clock freq");

static int isp_clks = 476000000;
module_param(isp_clks, int, 0444);
MODULE_PARM_DESC(isp_clks, "scaler clock freq");

static int isp_clka = 500000000;
module_param(isp_clka, int, 0444);
MODULE_PARM_DESC(isp_clka, "axi clock freq");

static int isp_clkv = 500000000;
module_param(isp_clkv, int, 0444);
MODULE_PARM_DESC(isp_clkv, "vpu clock freq");

static int isp_clkl = 588000000;
module_param(isp_clkl, int, 0444);
MODULE_PARM_DESC(isp_clkl, "ldc clock freq");

static int isp_memopt = 5;
module_param(isp_memopt, int, 0444);
MODULE_PARM_DESC(isp_memopt, "isp memory optimize");

extern int isp_ch0_pre_dequeue_time;
module_param(isp_ch0_pre_dequeue_time, int, 0444);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_time, "isp pre dequeue time, unit ms");

extern int direct_mode;
module_param(direct_mode, int, 0444);
MODULE_PARM_DESC(direct_mode, "isp direct mode");

extern int ivdc_mem_line;
module_param(ivdc_mem_line, int, 0444);
MODULE_PARM_DESC(ivdc_mem_line, "ivdc mem line");

extern int ivdc_mem_stride;
module_param(ivdc_mem_stride, int, 0444);
MODULE_PARM_DESC(ivdc_mem_stride, "ivdc mem stride");

extern int ivdc_threshold_line;
module_param(ivdc_threshold_line, int, 0444);
MODULE_PARM_DESC(ivdc_threshold_line, "ivdc threshold line");

extern int zeratul_open;
module_param(zeratul_open, int, 0444);
MODULE_PARM_DESC(zeratul_open, "zeratul open");

extern int atlas_open;
module_param(atlas_open, int, 0444);
MODULE_PARM_DESC(atlas_open, "atlas open");

static int isp_cache_line;
module_param(isp_cache_line, int, 0444);
MODULE_PARM_DESC(isp_cache_line, "isp cache line");

static int streamoff_mode;
module_param(streamoff_mode, int, 0444);
MODULE_PARM_DESC(streamoff_mode, "sensor streamoff mode");

struct module *isp_module = THIS_MODULE;

/* the clock parents the firmware matches the sclk names against */
char *sclk_name[3] = { "sclka", "mpll", "vpll" };

int isp_printf(unsigned int level, unsigned char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r = 0;

	if (level >= print_level) {
		va_start(args, fmt);
		vaf.fmt = fmt;
		vaf.va = &args;
		r = printk("%pV", &vaf);
		va_end(args);
		if (level >= ISP_ERROR_LEVEL)
			dump_stack();
	}
	return r;
}
EXPORT_SYMBOL(isp_printf);

char *get_clk_name(void) { return clk_name; }
char *get_clks_name(void) { return clks_name; }
char *get_clka_name(void) { return clka_name; }
char *get_clkv_name(void) { return clkv_name; }
char *get_clkl_name(void) { return clkl_name; }
int get_isp_clk(void) { return isp_clk; }
int get_isp_clks(void) { return isp_clks; }
int get_isp_clka(void) { return isp_clka; }
int get_isp_clkv(void) { return isp_clkv; }
int get_isp_clkl(void) { return isp_clkl; }
int get_isp_memopt(void) { return isp_memopt; }
int get_isp_cache_line(void) { return isp_cache_line; }
int get_isp_streamoff_mode(void) { return streamoff_mode; }
