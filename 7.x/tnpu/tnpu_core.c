/*
 * Ingenic TNPU driver
 *
 * Copyright (c) 2023 LiuTianyang
 *
 * This file is released under the GPLv2
 */

#include "tnpu_drv.h"
#include "tnpu_register.h"

#include "tnpu_boot_code.h"

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/file.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/r4kcache.h>
#include <asm/mipsregs.h>

#ifdef CONFIG_TNPU_CORE_DEBUG
#define PRINT_TNPU_DEBUG(fmt, ...) \
    printk(KERN_DEBUG "<TNPU> " fmt, ##__VA_ARGS__)
#else
#define PRINT_TNPU_DEBUG(fmt, ...)
#endif

/* exported by arch/mips/mm/c-r4k.c, not declared in a header */
extern void (*r4k_blast_dcache)(void);


/*
 * The vendor gives the core ~80 us (0x3ff polls) to reach its WFI after a
 * reset; a core running in another QEMU process can take longer. Silicon
 * answers within the first polls either way.
 */
#define TNPU_POLL_US              20000

#define TNPU_R_RISCV_32           1
#define TNPU_R_RISCV_RELATIVE     3
#define TNPU_R_RISCV_JUMP_SLOT    5

struct tnpu_libref {
    struct list_head               node;
    unsigned int                   paddr;
    pid_t                          pid;
};

struct tnpu_sharelib {
    struct list_head               node;
    char                           name[32];
    int                            ref_cnt;
    unsigned int                   size;
    void                          *kva;
    unsigned int                   paddr;
    Elf32_Dyn                     *dyn;
    char                          *strtab;
    Elf32_Sym                     *symtab;
    unsigned int                   sym_num;
    unsigned int                   need_lib[4];
    unsigned int                   need_lib_num;
};

struct tnpu_core_dev {
    struct device                  *dev;
    void __iomem                   *ccu_regs;
    void __iomem                   *dram_regs;
    int                             tnpu_irq;
    wait_queue_head_t               wait_queue;
    spinlock_t                      task_spin;
    struct mutex                    load_mutex;
    struct list_head                libref_list;
    struct list_head                sharelib_list;
    int (*libread)(struct file *elf,
            unsigned int uvaddr, loff_t off,
            void *dst_vaddr, size_t size);
    struct clk                     *div_tnpu;
    struct clk                     *gate_tnpu;
    int                             is_open;
#ifdef CONFIG_TNPU_DUTY_RATIO_DEBUG
    ktime_t                         basic_timer;
    ktime_t                         start_timer;
    unsigned long                   run_time;
    int                             duty_status;
#endif // CONFIG_TNPU_DUTY_RATIO_DEBUG
};
static struct tnpu_core_dev tnpu;

static void __iomem *tnpu_ioremap_regs(struct platform_device *pdev, int index)
{
    struct resource *res;
    void __iomem *map;

    res = platform_get_resource(pdev, IORESOURCE_MEM, index);
    map = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(map)) {
        PRINT_TNPU_ERROR("Failed to map registers: %ld\n", PTR_ERR(map));
        return map;
    }

    return map;
}

static irqreturn_t tnpu_irq_handler(int irq, void *data)
{
    unsigned int done_id, chn;
    unsigned long irqflags;
    int tohost = *TNPU_CCU_TOHOST;

#ifdef CONFIG_TNPU_SCHEDUL_DEBUG
    int done_chn;
    volatile unsigned long long done_time1;
    volatile unsigned long long done_time2;
    volatile unsigned long long done_time3 =
        ((unsigned long long)(*TNPU_CCU_TIME_H) << 32) | *TNPU_CCU_TIME_L;
    volatile unsigned long long done_time4;
#endif // CONFIG_TNPU_SCHEDUL_DEBUG

    *TNPU_CCU_TOHOST = 0;

    PRINT_TNPU_DEBUG("Received IRQ=%d.\n", tohost);
    if (tohost != 1) {
        int cmd = tohost & 0xf;
        int chn = (tohost >> 4) & 0xf;
        switch (cmd) {
        case TNPU_TOHOST_RET_EXL:
            PRINT_TNPU_ERROR("job=%d,pc=0x%x,abnormal exit!\n",
                    *TNPU_DRAM_TASK_ID(chn), *TNPU_DRAM_TASK_PC(chn));
            break;
        case TNPU_TOHOST_RET_TNNA:
            PRINT_TNPU_ERROR("job=%d,pc=0x%x,terminated abnormally in TNNA!\n",
                    *TNPU_DRAM_TASK_ID(chn), *TNPU_DRAM_TASK_PC(chn));
            break;
        case TNPU_TOHOST_RET_CVA:
            PRINT_TNPU_ERROR("job=%d,pc=0x%x,terminated abnormally in CVA!\n",
                    *TNPU_DRAM_TASK_ID(chn), *TNPU_DRAM_TASK_PC(chn));
            break;
        case TNPU_TOHOST_RET_STACK:
            PRINT_TNPU_ERROR("job=%d,pc=0x%x,stack overflow!\n",
                    *TNPU_DRAM_TASK_ID(chn), *TNPU_DRAM_TASK_PC(chn));
            break;
        }
        PRINT_TNPU_ERROR("Please check algorithm,mepc=0x%x,mtval=0x%x,type=%d,mstatus=0x%x\n",
                *TNPU_DRAM_TASK_ARG0(chn), *TNPU_DRAM_TASK_ARG1(chn),
                *TNPU_DRAM_TASK_ARG2(chn), *TNPU_DRAM_TASK_ARG3(chn));
    }

    /* the vendor BUG()s here; the core's stack is its own RAM, the kernel goes on */
    if (*TNPU_DRAM_SHM_END != TNPU_STACK_MAJIC) {
        chn = *TNPU_DRAM_SHM_TASK_DONE_ID % TNPU_MAX_TASK;
        PRINT_TNPU_ERROR("tnpu stack overflow, id=%d,pc=0x%x\n",
                *TNPU_DRAM_TASK_ID(chn), *TNPU_DRAM_TASK_PC(chn));
        *TNPU_DRAM_SHM_END = TNPU_STACK_MAJIC;
    }

    spin_lock_irqsave(&tnpu.task_spin, irqflags);
    done_id = *TNPU_DRAM_SHM_TASK_DONE_ID;
    chn = done_id % TNPU_MAX_TASK;

    if (*TNPU_DRAM_TASK_PID(chn) != 0) {
        PRINT_TNPU_DEBUG("IRQ run_next,id=0x%x\n", done_id);
        *TNPU_CCU_FROMHOST = 1;
    } else {
        PRINT_TNPU_DEBUG("IRQ stop\n");
        *TNPU_DRAM_SHM_RUN_STATUS = 0;
#ifdef CONFIG_TNPU_DUTY_RATIO_DEBUG
        if (tnpu.duty_status == 3) {
            tnpu.run_time += ktime_to_us(ktime_sub(ktime_get(),
                    tnpu.start_timer));
            tnpu.duty_status = 2;
        }
#endif // CONFIG_TNPU_DUTY_RATIO_DEBUG
    }
    spin_unlock_irqrestore(&tnpu.task_spin, irqflags);

#ifdef CONFIG_TNPU_SCHEDUL_DEBUG
    done_time4 = ((unsigned long long)(*TNPU_CCU_TIME_H) << 32) | *TNPU_CCU_TIME_L;
    wake_up_all(&tnpu.wait_queue);
    done_chn = (*TNPU_DRAM_SHM_TASK_DONE_ID - 1) % 8;
    done_time1 = ((unsigned long long)(*TNPU_DRAM_TASK_START_TIME_H(done_chn)) << 32)|
            *TNPU_DRAM_TASK_START_TIME_L(done_chn);
    done_time2 = ((unsigned long long)(*TNPU_DRAM_TASK_STOP_TIME_H(done_chn)) << 32)|
            *TNPU_DRAM_TASK_STOP_TIME_L(done_chn);
    PRINT_TNPU_INFO("IRQ:chn=%d, timer1=%lld, timer2=%lld, timer3=%lld\n",
            done_chn, done_time1, done_time2, done_time3);
#else
    wake_up_all(&tnpu.wait_queue);
#endif // CONFIG_TNPU_SCHEDUL_DEBUG

    return IRQ_HANDLED;
}

