/*
 * Ingenic TNPU ioctl
 *
 * Copyright (c) 2023 LiuTianyang
 *
 * This file is released under the GPLv2
*/
#ifndef __TNPU_REGISTER_H__
#define __TNPU_REGISTER_H__

/* TNPU_CCU */
#define TNPU_CCU_PADDR               (0x12700000)
#define TNPU_CCU_SIZE                (0x1000)

#define TNPU_CCU_BASE(_d) \
    ((volatile unsigned int *)((unsigned int)tnpu.ccu_regs + _d))

/* TNPU_DRAM */
#define TNPU_DRAM_PADDR              (0x12608000)
#define TNPU_DRAM_SIZE               (0x1000)

#define TNPU_DRAM_BASE(_d) \
    ((volatile unsigned int *)((unsigned int)tnpu.dram_regs + _d))

/*  TNPU_OTHRE  */
#define TNPU_IRQ_NUM (0 + 0x8)


/* TNPU_CCU */
#define TNPU_CCU_CCSR                TNPU_CCU_BASE(0x00)
#define TNPU_CCU_CRER                TNPU_CCU_BASE(0x04)
#define TNPU_CCU_FROMHOST            TNPU_CCU_BASE(0x08)
#define TNPU_CCU_TOHOST              TNPU_CCU_BASE(0x0c)
#define TNPU_CCU_TIME_L              TNPU_CCU_BASE(0x10)
#define TNPU_CCU_TIME_H              TNPU_CCU_BASE(0x14)
#define TNPU_CCU_TIME_CMP_L          TNPU_CCU_BASE(0x18)
#define TNPU_CCU_TIME_CMP_H          TNPU_CCU_BASE(0x1c)
#define TNPU_CCU_ERROR_EPC           TNPU_CCU_BASE(0x30)
#define TNPU_CCU_INDEX               TNPU_CCU_BASE(0x34)
#define TNPU_CCU_SOFT_RESET          TNPU_CCU_BASE(0xf0)
#define TNPU_CCU_LOCK                TNPU_CCU_BASE(0xf4)
#define TNPU_CCU_UNLOCK              TNPU_CCU_BASE(0xf8)
#define TNPU_CCU_CSCR                TNPU_CCU_BASE(0xfc)
#define TNPU_CCU_BECR                TNPU_CCU_BASE(0x100)
#define TNPU_CCU_AXIQOS              TNPU_CCU_BASE(0x104)

/* TNPU_DRAM */
#define TNPU_DRAM_BOOT_SIZE          (0x240)
#define TNPU_DRAM_SHM_SIZE           (0x1c0)
#define TNPU_TASK_SIZE               (44)
#define TNPU_STACK_MAJIC             (0xfedcba98)
#define TNPU_DRAM_BOOT_START         TNPU_DRAM_BASE(0x0)
#define TNPU_DRAM_SHM_START          TNPU_DRAM_BASE(TNPU_DRAM_BOOT_SIZE)
#define TNPU_DRAM_SHM_END \
    TNPU_DRAM_BASE(TNPU_DRAM_BOOT_SIZE + TNPU_DRAM_SHM_SIZE)

#define TNPU_DRAM_SHM_TASK_DONE_ID   TNPU_DRAM_BASE(TNPU_DRAM_BOOT_SIZE + 0)
#define TNPU_DRAM_SHM_RUN_STATUS     TNPU_DRAM_BASE(TNPU_DRAM_BOOT_SIZE + 4)
#define TNPU_DRAM_SHM_TASK_MAX_ID    TNPU_DRAM_BASE(TNPU_DRAM_BOOT_SIZE + 8)

#define TNPU_DRAM_SHM_TASK_START     (TNPU_DRAM_BOOT_SIZE + 12)
#define TNPU_DRAM_TASK_ID(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 0)
#define TNPU_DRAM_TASK_PID(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 4)
#define TNPU_DRAM_TASK_PC(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 8)
#define TNPU_DRAM_TASK_ARG0(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 12)
#define TNPU_DRAM_TASK_ARG1(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 16)
#define TNPU_DRAM_TASK_ARG2(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 20)
#define TNPU_DRAM_TASK_ARG3(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 24)
#define TNPU_DRAM_TASK_START_TIME_L(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 28)
#define TNPU_DRAM_TASK_START_TIME_H(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 32)
#define TNPU_DRAM_TASK_STOP_TIME_L(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 36)
#define TNPU_DRAM_TASK_STOP_TIME_H(task_chn) \
    TNPU_DRAM_BASE(TNPU_DRAM_SHM_TASK_START + (task_chn * TNPU_TASK_SIZE) + 40)


/* OTHER */
#define TNPU_CCU_CCSR_SLEEP_WIDTH    (1)
#define TNPU_CCU_CCSR_SLEEP_SHIFT    (3)
#define TNPU_CCU_CCSR_SLEEP_MASK     \
    (TNPU_CCU_CCSR_SLEEP_WIDTH << TNPU_CCU_CCSR_SLEEP_SHIFT)

#define CCU_SOFT_RESET_CMD_SET       (0x80000000)
#define CCU_SOFT_RESET_CMD_RELEASE   (0)

#define TNPU_TOHOST_RET_DONE         1
#define TNPU_TOHOST_RET_EXL          2
#define TNPU_TOHOST_RET_CLEAR        3
#define TNPU_TOHOST_RET_TNNA         4
#define TNPU_TOHOST_RET_CVA          5
#define TNPU_TOHOST_RET_STACK        6

//inline void tnpu_clk(int status)
//{
//    volatile unsigned int *cpm_clkgr1 =
//        (volatile unsigned int *)(0xb0000000 + 0x28);
//    volatile unsigned int *cpm_tnpucdr =
//        (volatile unsigned int *)(0xb0000000 + 0x98);
//
//    if (status) {
//        // tnpu_cclk：tnpu_hclk = 2: 1
//        *cpm_tnpucdr = (*cpm_tnpucdr & 0xFFFFFF00) | 0x01;
//
//        // open tnpu_clk
//        *cpm_clkgr1 &= ~(1 << 11);
//        *cpm_tnpucdr |= ((1 << 29) | 1);
//    } else {
//        // close tnpu_clk
//        *cpm_clkgr1 |= (1 << 11);
//    }
//}

#endif // __TNPU_REGISTER_H__
