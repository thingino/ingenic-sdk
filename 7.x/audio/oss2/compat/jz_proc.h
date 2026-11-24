/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _JZ_PROC_H_
#define _JZ_PROC_H_

#include <linux/proc_fs.h>

/* /proc/jz_audio/<name>: the ISP module owns /proc/jz and removes it on unload */
struct proc_dir_entry *jz_proc_mkdir(char *s);
void jz_proc_cleanup(void);

#endif