void tnpu_timer_open(void)
{
    *TNPU_CCU_CCSR |= 1 << 5;
    *TNPU_CCU_TIME_L = 0;
    *TNPU_CCU_TIME_H = 0;
}

void tnpu_timer_close(void)
{
    *TNPU_CCU_CCSR &= ~(1 << 5);
}

unsigned long long tnpu_timer_get(void)
{
    return ((unsigned long long)(*TNPU_CCU_TIME_H) << 32) | *TNPU_CCU_TIME_L;
}

static inline void tnpu_hardware_stop(void)
{
    *TNPU_CCU_SOFT_RESET = CCU_SOFT_RESET_CMD_SET;
}

static inline void tnpu_hardware_load(void)
{
    //volatile unsigned int *chip_info = (volatile unsigned int *)0xb300002c;
    //unsigned int chip_id = (*chip_info >> 12) & 0xff;

    //if (chip_id == 0x33) {
    //    memcpy((void *)TNPU_DRAM_BOOT_START,
    //            (void *)tnpu_boot_code_fence, sizeof(tnpu_boot_code_fence));
    //} else if (chip_id == 0x32) {
    //    memcpy((void *)TNPU_DRAM_BOOT_START,
    //            (void *)tnpu_boot_code_nofence, sizeof(tnpu_boot_code_nofence));
    //} else {
    //    PRINT_TNPU_ERROR("Failed to get chip id, 0x%x\n", *chip_info);
    //}
    memcpy((void *)TNPU_DRAM_BOOT_START,
            (void *)tnpu_boot_code, sizeof(tnpu_boot_code));
}

static int tnpu_hardware_reset(void)
{
    volatile unsigned int ccsr;
    int cnt = 0;

    *TNPU_CCU_SOFT_RESET = CCU_SOFT_RESET_CMD_SET;
    ndelay(10);
    tnpu_hardware_load();

    *TNPU_DRAM_SHM_TASK_DONE_ID = 0x0;
    *TNPU_DRAM_SHM_RUN_STATUS = 0;
    *TNPU_DRAM_SHM_TASK_MAX_ID = 0x0;

    *TNPU_DRAM_SHM_END = TNPU_STACK_MAJIC;

    *TNPU_CCU_SOFT_RESET = CCU_SOFT_RESET_CMD_RELEASE;

    do {
        udelay(1);
        ccsr = *TNPU_CCU_CCSR;
        cnt++;
        if (cnt > TNPU_POLL_US)
            return -ETIME;
    } while(!(ccsr & TNPU_CCU_CCSR_SLEEP_MASK));

    return 0;
}

int tnpu_clk_open(void)
{
    int ret;

    mutex_lock(&tnpu.load_mutex);
    if (tnpu.is_open) {
        mutex_unlock(&tnpu.load_mutex);
        return 0;
    }

    ret = clk_prepare_enable(tnpu.div_tnpu);
    if (ret) {
        PRINT_TNPU_ERROR("Failed to open div_tnpu, %d\n", ret);
        goto err_unlock;
    }
    ret = clk_prepare_enable(tnpu.gate_tnpu);
    if (ret) {
        PRINT_TNPU_ERROR("Failed to open gate_tnpu, %d\n", ret);
        clk_disable_unprepare(tnpu.div_tnpu);
        goto err_unlock;
    }

    ret = tnpu_hardware_reset();
    if (ret) {
        clk_disable_unprepare(tnpu.gate_tnpu);
        clk_disable_unprepare(tnpu.div_tnpu);
        goto err_unlock;
    }

    tnpu.is_open = 1;
    mutex_unlock(&tnpu.load_mutex);

    return 0;

err_unlock:
    mutex_unlock(&tnpu.load_mutex);
    return ret;
}

void tnpu_clk_close(void)
{
    mutex_lock(&tnpu.load_mutex);
    if (tnpu.is_open) {
        tnpu_hardware_stop();

        clk_disable_unprepare(tnpu.gate_tnpu);
        clk_disable_unprepare(tnpu.div_tnpu);
        tnpu.is_open = 0;
    }
    mutex_unlock(&tnpu.load_mutex);
}

