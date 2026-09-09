/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

extern int tx_isp_init(void);
extern void tx_isp_exit(void);
extern int tx_isp_sinfo_init(void);
extern void tx_isp_sinfo_exit(void);

static int __init tx_isp_module_init(void)
{
	int ret = tx_isp_init();
	if (!ret)
		tx_isp_sinfo_init();
	return ret;
}

static void __exit tx_isp_module_exit(void)
{
	tx_isp_sinfo_exit();
	tx_isp_exit();
}

module_init(tx_isp_module_init);
module_exit(tx_isp_module_exit);

MODULE_AUTHOR("Ingenic xhshen");
MODULE_DESCRIPTION("tx isp driver");
MODULE_LICENSE("GPL");
