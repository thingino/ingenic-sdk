#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include "include/audio_debug.h"
#include "include/jz_proc.h"

/*
 * Debug entries live in /proc/jz_audio, not /proc/jz: the ISP module owns
 * /proc/jz and removes it with everything below it when it unloads.
 */
static struct proc_dir_entry *proc_audio_root;

struct proc_dir_entry *jz_proc_mkdir(char *s)
{
	if (!proc_audio_root)
		proc_audio_root = proc_mkdir("jz_audio", NULL);
	if (!proc_audio_root)
		return NULL;
	return proc_mkdir(s, proc_audio_root);
}

void jz_proc_cleanup(void)
{
	proc_remove(proc_audio_root);
	proc_audio_root = NULL;
}

/* -------------------debugfs interface------------------- */
static int print_level = AUDIO_WARNING_LEVEL;
module_param(print_level, int, S_IRUGO);
MODULE_PARM_DESC(print_level, "private print level");

int pr_printf(unsigned int level, unsigned char *fmt, ...)
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
		if(level >= AUDIO_ERROR_LEVEL)
			dump_stack();
	}
	return r;
}
//EXPORT_SYMBOL(pr_printf);

void *pr_kmalloc(unsigned long size)
{
	void *addr = kmalloc(size, GFP_KERNEL);
	return addr;
}

void *pr_kzalloc(unsigned long size)
{
	void *addr = kzalloc(size, GFP_KERNEL);
	return addr;
}

void pr_kfree(const void *addr)
{
	kfree(addr);
}