int tnpu_core_init(struct platform_device *pdev,
            const char *parent_clk_name,
            unsigned long tnpu_clk_rate)
{
    int ret;
    unsigned long rate;
    struct clk *parent_clk;
    struct clk *clk;

    tnpu.ccu_regs = tnpu_ioremap_regs(pdev, 0);
    if (IS_ERR(tnpu.ccu_regs))
        return PTR_ERR(tnpu.ccu_regs);

    tnpu.dram_regs = tnpu_ioremap_regs(pdev, 1);
    if (IS_ERR(tnpu.dram_regs))
        return PTR_ERR(tnpu.dram_regs);

    tnpu.tnpu_irq = platform_get_irq(pdev, 0);
    if (tnpu.tnpu_irq < 0)
        return tnpu.tnpu_irq;

    init_waitqueue_head(&tnpu.wait_queue);
    spin_lock_init(&tnpu.task_spin);
    mutex_init(&tnpu.load_mutex);
    INIT_LIST_HEAD(&tnpu.libref_list);
    INIT_LIST_HEAD(&tnpu.sharelib_list);

    /* the CGU's TNPU clock is the vendor's mux_tnpu, div_tnpu and gate_tnpu in one */
    clk = devm_clk_get(&pdev->dev, "tnpu");
    if (IS_ERR(clk)) {
        PRINT_TNPU_ERROR("Failed to get the tnpu clock\n");
        return PTR_ERR(clk);
    }
    tnpu.div_tnpu = clk;
    tnpu.gate_tnpu = clk;

    if (!parent_clk_name || !*parent_clk_name)
        parent_clk_name = "apll";
    /* the vendor's "apll" parent is the CGU's SCLKA, which the APLL drives */
    parent_clk = devm_clk_get_optional(&pdev->dev,
            strcmp(parent_clk_name, "apll") ? parent_clk_name : "sclka");
    if (IS_ERR(parent_clk))
        return PTR_ERR(parent_clk);
    if (parent_clk && clk_set_parent(clk, parent_clk))
        PRINT_TNPU_ERROR("Failed to set parent %s\n", parent_clk_name);

    ret = clk_prepare_enable(clk);
    if (ret) {
        PRINT_TNPU_ERROR("Failed to open tnpu clock, %d\n", ret);
        return ret;
    }
    clk_set_rate(clk, tnpu_clk_rate);
    rate = clk_get_rate(clk) / 1000000;
    PRINT_TNPU_INFO("clk: parent=%s(%luMHz), v0=%luMHz\n", parent_clk_name,
            clk_get_rate(clk_get_parent(clk)) / 1000000, rate);

    *TNPU_CCU_AXIQOS |= 0x8800ff;

    tnpu_hardware_load();
    /* div_tnpu and gate_tnpu are the same clock: enabled twice, as tnpu_clk_close() drops both */
    ret = clk_prepare_enable(clk);
    if (ret) {
        clk_disable_unprepare(clk);
        return ret;
    }
    tnpu.is_open = 1;
    tnpu_clk_close();

    ret = devm_request_irq(&pdev->dev, tnpu.tnpu_irq,
            tnpu_irq_handler, IRQF_SHARED, "tnpu-irq", &tnpu);
    if (ret) {
        PRINT_TNPU_ERROR("request_irq fail\n");
        return ret;
    }
    return 0;
}

void tnpu_core_remove(void)
{
    struct tnpu_sharelib *lib, *tmp_lib;
    struct tnpu_libref *libref, *tmp_libref;

    mutex_lock(&tnpu.load_mutex);
    list_for_each_entry_safe(libref, tmp_libref, &tnpu.libref_list, node) {
        list_del(&libref->node);
        kfree(libref);
    }
    list_for_each_entry_safe(lib, tmp_lib, &tnpu.sharelib_list, node) {
        list_del(&lib->node);
        if (lib->kva)
            tnpu_cma_free(lib->size, lib->kva, lib->paddr);
        kfree(lib);
    }
    mutex_unlock(&tnpu.load_mutex);

    tnpu_clk_close();
}

static int tnpu_wait(unsigned int id, unsigned long timeout_us)
{
    wait_queue_head_t *wq = &tnpu.wait_queue;
    unsigned long timeout_jiffies;
    unsigned long timeout_expire;
    unsigned long start_jiffies = jiffies;
    DEFINE_WAIT(wait);

    if (timeout_us > 0) {
        timeout_jiffies = usecs_to_jiffies(timeout_us);
        if (timeout_jiffies < 2)
            timeout_jiffies = 2;
        timeout_expire = start_jiffies + timeout_jiffies;
    }

    if (*TNPU_DRAM_SHM_TASK_DONE_ID > id) {
        PRINT_TNPU_DEBUG("Finish wait, task_id=0x%x, done_id=%d.\n",
                id, *TNPU_DRAM_SHM_TASK_DONE_ID);
        return 0;
    }
    if (id > 0xfffffff8 && *TNPU_DRAM_SHM_TASK_DONE_ID < 8) {
        PRINT_TNPU_DEBUG("Finish overflow wait, task_id=%d, done_id=%d.\n",
                id, *TNPU_DRAM_SHM_TASK_DONE_ID);
        return 0;
    }

    for (;;) {
        prepare_to_wait(wq, &wait, TASK_INTERRUPTIBLE);

        if (*TNPU_DRAM_SHM_TASK_DONE_ID > id)
            break;

        if (id > 0xfffffff8 && *TNPU_DRAM_SHM_TASK_DONE_ID < 8) {
            PRINT_TNPU_DEBUG("overflow wait, task_id=%d.\n", id);
            break;
        }

        if (signal_pending(current)) {
            finish_wait(wq, &wait);
            PRINT_TNPU_ERROR("The waiting was terminated by unknown signal.\n");
            msleep(20);
            return -ERESTARTSYS;
        }

        if (timeout_us == 0) {
            schedule();
        } else {
            if (time_after_eq(jiffies, timeout_expire)) {
                finish_wait(wq, &wait);
                PRINT_TNPU_ERROR("Timeout occurred while waiting.\n");
                return -ETIME;
            }
            schedule_timeout(timeout_jiffies);
        }
    }

    finish_wait(wq, &wait);

    PRINT_TNPU_DEBUG("Finish wait, task_id=0x%x, done_id=%d.\n",
            id, *TNPU_DRAM_SHM_TASK_DONE_ID);

    return 0;
}

int ioctl_tnpu_job_wait(unsigned long arg)
{
    int ret;
    tnpu_ioctl_wait_t user_wait;

    ret = copy_from_user(&user_wait,
            (tnpu_ioctl_wait_t __user *)arg, sizeof(user_wait));
    if (ret) {
        PRINT_TNPU_ERROR("Failed to copy op from user\n");
        return -EFAULT;
    }

    if (*TNPU_DRAM_SHM_TASK_MAX_ID == 0) {
        // Bug, Exclude Previous Versions
        if (user_wait.seqno > 0x1000) {
            PRINT_TNPU_ERROR("The waiting seqno is 0x%x, max_id=0x%x, done_id=0x%x\n",
                user_wait.seqno, *TNPU_DRAM_SHM_TASK_MAX_ID, *TNPU_DRAM_SHM_TASK_DONE_ID);
            PRINT_TNPU_ERROR("Abnormal operation of library occurred!\n");
            PRINT_TNPU_ERROR("Please update the libaie.so.\n");
            return -EFAULT;
        }
    }

    return tnpu_wait(user_wait.seqno, user_wait.timeout_us);
}

