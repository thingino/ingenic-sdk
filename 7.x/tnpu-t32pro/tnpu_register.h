/*
 * Ingenic TNPU ioctl
 *
 * Copyright (c) 2023 LiuTianyang
 *
 * This file is released under the GPLv2
*/
#ifndef __TNPU_REGISTER_H__
#define __TNPU_REGISTER_H__

#define TNPU_CCU_PADDR				(0x12700000)
#define TNPU_CVA_PADDR				(0x15030000)
#define TNPU_ORAM_DMA_PADDR			(0x15050000)
#define TNPU_LUT_PADDR				(0x15060000)
#define TNPU_NNA_PADDR				(0x15070000)
#define TNPU_DRAM_PADDR				(0x150b0000)
#define TNPU_SIMD_DMA_PADDR			(0x150c0000)
#define TNPU_ORAM_PADDR				(0x15100000)

/* TNPU_CCU */
#define TNPU_CCU_SIZE                           (0x4000)

#define TNPU_CCU_BASE(_d) \
	((volatile unsigned int *)((unsigned int)tnpu.ccu_regs + (_d)))

/* TNPU_DRAM */
#ifdef TNPU_T32
#define TNPU_DRAM_SIZE                          (0x1000)
#else
#define TNPU_DRAM_SIZE                          (0x2000)
#endif

#define TNPU_DRAM_BASE(_d) \
	((volatile unsigned int *)((unsigned int)tnpu.dram_regs + (_d)))

/*  TNPU_OTHRE  */
#define TNPU_IRQ_NUM                            (0 + 0x8)

/*  TNPU_ORAM  */
#define TNPU_ORAM_SIZE                          (128 * 1024)

/* TNPU_CCU */
#define TNPU_CCU_CCSR                           TNPU_CCU_BASE(0x00)
#define TNPU_CCU_TIME_L                         TNPU_CCU_BASE(0x10)
#define TNPU_CCU_TIME_H                         TNPU_CCU_BASE(0x14)
#define TNPU_CCU_TIME_CMP_L                     TNPU_CCU_BASE(0x18)
#define TNPU_CCU_TIME_CMP_H                     TNPU_CCU_BASE(0x1c)
#define TNPU_CCU_INTC_MASK_H                    TNPU_CCU_BASE(0x24)
#define TNPU_CCU_INTC_PENDING                   TNPU_CCU_BASE(0x2c)
#define TNPU_CCU_ERROR_EPC                      TNPU_CCU_BASE(0x30)
#define TNPU_CCU_INDEX                          TNPU_CCU_BASE(0x34)
#define TNPU_CCU_UMPID                          TNPU_CCU_BASE(0x38)
#define TNPU_CCU_PMA_ADDRESS(_x)                TNPU_CCU_BASE(0x40 + (_x * 4))
#define TNPU_CCU_PMA_CONFIG(_x)                 TNPU_CCU_BASE(0x60 + (_x * 4))
#define TNPU_CCU_ERROR_STATUS                   TNPU_CCU_BASE(0x80)
#define TNPU_CCU_NNA_INT_TYPE                   TNPU_CCU_BASE(0x84)
#define TNPU_CCU_MCSR                           TNPU_CCU_BASE(0x88)
#define TNPU_CCU_SOFT_RESET                     TNPU_CCU_BASE(0xf0)
#define TNPU_CCU_CSCR                           TNPU_CCU_BASE(0xfc)
#define TNPU_CCU_BECR                           TNPU_CCU_BASE(0x100)
#define TNPU_CCU_AXIQOS                         TNPU_CCU_BASE(0x104)

/* TNPU_CCU CCSR */
#define TNPU_CCU_CCSR_SLEEP_WIDTH               (1)
#define TNPU_CCU_CCSR_SLEEP_SHIFT               (3)
#define TNPU_CCU_CCSR_SLEEP_MASK                \
	(TNPU_CCU_CCSR_SLEEP_WIDTH << TNPU_CCU_CCSR_SLEEP_SHIFT)

#define TNPU_CCU_CCSR_V0_SLEEP_WIDTH           (1)
#define TNPU_CCU_CCSR_V0_SLEEP_SHIFT           (6)
#define TNPU_CCU_CCSR_V0_SLEEP_MASK            \
	(TNPU_CCU_CCSR_V0_SLEEP_WIDTH << TNPU_CCU_CCSR_V0_SLEEP_SHIFT)

