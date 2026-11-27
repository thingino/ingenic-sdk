/*
 * Ingenic TNPU driver
 *
 * Copyright (c) 2023 LiuTianyang
 *
 * This file is released under the GPLv2
 */

#include "tnpu_drv.h"
#include "tnpu_register.h"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/pm.h>

#ifdef CONFIG_TNPU_DEV_DEBUG
#define PRINT_TNPU_DEBUG(fmt, ...) \
    printk(KERN_DEBUG "<TNPU> " fmt, ##__VA_ARGS__)
#else
#define PRINT_TNPU_DEBUG(fmt, ...)
#endif

#define TNPU_PID_LOG_MAX            (16)
struct tnpu_pid_log {
    pid_t                           pid;
    int                             count;
};

struct tnpu_drv_manger {
    //atomic_t                        open_cnt;
    struct mutex                    open_mutex;
    unsigned int                    dev_open_cnt;
    struct tnpu_pid_log             precord[TNPU_PID_LOG_MAX];
    struct miscdevice               misc;
};
static struct tnpu_drv_manger tnpu;
static const struct file_operations tnpu_fops;

static char *parent_clk_name = TNPU_PARENT_NAME;
module_param(parent_clk_name, charp, S_IRUGO);
MODULE_PARM_DESC(parent_clk_name,
        "TNPU parent clk: apll/vpll/mpll, default is apll");

static unsigned long tnpu_clk_rate = TNPU_DEFAULT_CLK_RATE;
module_param(tnpu_clk_rate, ulong, S_IRUGO);
MODULE_PARM_DESC(tnpu_clk_rate, "Set tnpu clk rate.(Hz).");

static unsigned int cma_pool_size = TNPU_DEFAULT_CMA_SIZE;
module_param(cma_pool_size, uint, S_IRUGO);
MODULE_PARM_DESC(cma_pool_size, "Size of the CMA pool to occupy (in bytes)");


static int tnpu_probe(struct platform_device *pdev)
{
    int ret;

    if (cma_pool_size & 4095) {
        cma_pool_size = (cma_pool_size + 4095) & ~4095;
    }
    ret = tnpu_bo_init(&pdev->dev, cma_pool_size);
    if (ret)
        goto probe_fail;

    ret = tnpu_core_init(pdev, parent_clk_name, tnpu_clk_rate);
    if (ret) {
        tnpu_bo_pool_remove();
        goto probe_fail;
    }

    mutex_init(&tnpu.open_mutex);
    memset(tnpu.precord, 0, sizeof(tnpu.precord));

    tnpu.misc.minor = MISC_DYNAMIC_MINOR;
    tnpu.misc.name  = TNPU_DEV_NAME;
    tnpu.misc.fops  = &tnpu_fops;
    tnpu.misc.parent = &pdev->dev;
    ret = misc_register(&tnpu.misc);
    if (ret) {
        tnpu_core_remove();
        tnpu_bo_pool_remove();
        goto probe_fail;
    }


    PRINT_TNPU_INFO("Successful insmod, version=0x%x.\n", TNPU_DRIVER_VERSION);
    return 0;

probe_fail:
    return ret;
}

static void tnpu_remove(struct platform_device *pdev)
{
    misc_deregister(&tnpu.misc);
    tnpu_core_remove();
    tnpu_bo_pool_remove();
    PRINT_TNPU_DEBUG("tnpu remove.\n");
}

static int tnpu_proc_open(pid_t pid)
{
    int i;
    PRINT_TNPU_DEBUG("tnpu proc[%d] open.\n", pid);

    for (i = 0; i < TNPU_PID_LOG_MAX; i++) {
        if (tnpu.precord[i].pid == pid) {
            tnpu.precord[i].count++;
            return 0;
        }
    }

    for (i = 0; i < TNPU_PID_LOG_MAX; i++) {
        if (tnpu.precord[i].pid == 0) {
            tnpu.precord[i].pid = pid;
            tnpu.precord[i].count = 1;
            return 0;
        }
    }

    PRINT_TNPU_ERROR("Reached the record upper limit of PID, %d\n", pid);
    return -1;
}

static int tnpu_proc_close(pid_t pid)
{
    int i;
    PRINT_TNPU_DEBUG("tnpu proc[%d] close.\n", pid);

    for (i = 0; i < TNPU_PID_LOG_MAX; i++) {
        if (tnpu.precord[i].pid == pid) {
            tnpu.precord[i].count--;
            if (tnpu.precord[i].count == 0) {
                tnpu.precord[i].pid = 0;
                tnpu_kill_job(pid);
                tnpu_bo_release(pid);
		PRINT_TNPU_DEBUG("tnpu proc[%d] release.\n", pid);
            }
            return 0;
        }
    }

    PRINT_TNPU_ERROR("Invalid pid, %d\n", pid);
    return -1;
}


static int tnpu_open(struct inode *indo, struct file *file)
{
    //if (atomic_inc_return(&tnpu.open_cnt) == 1) {
    //    tnpu_clk_open();
    //    tnpu_timer_open();
    //    PRINT_TNPU_DEBUG("tnpu clk turned on.\n");
    //}

    int ret;
    int clk_opened = 0;
    pid_t pid = task_tgid_nr(current);

    mutex_lock(&tnpu.open_mutex);

    if (tnpu.dev_open_cnt == 0) {
        ret = tnpu_clk_open();
        if (ret) {
            mutex_unlock(&tnpu.open_mutex);
            return ret;
        }
        clk_opened = 1;
        tnpu_timer_open();
        PRINT_TNPU_DEBUG("tnpu clk turned on.\n");
    }

    ret = tnpu_proc_open(pid);
    if (ret) {
        if (clk_opened) {
            tnpu_timer_close();
            tnpu_clk_close();
        }
        mutex_unlock(&tnpu.open_mutex);
        return ret;
    }

    tnpu.dev_open_cnt++;

    mutex_unlock(&tnpu.open_mutex);
    return 0;
}

static int tnpu_release(struct inode *inode, struct file *file)
{
    int ret;
    pid_t pid = task_tgid_nr(current);

    //tnpu_kill_job(pid);
    //tnpu_bo_release(pid);

    //if (atomic_dec_and_test(&tnpu.open_cnt)) {
    //    tnpu_timer_close();
    //    tnpu_clk_close();
    //    tnpu_duty_ratio_stop();
    //    PRINT_TNPU_DEBUG("tnpu clk turned off.\n");
    //}


    mutex_lock(&tnpu.open_mutex);

    ret = tnpu_proc_close(pid);
    if (ret) {
        mutex_unlock(&tnpu.open_mutex);
        return ret;
    }

    tnpu.dev_open_cnt--;
    if (tnpu.dev_open_cnt == 0) {
        tnpu_timer_close();
        tnpu_clk_close();
        tnpu_duty_ratio_stop();
        PRINT_TNPU_DEBUG("tnpu clk turned off.\n");
    }

    mutex_unlock(&tnpu.open_mutex);

    PRINT_TNPU_DEBUG("tnpu release, pid[%d].\n", pid);
    return 0;
}

static ssize_t tnpu_copy_read_data(char __user *buffer, size_t count,
        const void *data, size_t data_size)
{
    size_t copy_size = min(count, data_size);

    if (copy_size == 0)
        return 0;

    if (copy_to_user(buffer, data, copy_size)) {
        PRINT_TNPU_ERROR("tnpu drv read fail\n");
        return -EFAULT;
    }

    return copy_size;
}

static ssize_t tnpu_read(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
    tnpu_read_comamnd_t cmd;
    tnpu_read_pool_t pool;
    tnpu_read_ratio_t ratio;
    tnpu_read_timer_t timer;
    tnpu_read_version_t version;

    if (count < sizeof(cmd))
        return -EINVAL;

    if (copy_from_user(&cmd, buffer, sizeof(cmd))) {
        PRINT_TNPU_ERROR("tnpu drv read cmd fail\n");
        return -EFAULT;
    }

    switch (cmd) {
    case TNPU_READ_TIMER:
        timer.timer = tnpu_timer_get();
        return tnpu_copy_read_data(buffer, count, &timer, sizeof(timer));
    case TNPU_READ_POOL_AVAIL:
        pool.size = tnpu_pool_avail();
        return tnpu_copy_read_data(buffer, count, &pool, sizeof(pool));
    case TNPU_READ_POOL_SIZE:
        pool.size = tnpu_pool_size();
        return tnpu_copy_read_data(buffer, count, &pool, sizeof(pool));
    case TNPU_READ_DUTY_INIT:
        tnpu_duty_ratio_init();
        return min_t(size_t, count, sizeof(cmd));
    case TNPU_READ_DUTY_STOP:
        tnpu_duty_ratio_stop();
        return min_t(size_t, count, sizeof(cmd));
    case TNPU_READ_DUTY_SHOW:
        memset(&ratio, 0, sizeof(ratio));
        ratio.cmd = TNPU_READ_DUTY_SHOW;
        tnpu_duty_ratio_show(&ratio.all, &ratio.run);
        return tnpu_copy_read_data(buffer, count, &ratio, sizeof(ratio));
    case TNPU_READ_VERSION:
        version.version = TNPU_DRIVER_VERSION;
        return tnpu_copy_read_data(buffer, count, &version, sizeof(version));
    default:
        PRINT_TNPU_ERROR("Can not support cmd:%d\n", cmd);
        return -EINVAL;
    }
}

static ssize_t tnpu_write(struct file *file,
        const char __user *buffer, size_t count, loff_t *ppos)
{
    int ret;
    char command[72];
    size_t copy_len;
    unsigned long pool_info;

    if (count == 0)
        return 0;

    copy_len = min(count, sizeof(command) - 1);
    if (copy_from_user(command, buffer, copy_len))
        return -EFAULT;

    command[copy_len] = '\0';

    if (strncmp(command, "alloc", 5) == 0) {
        char *num_start = command + 5;

        while (*num_start == ' ' || *num_start == '\t')
            num_start++;

        ret = kstrtoul(num_start, 0, &pool_info);
        if (ret) {
            PRINT_TNPU_ERROR("Invalid size format: %s\n", command);
            return ret;
        }

        ret = tnpu_bo_pool_add(0, pool_info);
        if (ret)
            return ret;
    } else if (strncmp(command, "pool_paddr", 10) == 0) {
        char *num_start = command + 10;

        while (*num_start == ' ' || *num_start == '\t')
            num_start++;

        ret = kstrtoul(num_start, 10, &pool_info);
        if (ret) {
            PRINT_TNPU_ERROR("Invalid size format: %s\n", command);
            return ret;
        }

        ret = tnpu_bo_pool_add(pool_info, tnpu_pool_quota());
        if (ret)
            return ret;
    } else if (strncmp(command, "free", 4) == 0) {
        tnpu_bo_pool_remove();
    } else if (strncmp(command, "show", 4) == 0) {
        tnpu_show_allocations();
        tnpu_show_sharelib();
    } else {
        return -EINVAL;
    }

    return count;
}

static int tnpu_mmap(struct file *filp, struct vm_area_struct *vma)
{
    return tnpu_bo_mmap(vma);
}

static long tnpu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case IOCTL_TNPU_BO_APPEND:
        return ioctl_tnpu_bo_append(arg);
    case IOCTL_TNPU_BO_DESTROY:
        return ioctl_tnpu_bo_destroy(arg);
    case IOCTL_TNPU_SHARELIB_LOAD:
        return ioctl_tnpu_sharelib_load(arg);
    case IOCTL_TNPU_SHARELIB_UNLOAD:
        return ioctl_tnpu_sharelib_unload(arg);
    case IOCTL_TNPU_JOB_WAIT:
        return ioctl_tnpu_job_wait(arg);
    case IOCTL_TNPU_JOB_SUBMIT:
        return ioctl_tnpu_job_submit(arg);
    case IOCTL_TNPU_POOL_QUOTA:
        return ioctl_tnpu_pool_quota(arg);
    case IOCTL_TNPU_POOL_CREATE:
        return ioctl_tnpu_pool_create(arg);
    default:
        PRINT_TNPU_ERROR("Ioctl does not support %d\n", cmd);
        return -ENOTTY;
    }
    return 0;
}