static int tnpu_read_from_file(struct file *elf, unsigned int uvaddr,
        loff_t off, void *dst_vaddr, size_t size)
{
    int ret;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 0)
    ret = kernel_read(elf, dst_vaddr, size, &off);
#else
    ret = kernel_read(elf, off, dst_vaddr, size);
#endif
    if (ret < 0)
        return ret;
    if (ret != size)
        return -EIO;
    return 0;
}

static int tnpu_read_from_mem(struct file *elf, unsigned int uvaddr,
        loff_t off, void *dst_vaddr, size_t size)
{
    if (copy_from_user(dst_vaddr, (void *)((char *)uvaddr + off), size))
        return -EFAULT;
    return 0;
}

static void tnpu_free_lib(unsigned int paddr)
{
    struct tnpu_sharelib *lib, *tmp_lib;

    list_for_each_entry_safe_reverse(lib, tmp_lib, &tnpu.sharelib_list, node) {
        if (lib->paddr == paddr) {
            lib->ref_cnt--;
            if (lib->ref_cnt == 0) {
                int i;
                list_del(&lib->node);
                for (i = 0; i < lib->need_lib_num; i++) {
                    tnpu_free_lib(lib->need_lib[i]);
                }
                PRINT_TNPU_DEBUG("%s has been uninstalled.\n", lib->name);
                tnpu_cma_free(lib->size, lib->kva, lib->paddr);
                kfree(lib);
                break;
            }
        }
    }
}

static int tnpu_is_jump_var(const Elf_Sym *sym, const char *sym_name)
{
    if (ELF32_ST_TYPE(sym->st_info) == STT_OBJECT &&
            ELF32_ST_BIND(sym->st_info) == STB_GLOBAL) {
        PRINT_TNPU_ERROR("[SHLIB] unsupported symbol reference: symbol='%s'\n",
                sym_name);
        PRINT_TNPU_ERROR("[SHLIB] Global object jumps are not allowed.\n");
        return -1;
    }

    if (ELF32_ST_TYPE(sym->st_info) != STT_FUNC && sym->st_value != 0) {
        PRINT_TNPU_ERROR("[SHLIB] unsupported symbol type: symbol='%s', type=%d, bind=%d\n",
                sym_name, ELF32_ST_TYPE(sym->st_info), ELF32_ST_BIND(sym->st_info));
        PRINT_TNPU_ERROR("[SHLIB] Export function symbols only.\n");
        return -2;
    }

    return 0;
}

static int tnpu_find_func_addr(const char *rela_name, unsigned int *func_addr)
{
    struct tnpu_sharelib *need_lib;
    unsigned int need_off;
    char *need_name;
    int j;

    if (!func_addr)
        return -EINVAL;

    *func_addr = 0;

    /* Search libraries in reverse order (newest first) */
    list_for_each_entry_reverse(need_lib, &tnpu.sharelib_list, node) {
        for (j = 1; j < need_lib->sym_num; j++) {
            need_off = need_lib->symtab[j].st_value;
            if (!need_off)
                continue;

            need_name = need_lib->strtab + need_lib->symtab[j].st_name;
            if (!strcmp(need_name, rela_name)) {
                if (tnpu_is_jump_var(&need_lib->symtab[j], need_name)) {
                    PRINT_TNPU_ERROR("[SHLIB] dependency symbol is not callable: lib='%s', symbol='%s'\n",
                            need_lib->name, need_name);
                    PRINT_TNPU_ERROR("[SHLIB] Check TNPU shared-library exports.\n");
                    return -EINVAL;
                }
                PRINT_TNPU_DEBUG("  Linked func: %s(from %s),addr:0x%x,offset:0x%x\n",
                        need_name, need_lib->name, need_lib->paddr, need_off);
                *func_addr = need_lib->paddr + need_off;
                return 0;
            }
        }
    }

    return -ENOENT;
}

static int tnpu_process_relocations(struct tnpu_sharelib *lib, Elf32_Rela *rela, int entry_id)
{
    unsigned int *rela_pos;
    int rela_type, rela_idx, link_off, ret;
    char *rela_name;

    rela_pos = (unsigned int *)((char *)lib->kva + rela[entry_id].r_offset);
    rela_type = ELF_R_TYPE(rela[entry_id].r_info);
    rela_idx = ELF_R_SYM(rela[entry_id].r_info);
    rela_name = lib->strtab + lib->symtab[rela_idx].st_name;

    PRINT_TNPU_DEBUG("  Link pos=%p,type=%d,idx=%d,name=%s\n",
            rela_pos, rela_type, rela_idx, rela_name);

    switch (rela_type) {
    case TNPU_R_RISCV_JUMP_SLOT:
    case TNPU_R_RISCV_32:
        link_off = lib->symtab[rela_idx].st_value;
        if (link_off) {
            if (tnpu_is_jump_var(&lib->symtab[rela_idx], rela_name)) {
                PRINT_TNPU_ERROR("[SHLIB] local symbol is not callable: lib='%s', symbol='%s'\n",
                        lib->name, rela_name);
                PRINT_TNPU_ERROR("[SHLIB] Check TNPU shared-library code.\n");
                return -1;
            }
            *rela_pos = lib->paddr + link_off;
        } else {
            ret = tnpu_find_func_addr(rela_name, rela_pos);
            if (ret == -ENOENT) {
                PRINT_TNPU_ERROR("[SHLIB] unresolved symbol: lib='%s', symbol='%s'\n",
                        lib->name, rela_name);
                PRINT_TNPU_ERROR("[SHLIB] Check dependency libraries and exported symbols.\n");
            }
            if (ret) {
                return -1;
            }
        }
        break;
    case TNPU_R_RISCV_RELATIVE:
        *rela_pos = lib->paddr + rela[entry_id].r_addend;
        break;
    default:
        PRINT_TNPU_ERROR("[SHLIB] unsupported relocation: lib='%s', type=%d\n",
                lib->name, rela_type);
        PRINT_TNPU_ERROR("[SHLIB] Check toolchain and linker settings.\n");
        return -1;
    }

    return 0;
}