#define TNPU_CCU_CCSR_V1_SLEEP_WIDTH           (1)
#define TNPU_CCU_CCSR_V1_SLEEP_SHIFT           (7)
#define TNPU_CCU_CCSR_V1_SLEEP_MASK            \
	(TNPU_CCU_CCSR_V1_SLEEP_WIDTH << TNPU_CCU_CCSR_V1_SLEEP_SHIFT)

/* TNPU_CCU SOFT RESET */
#define TNPU_CCU_SOFT_RESET_CMD_SET            (0xc0000000)
#define TNPU_CCU_SOFT_RESET_CMD_RELEASE        (0x0)
#define TNPU_CCU_SOFT_RESET_CMD_RELEASE_V0     (0x40000000)
#define TNPU_CCU_SOFT_RESET_CMD_RELEASE_V1     (0x80000000)

/* TNPU_CCU BECR */
#define TNPU_CCU_BECR_SET_WIDTH                (1)
#define TNPU_CCU_BECR_SET_SHIFT                (0)
#define TNPU_CCU_BECR_SET_MASK                 \
	(TNPU_CCU_BECR_SET_WIDTH << TNPU_CCU_BECR_SET_SHIFT)

#define TNPU_CCU_BECR_V0_IDLE_WIDTH            (1)
#define TNPU_CCU_BECR_V0_IDLE_SHIFT            (1)
#define TNPU_CCU_BECR_V0_IDLE_MASK             \
	(TNPU_CCU_BECR_V0_IDLE_WIDTH << TNPU_CCU_BECR_V0_IDLE_SHIFT)

#define TNPU_CCU_BECR_ORAM_IDLE_WIDTH          (1)
#define TNPU_CCU_BECR_ORAM_IDLE_SHIFT          (2)
#define TNPU_CCU_BECR_ORAM_IDLE_MASK           \
	(TNPU_CCU_BECR_ORAM_IDLE_WIDTH << TNPU_CCU_BECR_ORAM_IDLE_SHIFT)

#define TNPU_CCU_BECR_V1_IDLE_WIDTH            (1)
#define TNPU_CCU_BECR_V1_IDLE_SHIFT            (4)
#define TNPU_CCU_BECR_V1_IDLE_MASK             \
	(TNPU_CCU_BECR_V1_IDLE_WIDTH << TNPU_CCU_BECR_V1_IDLE_SHIFT)

#define TNPU_CCU_BECR_ALL_IDLE_MASK            \
	(TNPU_CCU_BECR_V0_IDLE_MASK | TNPU_CCU_BECR_ORAM_IDLE_MASK | TNPU_CCU_BECR_V1_IDLE_MASK)


/* TNPU_CCU OTHER */
#ifdef TNPU_T32
#define TNPU_CCU_CORE_OFF                     (0x0)
#else
#define TNPU_CCU_CORE_OFF                     (0x1000)
#endif
#define TNPU_CCU_CRER(sel)                    TNPU_CCU_BASE(((sel + 1) * TNPU_CCU_CORE_OFF) + 0x4)
#define TNPU_CCU_FROMHOST(sel)                TNPU_CCU_BASE(((sel + 1) * TNPU_CCU_CORE_OFF) + 0x8)
#define TNPU_CCU_TOHOST(sel)                  TNPU_CCU_BASE(((sel + 1) * TNPU_CCU_CORE_OFF) + 0xc)
#define TNPU_CCU_LOCK(n)                      TNPU_CCU_BASE(0x0200 + ((n) * 4))


/* TNPU_DRAM */
#define TNPU_DRAM_TASK                        (TNPU_DRAM_BASE(0))
#define TNPU_DRAM_TASK_NUM                    (2)
#define TNPU_DRAM_TASK_UNIT_SIZE              (32 * 2) // 2 cacheline
#define TNPU_DRAM_TASK_SIZE                   (TNPU_DRAM_TASK_UNIT_SIZE * TNPU_DRAM_TASK_NUM)
#define TNPU_DRAM_TASK_TOTAL_SIZE             (TNPU_DRAM_TASK_SIZE * 2) // v0+v1
#define TNPU_DRAM_TASK_UNIT(sel, chn, pos) \
	(TNPU_DRAM_BASE((TNPU_DRAM_TASK_SIZE * (sel)) + (TNPU_DRAM_TASK_UNIT_SIZE * (chn)) + ((pos) * 4)))

