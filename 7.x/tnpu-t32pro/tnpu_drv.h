/*
 * Ingenic TNPU driver
 *
 * Copyright (c) 2023 LiuTianyang
 *
 * This file is released under the GPLv2
 */
#ifndef __TNPU_DRV_H__
#define __TNPU_DRV_H__

#include "tnpu_ioctl.h"
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/version.h>

#define TNPU_DRIVER_VERSION     0x020112


// TODO: Only for testing use.
//#define CONFIG_USED_EXT_MEM  // Avoiding CMA and forcing the use of RMEM
//#define CONFIG_TNPU_DEV_DEBUG
//#define CONFIG_TNPU_MEM_DEBUG
//#define CONFIG_TNPU_CORE_DEBUG
//#define CONFIG_TNPU_SCHEDUL_DEBUG
//#define CONFIG_TNPU_PERF_FILE "/tmp/tnpu_perf.txt"

#define PRINT_TNPU_INFO(fmt, ...) printk(KERN_INFO "<TNPU> " fmt, ##__VA_ARGS__)
#define PRINT_TNPU_ERROR(fmt, ...) \
	printk(KERN_ERR "<TNPU:%s> *ERROR* " fmt, __func__, ##__VA_ARGS__)

/* tnpu_mem_bo.c */
void *tnpu_cma_alloc(size_t size, dma_addr_t *dma_handle);
void tnpu_cma_free(size_t size, void *kvaddr, dma_addr_t dma_handle);
int tnpu_bo_pool_add(unsigned int paddr, unsigned int size);
int tnpu_bo_pool_remove(void);
unsigned int tnpu_pool_avail(void);
unsigned int tnpu_pool_size(void);
int tnpu_bo_init(struct device *dev, unsigned int cma_pool_size);
void tnpu_bo_release(pid_t pid);
int tnpu_bo_mmap(struct vm_area_struct *vma);
int ioctl_tnpu_bo_append(unsigned long arg);
int ioctl_tnpu_bo_destroy(unsigned long arg);
unsigned int tnpu_pool_quota(void);
int ioctl_tnpu_pool_quota(unsigned long arg);
int ioctl_tnpu_pool_create(unsigned long arg);
void tnpu_show_allocations(void);

/* tnpu_core.c */
int tnpu_hardware_suspend(void);
int tnpu_hardware_resume(void);
void tnpu_timer_open(void);
void tnpu_timer_close(void);
unsigned long long tnpu_timer_get(void);
int tnpu_clk_open(void);
void tnpu_clk_close(void);
int tnpu_core_init(struct platform_device *pdev,
			const char *parent_clk_name,
			unsigned long tnpu_clk_rate);
void tnpu_core_remove(void);
int ioctl_tnpu_job_wait(unsigned long arg);
int ioctl_tnpu_sharelib_load(unsigned long arg);
int ioctl_tnpu_sharelib_unload(unsigned long arg);
int tnpu_sharelib_busy(void);
void tnpu_show_sharelib(void);
int ioctl_tnpu_job_submit(unsigned long arg);
int ioctl_tnpu_job_submit_chain(unsigned long arg);
void tnpu_kill_job(pid_t pid);

/* other */

#endif // __TNPU_DRV_H__