static unsigned int tnpu_get_lib(const char *name, unsigned int vaddr)
{
    struct tnpu_sharelib *lib;
    struct file *elf;
    int ret, i;
    size_t name_len;
    unsigned int load_end = 0;
    unsigned int dyn_off = 0;
    unsigned int hash_off = 0;
    unsigned int dynsym_off = 0;
    unsigned int sym_num = 0;
    struct elfhdr ehdr;
    struct elf_phdr phdr;
    struct elf_shdr shdr;
    Elf32_Dyn *dyn;

    name_len = strnlen(name, TNPU_NAME_MAX - 1);

    /* Check if library already loaded */
    list_for_each_entry(lib, &tnpu.sharelib_list, node) {
        if (!strcmp(name, lib->name)) {
            PRINT_TNPU_DEBUG("%s lib already loaded.\n", name);
            lib->ref_cnt++;
            return lib->paddr;
        }
    }
    lib = NULL;

    /* Setup read method */
    if (vaddr == 0) {
        elf = filp_open(name, O_RDONLY, 0);
        if (IS_ERR(elf)) {
            PRINT_TNPU_ERROR("[SHLIB] open failed: lib='%s'\n", name);
            PRINT_TNPU_ERROR("[SHLIB] Check path, file existence, and permissions.\n");
            return 0;
        }
        tnpu.libread = tnpu_read_from_file;
    } else {
        elf = NULL;
        tnpu.libread = tnpu_read_from_mem;
    }

    /* Read ELF header */
    ret = tnpu.libread(elf, vaddr, 0, (char *)&ehdr, sizeof(struct elfhdr));
    if (ret < 0) {
        PRINT_TNPU_ERROR("[SHLIB] failed to read ELF header: lib='%s'\n", name);
        PRINT_TNPU_ERROR("[SHLIB] Check file completeness and TNPU shared-library format.\n");
        goto get_lib_error;
    }
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
            ehdr.e_ident[EI_CLASS] != ELFCLASS32 ||
            ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
            ehdr.e_type != ET_DYN) {
        PRINT_TNPU_ERROR("[SHLIB] invalid ELF header: lib='%s'\n", name);
        PRINT_TNPU_ERROR("[SHLIB] Check TNPU shared-library format.\n");
        goto get_lib_error;
    }

    /* Parse program headers */
    for (i = 0; i < ehdr.e_phnum; i++) {
        ret = tnpu.libread(elf, vaddr,
                ehdr.e_phoff + (i * sizeof(struct elf_phdr)),
                (char *)(&phdr), sizeof(struct elf_phdr));
        if (ret < 0) {
            PRINT_TNPU_ERROR("[SHLIB] failed to read program headers: lib='%s'\n", name);
            PRINT_TNPU_ERROR("[SHLIB] Check file completeness and ELF format.\n");
            goto get_lib_error;
        }
        if (phdr.p_type == PT_LOAD) {
            unsigned int end = phdr.p_offset + phdr.p_filesz;

            if (load_end < end)
                load_end = end;
        } else if (phdr.p_type == PT_DYNAMIC) {
            dyn_off = phdr.p_offset;
        }
    }
    if (!dyn_off || dyn_off >= load_end) {
        PRINT_TNPU_ERROR("[SHLIB] missing PT_DYNAMIC: lib='%s'\n", name);
        PRINT_TNPU_ERROR("[SHLIB] Check linker output and library type.\n");
        goto get_lib_error;
    }

    /* Allocate library structure */
    lib = kzalloc(sizeof(*lib), GFP_KERNEL);
    if (!lib) {
        PRINT_TNPU_ERROR("[SHLIB] metadata allocation failed: lib='%s'\n", name);
        PRINT_TNPU_ERROR("[SHLIB] Check kernel memory usage.\n");
        goto get_lib_error;
    }
    memcpy(lib->name, name, name_len);
    lib->name[name_len] = '\0';
    lib->ref_cnt = 1;
    lib->size = round_up(load_end, PAGE_SIZE);
    lib->kva = tnpu_cma_alloc(lib->size, &lib->paddr);
    if (!lib->kva) {
        PRINT_TNPU_ERROR("[SHLIB] out of contiguous memory: lib='%s', size=0x%x\n",
                name, lib->size);
        PRINT_TNPU_ERROR("[SHLIB] Check CMA size, pool usage, and loaded shared libraries.\n");
        goto get_lib_error;
    }
    lib->dyn = (Elf32_Dyn *)((char *)lib->kva + dyn_off);

    /* Load segments */
    ret = tnpu.libread(elf, vaddr, 0, (char *)lib->kva, load_end);
    if (ret < 0) {
        PRINT_TNPU_ERROR("[SHLIB] failed to load segments: lib='%s'\n", name);
        PRINT_TNPU_ERROR("[SHLIB] Check file completeness and source buffer content.\n");
        goto get_lib_error;
    }
    //printk(KERN_NOTICE "@@@@ lib: name=%s, kv=0x%x, pa=0x%x, dyn=0x%x\n",
    //        name,(unsigned int)lib->kva, (unsigned int)lib->paddr, (unsigned int)lib->dyn);

    /* Process dynamic entries */
    if (lib->dyn->d_tag == DT_HASH)
        hash_off = lib->dyn->d_un.d_val;

    for (dyn = lib->dyn + 1; dyn->d_tag != DT_NULL; dyn++) {
        switch (dyn->d_tag) {
        case DT_STRTAB:
            lib->strtab = (char *)lib->kva + dyn->d_un.d_val;
            break;
        case DT_SYMTAB:
            dynsym_off = dyn->d_un.d_val;
            lib->symtab = (Elf_Sym *)((char *)lib->kva + dyn->d_un.d_val);
            break;
        case DT_RELA:
            dyn_off = dyn->d_un.d_val;
            break;
        }
    }
    if (!lib->strtab || !lib->symtab) {
        PRINT_TNPU_ERROR("[SHLIB] missing STRTAB or SYMTAB: lib='%s'\n", name);
        PRINT_TNPU_ERROR("[SHLIB] Check linker output.\n");
        goto get_lib_error;
    }

    if (hash_off && hash_off <= load_end - 2 * sizeof(unsigned int))
        sym_num = ((unsigned int *)((char *)lib->kva + hash_off))[1];

    if (!sym_num && dynsym_off && ehdr.e_shoff && ehdr.e_shnum) {
        for (i = 0; i < ehdr.e_shnum; i++) {
            ret = tnpu.libread(elf, vaddr,
                    ehdr.e_shoff + (i * sizeof(struct elf_shdr)),
                    (char *)&shdr, sizeof(struct elf_shdr));
            if (ret < 0) {
                PRINT_TNPU_ERROR("[SHLIB] failed to read section headers: lib='%s'\n", name);
                PRINT_TNPU_ERROR("[SHLIB] Check file completeness and ELF format.\n");
                goto get_lib_error;
            }
            if (shdr.sh_type == SHT_DYNSYM && shdr.sh_addr == dynsym_off) {
                if (!shdr.sh_entsize || (shdr.sh_size % shdr.sh_entsize)) {
                    PRINT_TNPU_ERROR("[SHLIB] invalid DYNSYM section: lib='%s'\n", name);
                    PRINT_TNPU_ERROR("[SHLIB] Check linker output.\n");
                    goto get_lib_error;
                }
                sym_num = shdr.sh_size / shdr.sh_entsize;
                break;
            }
        }
    }

    if (!sym_num) {
        PRINT_TNPU_ERROR("[SHLIB] failed to get symbol count: lib='%s'\n", name);
        PRINT_TNPU_ERROR("[SHLIB] Check DT_HASH and DYNSYM section headers.\n");
        goto get_lib_error;
    }

    lib->sym_num = sym_num;

    if (vaddr == 0) {
        filp_close(elf, NULL);
        elf = NULL;
    }

    /* Handle dependencies and relocations */
    lib->need_lib_num = 0;
    if (dyn_off) {
        unsigned int relasz = 0;
        unsigned int relaend = 0;
        Elf32_Rela *rela = (Elf_Rela *)((char *)lib->kva + dyn_off);

        for (dyn = lib->dyn; dyn->d_tag != DT_NULL; dyn++) {
            switch (dyn->d_tag) {
            case DT_NEEDED:
                if (lib->need_lib_num >= ARRAY_SIZE(lib->need_lib)) {
                    PRINT_TNPU_ERROR("[SHLIB] dependency limit exceeded: lib='%s', max=%u\n",
                            lib->name, (unsigned int)ARRAY_SIZE(lib->need_lib));
                    PRINT_TNPU_ERROR("[SHLIB] Reduce DT_NEEDED count or update the driver limit.\n");
                    goto get_lib_error;
                }
                PRINT_TNPU_DEBUG("%s depends on %s\n",
                        lib->name, lib->strtab + dyn->d_un.d_val);
                lib->need_lib[lib->need_lib_num] =
                        tnpu_get_lib(lib->strtab + dyn->d_un.d_val, 0);
                if (!lib->need_lib[lib->need_lib_num]) {
                    PRINT_TNPU_ERROR("[SHLIB] dependency load failed: parent='%s', need='%s'\n",
                            lib->name, lib->strtab + dyn->d_un.d_val);
                    PRINT_TNPU_ERROR("[SHLIB] Check dependent TNPU libraries in the current directory.\n");
                    goto get_lib_error;
                }
                lib->need_lib_num++;
                break;
            case DT_RELASZ:
                relasz = dyn->d_un.d_val;
                break;
            case DT_RELAENT:
                relaend = dyn->d_un.d_val;
                break;
            }
        }

        if (relaend) {
            unsigned int rela_num = relasz / relaend;
            for (i = 0; i < rela_num; i++) {
                ret = tnpu_process_relocations(lib, rela, i);
                if (ret) {
                    goto get_lib_error;
                }
            }
        }
    }

    /* Add to library list */
    list_add_tail(&lib->node, &tnpu.sharelib_list);

    PRINT_TNPU_DEBUG("Loaded %s, pa=0x%x, size=%d\n",
            lib->name, lib->paddr, lib->size);
    return lib->paddr;