#define TNPU_DRAM_TASK_ID(sel, chn)           TNPU_DRAM_TASK_UNIT(sel, chn, 0)
#define TNPU_DRAM_TASK_DEVICE(sel, chn)       TNPU_DRAM_TASK_UNIT(sel, chn, 1)
#define TNPU_DRAM_TASK_PC(sel, chn)           TNPU_DRAM_TASK_UNIT(sel, chn, 2)
#define TNPU_DRAM_TASK_ARG0(sel, chn)         TNPU_DRAM_TASK_UNIT(sel, chn, 3)
#define TNPU_DRAM_TASK_ARG1(sel, chn)         TNPU_DRAM_TASK_UNIT(sel, chn, 4)
#define TNPU_DRAM_TASK_ARG2(sel, chn)         TNPU_DRAM_TASK_UNIT(sel, chn, 5)
#define TNPU_DRAM_TASK_ARG3(sel, chn)         TNPU_DRAM_TASK_UNIT(sel, chn, 6)
#define TNPU_DRAM_TASK_START_TIME_L(sel, chn) TNPU_DRAM_TASK_UNIT(sel, chn, 7)
#define TNPU_DRAM_TASK_START_TIME_H(sel, chn) TNPU_DRAM_TASK_UNIT(sel, chn, 8)
#define TNPU_DRAM_TASK_STOP_TIME_L(sel, chn)  TNPU_DRAM_TASK_UNIT(sel, chn, 9)
#define TNPU_DRAM_TASK_STOP_TIME_H(sel, chn)  TNPU_DRAM_TASK_UNIT(sel, chn, 10)
#define TNPU_DRAM_TASK_ORAM_ADDR(sel, chn)    TNPU_DRAM_TASK_UNIT(sel, chn, 11)
#define TNPU_DRAM_TASK_END(sel, chn)          TNPU_DRAM_TASK_UNIT(sel, chn, 15)

#define TNPU_DRAM_BOOT_SIZE                   (0x400)
#define TNPU_DRAM_BOOT_KDATA                  (0x50)
#define TNPU_DRAM_BOOT_START                  TNPU_DRAM_BASE(TNPU_DRAM_TASK_TOTAL_SIZE)
#define TNPU_DRAM_BOOT_KDATA_OFFSET           (TNPU_DRAM_TASK_TOTAL_SIZE + TNPU_DRAM_BOOT_KDATA)
#define TNPU_DRAM_BOOT_KDATA_V0_QUEUE_INDEX   TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 0))
#define TNPU_DRAM_BOOT_KDATA_V0_CCU_BASE      TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 1))
#define TNPU_DRAM_BOOT_KDATA_V0_TASK_BASE     TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 2))
#define TNPU_DRAM_BOOT_KDATA_V0_SP            TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 3))
#define TNPU_DRAM_BOOT_KDATA_V1_QUEUE_INDEX   TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 4))
#define TNPU_DRAM_BOOT_KDATA_V1_CCU_BASE      TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 5))
#define TNPU_DRAM_BOOT_KDATA_V1_TASK_BASE     TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 6))
#define TNPU_DRAM_BOOT_KDATA_V1_SP            TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 7))
#define TNPU_DRAM_BOOT_KDATA_UART_BASE        TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 8))
#define TNPU_DRAM_BOOT_KDATA_VERSION_BASE     TNPU_DRAM_BASE(TNPU_DRAM_BOOT_KDATA_OFFSET + (4 * 9))

/* OTHER */
#define TNPU_V0_SP_TOP                        (TNPU_DRAM_PADDR + TNPU_DRAM_SIZE)
#define TNPU_V1_SP_SIZE                       (0x1000)

#define CCU_SOFT_RESET_CMD_SET                (0xc0000000)
#define CCU_SOFT_RESET_CMD_RELEASE            (0)

#define TNPU_TOHOST_RET_DONE                  1
#define TNPU_TOHOST_RET_EXL                   2
#define TNPU_TOHOST_RET_CLEAR                 3
#define TNPU_TOHOST_RET_STACK                 4
#define TNPU_TOHOST_RET_INTC                  5

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
