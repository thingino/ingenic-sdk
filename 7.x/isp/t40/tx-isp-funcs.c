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
MODULE_PARM_DESC(clk_name, "chose parent clk");

static int isp_clk = 200000000;
module_param(isp_clk, int, 0444);
MODULE_PARM_DESC(isp_clk, "isp clock freq");

/* input format:0xff */
/* main:[0-3]bit sec:[4-7]bit */
/* 0:nomal 1:bypass Lynne+BGM 2:bypass Lynne+BGM+Ass 3-f:nomal */
static int isp_memopt;
module_param(isp_memopt, int, 0444);
MODULE_PARM_DESC(isp_memopt, "isp memory optimize");

extern int isp_ch0_pre_dequeue_time;
module_param(isp_ch0_pre_dequeue_time, int, 0444);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_time, "isp pre dequeue time, unit ms");

extern uint16_t isp_dual_buf[2];
module_param_array(isp_dual_buf, ushort, NULL, 0444);
MODULE_PARM_DESC(isp_dual_buf, "isp dual buff");

char *sclk_name[4] = { "mpll", "vpll", "epll", "sclka" };

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
int get_isp_clk(void) { return isp_clk; }
int get_isp_memopt(void) { return isp_memopt; }