get_lib_error:
    if (lib) {
        for (i = 0; i < lib->need_lib_num; i++) {
            if (lib->need_lib[i]) {
                tnpu_free_lib(lib->need_lib[i]);
            }
        }
        if (lib->kva) {
            tnpu_cma_free(lib->size, lib->kva, lib->paddr);
        }
        kfree(lib);
    }
    if (elf) {
        fput(elf);
    }
    return 0;
}


int ioctl_tnpu_sharelib_load(unsigned long arg)
{
    tnpu_ioctl_sharelib_t load_lib;
    struct tnpu_libref *libref;

    if (copy_from_user(&load_lib, (void __user *)arg, sizeof(load_lib))) {
        PRINT_TNPU_ERROR("[SHLIB] load request copy failed.\n");
        PRINT_TNPU_ERROR("[SHLIB] Check caller buffer and access permissions.\n");
        return -EFAULT;
    }
    load_lib.name[TNPU_NAME_MAX - 1] = '\0';

    libref = kmalloc(sizeof(*libref), GFP_KERNEL);
    if (!libref) {
        PRINT_TNPU_ERROR("[SHLIB] load request allocation failed: lib='%s'\n",
                load_lib.name);
        PRINT_TNPU_ERROR("[SHLIB] Check kernel memory usage.\n");
        return -ENOMEM;
    }

    mutex_lock(&tnpu.load_mutex);

    libref->paddr = tnpu_get_lib(load_lib.name, load_lib.uvaddr);
    if (libref->paddr == 0) {
        PRINT_TNPU_ERROR("[SHLIB] load failed: lib='%s'\n", load_lib.name);
        PRINT_TNPU_ERROR("[SHLIB] Check previous [SHLIB] errors for details.\n");
        mutex_unlock(&tnpu.load_mutex);
        kfree(libref);
        return -EFAULT;
    }
    libref->pid = task_tgid_nr(current);

    list_add_tail(&libref->node, &tnpu.libref_list);

    load_lib.paddr = libref->paddr;

    if (copy_to_user((void __user *)arg, &load_lib, sizeof(load_lib))) {
        PRINT_TNPU_ERROR("[SHLIB] load result copy failed: lib='%s'\n",
                load_lib.name);
        PRINT_TNPU_ERROR("[SHLIB] Check user buffer and caller memory.\n");
        tnpu_free_lib(libref->paddr);
        list_del(&libref->node);
        mutex_unlock(&tnpu.load_mutex);
        kfree(libref);
        return -EFAULT;
    }
    mutex_unlock(&tnpu.load_mutex);
    return 0;
}

