/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/proc_fs.h>

#include "lib_base.h"
#include "jz_proc.h"
#include "tx-isp-7x.h"

extern int tx_isp_init(void);
extern void tx_isp_exit(void);

/* the vendor kernels' /proc/jz, which the ISP's entries live under */
static struct proc_dir_entry *jz_proc_root;

struct proc_dir_entry *jz_proc_mkdir(char *s)
{
	return jz_proc_root ? proc_mkdir(s, jz_proc_root) : NULL;
}

static int __init tx_isp_module_init(void)
{
	int ret;

	ret = tx_isp_7x_init();
	if (ret)
		return ret;
	jz_proc_root = proc_mkdir("jz", NULL);
	ret = tx_isp_init();
	if (ret) {
		proc_remove(jz_proc_root);
		tx_isp_7x_exit();
	}
	return ret;
}

static void __exit tx_isp_module_exit(void)
{
	tx_isp_exit();
	proc_remove(jz_proc_root);
	tx_isp_7x_exit();
}

module_init(tx_isp_module_init);
module_exit(tx_isp_module_exit);

MODULE_AUTHOR("Ingenic xhshen");
MODULE_DESCRIPTION("tx isp driver");
MODULE_LICENSE("GPL");