#ifdef CONFIG_PM
static int tnpu_suspend(struct device *dev) {
    tnpu_clk_close();
    return 0;
}
static int tnpu_resume(struct device *dev) {
    return tnpu_clk_open();
}
#endif // CONFIG_PM


static const struct file_operations tnpu_fops = {
    .owner          = THIS_MODULE,
    .open           = tnpu_open,
    .release        = tnpu_release,
    .read           = tnpu_read,
    .write          = tnpu_write,
    .mmap           = tnpu_mmap,
    .unlocked_ioctl = tnpu_ioctl,
};

#ifdef CONFIG_PM
static SIMPLE_DEV_PM_OPS(tnpu_pm_ops, tnpu_suspend, tnpu_resume);
#endif

/* The T32 and T33 vendor drivers are the same source (uranus-3.2-C tnpu_t32, tnpu_t33) */
static const struct of_device_id tnpu_of_match[] = {
    { .compatible = "ingenic,t32-tnpu" },
    { }
};
MODULE_DEVICE_TABLE(of, tnpu_of_match);

static struct platform_driver tnpu_driver = {
    .probe  = tnpu_probe,
    .remove = tnpu_remove,
    .driver = {
        .name           = "tnpu",
        .of_match_table = tnpu_of_match,
#ifdef CONFIG_PM
        .pm             = &tnpu_pm_ops,
#endif
    },
};
module_platform_driver(tnpu_driver);

MODULE_DESCRIPTION("Ingenic TNPU Driver");
MODULE_AUTHOR("LiuTianyang <rick.tyliu@ingenic.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("20241203");