int ioctl_tnpu_sharelib_unload(unsigned long arg)
{
    unsigned int lib_paddr;
    unsigned int pid = task_tgid_nr(current);
    struct tnpu_libref *libref;

    if (copy_from_user(&lib_paddr, (void __user *)arg, sizeof(unsigned int))) {
        PRINT_TNPU_ERROR("Failed to copy from user\n");
        return -EFAULT;
    }

    mutex_lock(&tnpu.load_mutex);
    list_for_each_entry_reverse(libref, &tnpu.libref_list, node) {
        if (libref->paddr == lib_paddr && libref->pid == pid) {
            tnpu_free_lib(lib_paddr);
            list_del(&libref->node);
            kfree(libref);
            break;
        }
    }
    mutex_unlock(&tnpu.load_mutex);

    return 0;
}

void tnpu_show_sharelib(void)
{
    struct tnpu_sharelib *lib;
    unsigned int total_size = 0;
    unsigned int lib_count = 0;

    PRINT_TNPU_INFO("TNPU Shared Libraries:\n");

    mutex_lock(&tnpu.load_mutex);
    if (list_empty(&tnpu.sharelib_list)) {
        PRINT_TNPU_INFO("  (No shared libraries)\n");
    } else {
        PRINT_TNPU_INFO("%-12s %-12s %-10s %s\n", "KVA", "PAddr", "Size", "Name");
    list_for_each_entry(lib, &tnpu.sharelib_list, node) {
        PRINT_TNPU_INFO("%-12p 0x%-10x %-10x %s\n",
                lib->kva, lib->paddr, lib->size, lib->name);
        total_size += lib->size;
        lib_count++;
    }
        PRINT_TNPU_INFO("Total: %u libraries, %u bytes\n", lib_count, total_size);
    }
    mutex_unlock(&tnpu.load_mutex);
}

int ioctl_tnpu_job_submit(unsigned long arg)
{
    tnpu_ioctl_submit_t submit;

    unsigned long irqflags;
    unsigned int curr_id, done_id, chn;
    unsigned int curr_pid = task_tgid_nr(current);
    PRINT_TNPU_DEBUG("Task submit,pid=%d\n", curr_pid);

#ifdef CONFIG_TNPU_SCHEDUL_DEBUG
    volatile unsigned int submit1_time_l, submit1_time_h;
    volatile unsigned int submit2_time_l, submit2_time_h;
    submit1_time_l = *TNPU_CCU_TIME_L;
    submit1_time_h = *TNPU_CCU_TIME_H;
#endif // CONFIG_TNPU_SCHEDUL_DEBUG

    if (copy_from_user(&submit, (void __user *)arg, sizeof(submit))) {
        PRINT_TNPU_ERROR("Failed to copy from user.\n");
        return -EFAULT;
    }

    if (submit.ctl & TNPU_JOB_DELAY) {
        int i;
        for (i = 0; i < TNPU_JOB_DELAY_DENSITY; i++) {
            done_id = *TNPU_DRAM_SHM_TASK_DONE_ID;
            if (done_id == *TNPU_DRAM_SHM_TASK_MAX_ID)
                break;
            PRINT_TNPU_DEBUG("Task delay,done_id=0x%x,pid=%d\n", done_id, curr_pid);
            tnpu_wait(done_id, 0);
        }
    }

    spin_lock_irqsave(&tnpu.task_spin, irqflags);

    curr_id = *TNPU_DRAM_SHM_TASK_MAX_ID;
    *TNPU_DRAM_SHM_TASK_MAX_ID = curr_id + 1;
    chn = curr_id % TNPU_MAX_TASK;

    done_id = *TNPU_DRAM_SHM_TASK_DONE_ID;
    if (TNPU_MAX_TASK < (curr_id - done_id)) {
        if (65535 > (curr_id - done_id)) {
            // Avoid integer overflow
            spin_unlock_irqrestore(&tnpu.task_spin, irqflags);
            PRINT_TNPU_DEBUG("Task wait,curr_id=0x%x,done_id=%d\n", curr_id, done_id);
            tnpu_wait(curr_id - TNPU_MAX_TASK + 2, 0);
            spin_lock_irqsave(&tnpu.task_spin, irqflags);
            done_id = *TNPU_DRAM_SHM_TASK_DONE_ID;
        }
    }

#ifdef CONFIG_TNPU_DUTY_RATIO_DEBUG
    if (tnpu.duty_status == 2) {
        tnpu.start_timer = ktime_get();
        tnpu.duty_status = 3;
    }
#endif // CONFIG_TNPU_DUTY_RATIO_DEBUG

    *TNPU_DRAM_TASK_ID(chn) = curr_id;
    *TNPU_DRAM_TASK_PID(chn) = curr_pid;
    *TNPU_DRAM_TASK_PC(chn) = submit.pc;
    *TNPU_DRAM_TASK_ARG0(chn) = submit.arg[0];
    *TNPU_DRAM_TASK_ARG1(chn) = submit.arg[1];
    *TNPU_DRAM_TASK_ARG2(chn) = submit.arg[2];
    *TNPU_DRAM_TASK_ARG3(chn) = submit.arg[3];
    *TNPU_DRAM_TASK_PID(*TNPU_DRAM_SHM_TASK_MAX_ID % TNPU_MAX_TASK) = 0;

    if (!(submit.ctl & TNPU_KEEP_HOST_CACHE)) {
        /*
         * The vendor blasts L1 when CP0 ErrCtl bit 29 says L1 and L2 are
         * connected, and otherwise syncs the uncached DRAM window, which
         * writes nothing back. Writing back L1 always covers both.
         */
        r4k_blast_dcache();
    }
    

#ifdef CONFIG_TNPU_SCHEDUL_DEBUG
    if (*TNPU_DRAM_SHM_RUN_STATUS == 0) {
        *TNPU_DRAM_SHM_RUN_STATUS = 1;
        submit2_time_l = *TNPU_CCU_TIME_L;
        submit2_time_h = *TNPU_CCU_TIME_H;
        *TNPU_CCU_FROMHOST = 1;
        spin_unlock_irqrestore(&tnpu.task_spin, irqflags);
        PRINT_TNPU_INFO("Task run,curr_id=%d,done_id=%d,pc=0x%x,timer1=%lld,timer2=%lld\n",
                curr_id, done_id, *TNPU_DRAM_TASK_PC(chn),
                (unsigned long long)submit1_time_h << 32 | submit1_time_l,
                (unsigned long long)submit2_time_h << 32 | submit2_time_l);
    } else {
        spin_unlock_irqrestore(&tnpu.task_spin, irqflags);
        submit2_time_l = *TNPU_CCU_TIME_L;
        submit2_time_h = *TNPU_CCU_TIME_H;
        PRINT_TNPU_INFO("Task wait,curr_id=%d,done_id=%d,status=%d,timer1=%lld,timer2=%lld\n",
                curr_id, done_id, *TNPU_DRAM_SHM_RUN_STATUS,
                (unsigned long long)submit1_time_h << 32 | submit1_time_l,
                (unsigned long long)submit2_time_h << 32 | submit2_time_l);
    }
#else
    if (*TNPU_DRAM_SHM_RUN_STATUS == 0) {
        *TNPU_DRAM_SHM_RUN_STATUS = 1;
        PRINT_TNPU_DEBUG("Task run,curr_id=0x%x,pc=0x%x,pid=%d\n",
                        curr_id, submit.pc, curr_pid);
        *TNPU_CCU_FROMHOST = 1;
    }
    spin_unlock_irqrestore(&tnpu.task_spin, irqflags);
#endif // CONFIG_TNPU_SCHEDUL_DEBUG

    submit.id = curr_id;
    if (copy_to_user(&((tnpu_ioctl_submit_t __user *)arg)->id,
            &submit.id, sizeof(unsigned int))) {
        PRINT_TNPU_ERROR("Failed to copy from user.\n");
        tnpu_wait(curr_id, 0);
        return -EFAULT;
    }

    if (submit.ctl & TNPU_JOB_BLOCK) {
        return tnpu_wait(curr_id, 0);
    }

    return 0;
}

