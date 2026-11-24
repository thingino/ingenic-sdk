// SPDX-License-Identifier: GPL-2.0
/*
 * The Apical blob imports usleep_range(), which 7.x only has as an inline
 * over usleep_range_state(); give it a real symbol of that name.
 */
#include <linux/delay.h>
#include <linux/sched.h>

void tx_isp_usleep_range(unsigned long min, unsigned long max) __asm__("usleep_range");

void tx_isp_usleep_range(unsigned long min, unsigned long max)
{
	usleep_range_state(min, max, TASK_UNINTERRUPTIBLE);
}
