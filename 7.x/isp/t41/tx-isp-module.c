/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include "tx-isp-abi.h"
#include "tx-isp-twin.h"

extern int tx_isp_init(void);
extern void tx_isp_exit(void);

static int __init tx_isp_module_init(void)
{
	int ret = tx_isp_abi_init();

	if (ret)
		return ret;
	ret = tx_isp_init();
	if (ret) {
		tx_isp_abi_exit();
		return ret;
	}
	tx_isp_sinfo_init();
	return 0;
}

static void __exit tx_isp_module_exit(void)
{
	tx_isp_sinfo_exit();
	tx_isp_exit();
	tx_isp_abi_exit();
}

module_init(tx_isp_module_init);
module_exit(tx_isp_module_exit);

MODULE_AUTHOR("Ingenic xhshen");
MODULE_DESCRIPTION("tx isp driver");
MODULE_LICENSE("GPL");
