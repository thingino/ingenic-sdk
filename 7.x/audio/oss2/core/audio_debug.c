#include <linux/module.h>
#include <linux/proc_fs.h>
#include "jz_proc.h"

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
