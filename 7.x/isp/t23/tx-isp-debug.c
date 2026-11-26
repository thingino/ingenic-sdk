#include <linux/debugfs.h>
#include <linux/module.h>
#include <tx-isp-debug.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/timekeeping.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include "tx-isp-abi.h"

/* -------------------debugfs interface------------------- */
static int print_level = ISP_WARNING_LEVEL;
module_param(print_level, int, S_IRUGO);
MODULE_PARM_DESC(print_level, "isp print level");

static char *clk_name = "mpll";
module_param(clk_name, charp, S_IRUGO);
MODULE_PARM_DESC(clk_name, "select the isp parent clock");

static char *clka_name = "mpll";
module_param(clka_name, charp, S_IRUGO);
MODULE_PARM_DESC(clka_name, "select the axi bus parent clock");

static int isp_clk = 153000000;
module_param(isp_clk, int, S_IRUGO);
MODULE_PARM_DESC(isp_clk, "isp clock freq");

static int isp_clka = 416000000;
module_param(isp_clka, int, S_IRUGO);
MODULE_PARM_DESC(isp_clka, "isp aclock freq");

extern int isp_ch0_pre_dequeue_time;
module_param(isp_ch0_pre_dequeue_time, int, S_IRUGO);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_time, "isp pre dequeue time, unit ms");

extern int isp_ch0_pre_dequeue_interrupt_process;
module_param(isp_ch0_pre_dequeue_interrupt_process, int, S_IRUGO);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_interrupt_process, "isp pre dequeue interrupt process");

extern int isp_ch0_pre_dequeue_valid_lines;
module_param(isp_ch0_pre_dequeue_valid_lines, int, S_IRUGO);
MODULE_PARM_DESC(isp_ch0_pre_dequeue_valid_lines, "isp pre dequeue valid lines");

extern int isp_ch1_dequeue_delay_time;
module_param(isp_ch1_dequeue_delay_time, int, S_IRUGO);
MODULE_PARM_DESC(isp_ch1_dequeue_delay_time, "isp pre dequeue time, unit ms");

extern int isp_day_night_switch_drop_frame_num;
module_param(isp_day_night_switch_drop_frame_num, int, S_IRUGO);
MODULE_PARM_DESC(isp_day_night_switch_drop_frame_num, "isp day night switch drop frame number");

extern int isp_memopt;
module_param(isp_memopt, int, S_IRUGO);
MODULE_PARM_DESC(isp_memopt, "isp memory optimize");

extern int direct_mode;
module_param(direct_mode, int, S_IRUGO);
MODULE_PARM_DESC(direct_mode, "isp direct mode");

extern int ivdc_mem_line;
module_param(ivdc_mem_line, int, S_IRUGO);
MODULE_PARM_DESC(ivdc_mem_line, "ivdc mem line");

extern int ivdc_threshold_line;
module_param(ivdc_threshold_line, int, S_IRUGO);
MODULE_PARM_DESC(ivdc_threshold_line, "ivdc threshold line");

#ifdef CONFIG_MULTI_SENSOR
/*
 * The multi-sensor firmware hands this pin to the VIC, which flips the MIPI
 * switch between the sensors itself; it supports PA7, PB22 and PB30 (7, 54,
 * 62). The firmware's own get_mipi_switch_gpio() is a weak stub returning 0.
 */
static int mipi_switch_gpio = 7;
module_param(mipi_switch_gpio, int, S_IRUGO);
MODULE_PARM_DESC(mipi_switch_gpio, "select mipi switch gpio");

int get_mipi_switch_gpio(void)
{
	return mipi_switch_gpio;
}
#endif

char *sclk_name[2] = {"mpll", "sclka"};

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
	}
	return r;
}
EXPORT_SYMBOL(isp_printf);

int get_isp_clk(void)
{
	return isp_clk;
}

int get_isp_clka(void)
{
	return isp_clka;
}

char *get_clk_name(void)
{
	return clk_name;
}

char *get_clka_name(void)
{
	return clka_name;
}

void *private_vmalloc(unsigned long size)
{
	void *addr = vmalloc(size);
	if (!addr)
		pr_err("%s: vmalloc(%lu) failed\n", __func__, size);
	return addr;
}

void private_vfree(const void *addr)
{
	vfree(addr);
}

ktime_t private_ktime_set(const long secs, const unsigned long nsecs)
{
	return ktime_set(secs, nsecs);
}

void private_set_current_state(unsigned int state)
{
	__set_current_state(state);
	return;
}


int private_schedule_hrtimeout(ktime_t *ex, const enum hrtimer_mode mode)
{
	return schedule_hrtimeout(ex, mode);
}

#define WORK_DATA_310_STATIC	0xffffffe0u

/*
 * The firmware's static work_structs carry 3.10's WORK_STRUCT_NO_POOL in
 * .data; 7.x would read that as a pool id. Rebuild them on first use,
 * keeping the function the firmware put there.
 */
bool private_schedule_work(struct work_struct *work)
{
	if ((u32)work->data.counter == WORK_DATA_310_STATIC) {
		work_func_t fn = work->func;

		INIT_WORK(work, fn);
	}
	return schedule_work(work);
}


void private_do_gettimeofday(struct timeval *tv)
{
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
}


/* Must be check the return value */
__must_check int private_get_driver_interface(struct jz_driver_common_interfaces **pfaces)
{
	if(pfaces == NULL)
		return -1;
	*pfaces = get_driver_common_interfaces();
	if(*pfaces && ((*pfaces)->flags_0 != (unsigned int)_printk || (*pfaces)->flags_0 !=(*pfaces)->flags_1)){
		ISP_ERROR("flags = 0x%08x, jzflags = %p,0x%08x", (*pfaces)->flags_0, _printk, (*pfaces)->flags_1);
		return -1;
	}else
		return 0;
}
