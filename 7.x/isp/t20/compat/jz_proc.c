// SPDX-License-Identifier: GPL-2.0
/* /proc/jz, which the vendor kernel creates (arch/mips/xburst/core/jz_proc) */
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <jz_proc.h>

static struct proc_dir_entry *jz_root;

struct proc_dir_entry *get_jz_proc_root(void)
{
	if (!jz_root)
		jz_root = proc_mkdir("jz", NULL);
	return jz_root;
}
EXPORT_SYMBOL(get_jz_proc_root);

struct proc_dir_entry *jz_proc_mkdir(char *s)
{
	struct proc_dir_entry *root = get_jz_proc_root();

	return root ? proc_mkdir(s, root) : NULL;
}
EXPORT_SYMBOL(jz_proc_mkdir);

/* the vendor kernel never unloads /proc/jz; a module has to take it down */
void jz_proc_cleanup(void)
{
	proc_remove(jz_root);
	jz_root = NULL;
}
