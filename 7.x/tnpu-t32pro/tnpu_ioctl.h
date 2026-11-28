/*
 * Ingenic TNPU ioctl
 *
 * Copyright (c) 2023 LiuTianyang
 *
 * This file is released under the GPLv2
*/
#ifndef _TNPU_IOCTL_H_
#define _TNPU_IOCTL_H_

#define TNPU_DEV_NAME                  "ingenic-npu"

/*  Reference Documentation/ioctl/ioctl-number.txt */
#define IOCTL_TNPU_BO_APPEND           _IOWR(0xF4, 0x21, int)
#define IOCTL_TNPU_BO_DESTROY          _IOWR(0xF4, 0x22, int)
#define IOCTL_TNPU_SHARELIB_LOAD       _IOWR(0xF4, 0x23, int)
#define IOCTL_TNPU_SHARELIB_UNLOAD     _IOWR(0xF4, 0x24, int)
#define IOCTL_TNPU_JOB_WAIT            _IOWR(0xF4, 0x25, int)
#define IOCTL_TNPU_JOB_SUBMIT          _IOWR(0xF4, 0x26, int)
#define IOCTL_TNPU_POOL_QUOTA          _IOWR(0xF4, 0x27, int)
#define IOCTL_TNPU_POOL_CREATE         _IOWR(0xF4, 0x28, int)
#define IOCTL_TNPU_JOB_SUBMIT_CHAIN    _IOWR(0xF4, 0x29, int)

/* CTL */
#define TNPU_JOB_BACKGROUND_SHIFT      (0)
#define TNPU_JOB_KEEP_CACHE_SHIFT      (1)
#define TNPU_JOB_BACKGROUND            (1 << TNPU_JOB_BACKGROUND_SHIFT)
#define TNPU_JOB_KEEP_CACHE            (1 << TNPU_JOB_KEEP_CACHE_SHIFT)


/* DEVICE */
#define TNPU_TASK_DEVICE_V0            (1 << 0)
#define TNPU_TASK_DEVICE_V1            (1 << 1)
#define TNPU_TASK_DEVICE_CSC           (1 << 2)
#define TNPU_TASK_DEVICE_LUT           (1 << 3)
#define TNPU_TASK_DEVICE_NNA           (1 << 4)

#define TNPU_TASK_DEVICE_EVENT0        (1 << 8)
//                                     (1 << 9) reserver

/* NICE */
#define TNPU_TASK_NICE_LOW             (0)
#define TNPU_TASK_NICE_MEDIUM          (1)
#define TNPU_TASK_NICE_HIGH            (2)
#define TNPU_TASK_NICE_MAX             (3)
#define TNPU_TASK_SCHED_POOL_SIZE      (TNPU_TASK_NICE_MAX + 1)


/* DRIVER INFO */
#define TNPU_NAME_MAX                  (64) // max lib name length (bytes)
#define TNPU_MAX_ARG                   (4)  // max args for task func
#define TNPU_MAX_TASK                  (16) // max submit tasks in queue
#define TNPU_MAX_TASK_SEND             (2)

typedef struct {
	int                      handle;
	unsigned int             size;
	unsigned int             paddr;
} tnpu_ioctl_bo_t;

typedef struct {
	char                     name[TNPU_NAME_MAX];
	unsigned int             uvaddr;
	unsigned int             paddr;
} tnpu_ioctl_sharelib_t;

typedef struct {
	unsigned int             id;
	unsigned long long       seqno;
	unsigned int             timeout_us;
} tnpu_ioctl_wait_t;

typedef struct {
	unsigned int             device[2];
	unsigned int             pc[2];
	unsigned int             arg[2][4];
	int                      nice;
	int                      oram;
	unsigned int             ctl;
	tnpu_ioctl_wait_t        wait;
} tnpu_ioctl_submit_t;

typedef struct {
	unsigned int             job_num;
	tnpu_ioctl_submit_t     *job;
} tnpu_ioctl_submit_chain_t;

typedef enum {
	TNPU_READ_VERSION,
	TNPU_READ_TIMER,
	TNPU_READ_POOL_AVAIL,
	TNPU_READ_POOL_SIZE,
	TNPU_READ_DUTY_INIT,
	TNPU_READ_DUTY_SHOW,
	TNPU_READ_DUTY_STOP,
} tnpu_read_comamnd_t;

typedef union {
	tnpu_read_comamnd_t        cmd;
	unsigned int               version;
} tnpu_read_version_t;

typedef union {
	tnpu_read_comamnd_t        cmd;
	unsigned long long         timer;
} tnpu_read_timer_t;

typedef union {
	tnpu_read_comamnd_t        cmd;
	unsigned int               size;
} tnpu_read_pool_t;

typedef struct {
	tnpu_read_comamnd_t        cmd;
	unsigned long              all;
	unsigned long              run;
} tnpu_read_ratio_t;

#endif // _TNPU_IOCTL_H_
