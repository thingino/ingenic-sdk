/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _JZ_PROC_H_
#define _JZ_PROC_H_
struct proc_dir_entry *jz_proc_mkdir(char *s);
struct proc_dir_entry *get_jz_proc_root(void);
void jz_proc_cleanup(void);
#endif
