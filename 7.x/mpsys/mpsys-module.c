// SPDX-License-Identifier: GPL-2.0
/*
 * mpsys_driver.ko: the vendor multi-process system object (libimp's
 * /dev/mpsys_data frame and message sharing between processes; on the
 * T40/T41 also /dev/nna_lock, nmem= allocation and a global lock) on
 * Linux 7.x.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "mpsys-abi.h"

static bool have_utils;

static int __init mpsys_init(void)
{
	int ret;

	ret = mpsys_abi_init();
	if (ret)
		return ret;

#ifndef MPSYS_FW_515
	/*
	 * The vendor shim failed the whole module without nmem= on the
	 * command line; libimp only needs mpsys_data, so carry on without
	 * mpsys_utils.
	 */
	have_utils = !mpsys_utils_init();
	if (!have_utils)
		pr_warn("mpsys: no nmem= region, /dev/nna_lock not created\n");
#endif

	ret = mpsys_data_init();
	if (ret) {
		if (have_utils)
			mpsys_utils_exit();
		mpsys_abi_exit();
		return ret;
	}
	return 0;
}

static void __exit mpsys_exit(void)
{
	mpsys_data_exit();
	if (have_utils)
		mpsys_utils_exit();
	mpsys_abi_exit();
}

module_init(mpsys_init);
module_exit(mpsys_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ingenic multi-process system (vendor object + 7.x adapter)");
