#include <linux/fs.h>
#include <linux/module.h>
#include <linux/seq_file.h>

#ifdef CONFIG_KERNEL_5_15
#include <linux/proc_fs.h>
#endif

#include "lib_base.h"

static int tx_isp_debug_show(struct seq_file *s, void *v)
{
	seq_puts(s, "tx-isp debug: basic stub\n");
	return 0;
}

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
static int tx_isp_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tx_isp_debug_show, NULL);
}

const struct file_operations tx_isp_debug_fops = {
#ifndef CONFIG_ZERATUL
	.owner = THIS_MODULE,
#else
	.owner = NULL,
#endif
	.open = tx_isp_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#elif defined(CONFIG_KERNEL_5_15)
static int tx_isp_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tx_isp_debug_show, NULL);
}

const struct proc_ops tx_isp_debug_fops = {
	.proc_open = tx_isp_debug_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#endif
