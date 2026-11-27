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

#define TNPU_NAME_MAX 32

#define TNPU_JOB_BLOCK_SHIFT           (0)
#define TNPU_JOB_DELAY_SHIFT           (1)
#define TNPU_KEEP_HOST_CACHE_SHIFT     (2)
#define TNPU_JOB_BLOCK                 (1 << TNPU_JOB_BLOCK_SHIFT)
#define TNPU_JOB_DELAY                 (1 << TNPU_JOB_DELAY_SHIFT)
#define TNPU_KEEP_HOST_CACHE           (1 << TNPU_KEEP_HOST_CACHE_SHIFT)


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
    unsigned int             seqno;
    unsigned int             timeout_us;
} tnpu_ioctl_wait_t;

typedef struct {
    unsigned int             pc;
    unsigned int             arg[4];
    unsigned int             ctl;
    unsigned int             id;
} tnpu_ioctl_submit_t;

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


#define TNPU_SHM_OFFSET                (0x200) // TNPU_DRAM_BOOT_SIZE
#define TNPU_MAX_ARG                   (4)
#define TNPU_MAX_TASK                  (8)
struct tnpu_task {
    unsigned int id;
    unsigned int pid;
    unsigned int pc;
    unsigned int arg[TNPU_MAX_ARG];
    unsigned int start_time_l;
    unsigned int start_time_h;
    unsigned int stop_time_l;
    unsigned int stop_time_h;
};

struct tnpu_shm {
    unsigned int task_done_id;
    unsigned int run_status;
    unsigned int task_max_id;
    struct tnpu_task queue[TNPU_MAX_TASK];
};

#endif // _TNPU_IOCTL_H_