void tnpu_duty_ratio_init(void)
{
#ifdef CONFIG_TNPU_DUTY_RATIO_DEBUG
    unsigned long irqflags;
    ktime_t now = ktime_get();

    tnpu.basic_timer = now;
    tnpu.run_time = 0;
    spin_lock_irqsave(&tnpu.task_spin, irqflags);
    if (*TNPU_DRAM_SHM_RUN_STATUS == 0) {
        tnpu.start_timer = ktime_set(0, 0);
        tnpu.duty_status = 2;
    }
    else {
        tnpu.start_timer = now;
        tnpu.duty_status = 3;
    }
    spin_unlock_irqrestore(&tnpu.task_spin, irqflags);
#endif // CONFIG_TNPU_DUTY_RATIO_DEBUG
}

void tnpu_duty_ratio_show(unsigned long *all, unsigned long *run)
{
#ifdef CONFIG_TNPU_DUTY_RATIO_DEBUG
    unsigned long irqflags;
    ktime_t now = ktime_get();

    spin_lock_irqsave(&tnpu.task_spin, irqflags);
    *all = ktime_to_us(ktime_sub(now, tnpu.basic_timer));
    tnpu.basic_timer = now;
    if (tnpu.duty_status != 3) {
        *run = tnpu.run_time;
    } else {
        *run = tnpu.run_time +
                ktime_to_us(ktime_sub(now, tnpu.start_timer));
        tnpu.start_timer = now;
    }
    tnpu.run_time = 0;
    spin_unlock_irqrestore(&tnpu.task_spin, irqflags);
#else
    *run = 0;
#endif // CONFIG_TNPU_DUTY_RATIO_DEBUG
}

void tnpu_duty_ratio_stop(void)
{
#ifdef CONFIG_TNPU_DUTY_RATIO_DEBUG
    unsigned long irqflags;

    spin_lock_irqsave(&tnpu.task_spin, irqflags);
    tnpu.duty_status = 0;
    spin_unlock_irqrestore(&tnpu.task_spin, irqflags);
#endif // CONFIG_TNPU_DUTY_RATIO_DEBUG
}

void tnpu_kill_job(pid_t pid)
{
    unsigned long irqflags;
    unsigned int i, run_id, run_chn;
    struct tnpu_libref *libref, *tmp_libref;

    // job
    spin_lock_irqsave(&tnpu.task_spin, irqflags);
    run_id = *TNPU_DRAM_SHM_TASK_DONE_ID;
    run_chn = run_id % TNPU_MAX_TASK;
    for (i = 0; i < TNPU_MAX_TASK; i++) {
        if (*TNPU_DRAM_TASK_PID(i) != pid)
            continue;
        *TNPU_DRAM_TASK_PC(i) = 0;
        *TNPU_DRAM_TASK_PID(i) = 0;
        if (i == run_chn && *TNPU_DRAM_SHM_RUN_STATUS) {
            int cnt = 0;
            *TNPU_CCU_BECR |= 1;
            do {
                cnt++;
                udelay(1);
                if (cnt > TNPU_POLL_US) {
                    PRINT_TNPU_ERROR("Can not kill job.\n");
                    PRINT_TNPU_ERROR("TNPU is no longer running, please reboot!\n");
                    spin_unlock_irqrestore(&tnpu.task_spin, irqflags);
                    return;
                }
            } while((*TNPU_CCU_BECR & 6) != 6);
            *TNPU_CCU_SOFT_RESET = CCU_SOFT_RESET_CMD_SET;
            *TNPU_CCU_BECR &= ~(1 << 0);
            *TNPU_CCU_TOHOST = 0;

            cnt = 0;
            ndelay(20);
            *TNPU_CCU_SOFT_RESET = CCU_SOFT_RESET_CMD_RELEASE;
            do {
                udelay(1);
                cnt++;
                if (cnt > TNPU_POLL_US) {
                    PRINT_TNPU_ERROR("Can not reset tnpu.\n");
                    PRINT_TNPU_ERROR("TNPU is no longer running, please reboot!\n");
                    break;
                }
            }while(!(*TNPU_CCU_CCSR & TNPU_CCU_CCSR_SLEEP_MASK));
            run_id++;
            *TNPU_DRAM_SHM_TASK_DONE_ID = run_id;
            wake_up_all(&tnpu.wait_queue);
            if (run_id < *TNPU_DRAM_SHM_TASK_MAX_ID) {
                *TNPU_CCU_FROMHOST = 1;
            } else {
                *TNPU_DRAM_SHM_RUN_STATUS = 0;
            }
        }
    }
    spin_unlock_irqrestore(&tnpu.task_spin, irqflags);

    // lib
    mutex_lock(&tnpu.load_mutex);
    list_for_each_entry_safe_reverse(libref, tmp_libref, &tnpu.libref_list, node) {
        if (libref->pid == pid) {
            tnpu_free_lib(libref->paddr);
            list_del(&libref->node);
            kfree(libref);
        }
    }
    mutex_unlock(&tnpu.load_mutex);
}
