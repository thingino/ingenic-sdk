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

// TODO: Only for testing use.
//#define CONFIG_USED_EXT_MEM    // used rmem
//#define CONFIG_TNPU_DEV_DEBUG
//#define CONFIG_TNPU_MEM_DEBUG
//#define CONFIG_TNPU_CORE_DEBUG
//#define CONFIG_TNPU_SCHEDUL_DEBUG
//#define CONFIG_TNPU_DUTY_RATIO_DEBUG

#define TNPU_DRIVER_VERSION     0x010216
#define TNPU_JOB_DELAY_DENSITY  16


/* 1. Parent Clock Name */
#ifdef CONFIG_TNPU_CLK_PARENT_APLL
#define TNPU_PARENT_NAME "apll"
#elif defined(CONFIG_TNPU_CLK_PARENT_VPLL)
#define TNPU_PARENT_NAME "vpll"
#elif defined(CONFIG_TNPU_CLK_PARENT_MPLL)
#define TNPU_PARENT_NAME "mpll"
#else
#define TNPU_PARENT_NAME "apll"
#endif

/* 2. Clock Rate (MHz -> Hz) */
#ifdef CONFIG_TNPU_CLOCK_RATE
#define TNPU_DEFAULT_CLK_RATE  (CONFIG_TNPU_CLOCK_RATE * 1000000UL)
#else
#define TNPU_DEFAULT_CLK_RATE  500000000UL
#endif

/* 3. CMA Pool (KB -> Bytes) */
#ifdef CONFIG_TNPU_CMA_POOL_SIZE
#define TNPU_DEFAULT_CMA_SIZE  (CONFIG_TNPU_CMA_POOL_SIZE * 1024UL)
#else
#define TNPU_DEFAULT_CMA_SIZE  0
#endif

#define PRINT_TNPU_INFO(fmt, ...) printk(KERN_INFO "<TNPU> " fmt, ##__VA_ARGS__)
#define PRINT_TNPU_ERROR(fmt, ...) \
    printk(KERN_ERR "<TNPU:%s> *ERROR* " fmt, __func__, ##__VA_ARGS__)


/*  tnpu_mem_bo.c */
void *tnpu_cma_alloc(size_t size, dma_addr_t *dma_handle);
void tnpu_cma_free(size_t size, void *kvaddr, dma_addr_t dma_handle);
int tnpu_bo_pool_add(unsigned int paddr, unsigned int size);
void tnpu_bo_pool_remove(void);
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
void tnpu_show_sharelib(void);
int ioctl_tnpu_job_submit(unsigned long arg);
void tnpu_duty_ratio_init(void);
void tnpu_duty_ratio_show(unsigned long *all, unsigned long *run);
void tnpu_duty_ratio_stop(void);
void tnpu_kill_job(pid_t pid);

/* other */

#endif // __TNPU_DRV_H__
