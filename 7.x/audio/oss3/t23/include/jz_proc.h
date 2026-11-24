#ifndef _JZ_PROC_H_
#define _JZ_PROC_H_

#include <linux/proc_fs.h>

/*
 * Create /proc/jz_audio/<name> for module debug entries.
 */
struct proc_dir_entry *jz_proc_mkdir(char *s);
void jz_proc_cleanup(void);

#endif
