/****************************************************************
*****************************************************************/
#ifndef __HERA_H__
#define __HERA_H__

#ifdef JZM_HUNT_SIM
#include "hunt.h"
#else
#ifndef __place_k0_data__
#define __place_k0_data__
#endif
#ifndef __place_k0_text__
#define __place_k0_text__
#endif
#endif
#define T32V_VPU_NUM 1 //ttt
#define read_reg(reg, val) (*((volatile uint32_t *)(reg)))//ttt
#define write_reg(reg, val) (*((volatile uint32_t *)(reg)) = (val))//ttt

#define USE_HERA_MCE

#define CPM_RADIX_SR(ID)		(31-(ID))
#define CPM_RADIX_STP(ID)		(30-(ID))
#define CPM_RADIX_ACK(ID)		(29-(ID))

#define VPU_BASE             0            // force to 0, for we use ralative addr in this driver, this is importent
#define RADIX_BASE           VPU_BASE

/*************************************************************************************
  HERA SLAVE BASE
 *************************************************************************************/
#define RADIX_HID_M0		0x0   // CFGC 13200000
#define RADIX_HID_M1		0x1   // VDMA 13208000
#define RADIX_HID_M2		0x2   // ODMA 13210000
#define RADIX_HID_M3		0x3   // TMC  13218000
#define RADIX_HID_M4		0x4   // EFE  13220000
#define RADIX_HID_M5		0x5   // JRFD 13228000
#define RADIX_HID_M6		0x6   // MCE  13230000
#define RADIX_HID_M7		0x7   // TFM  13238000
#define RADIX_HID_M8		0x8   // MD   13240000
#define RADIX_HID_M9		0x9   // DT   13248000
#define RADIX_HID_M10		0xA   // DBLK 13250000
#define RADIX_HID_M11		0xB   // SAO  13258000
#define RADIX_HID_M12		0xC   // BC   13260000
#define RADIX_HID_M13		0xD   // SDE  13268000
#define RADIX_HID_M14		0xE   // IPRED 13270000
#define RADIX_HID_M15		0xF   // STC  13278000
#define RADIX_HID_M16 		0x10  // VMAU 13280000
#define RADIX_HID_M17 		0x11  // VMAU 13288000
#define RADIX_HID_M18 		0x12  // EMC  13290000
#define RADIX_HID_M19       0x1E  // JPEG
#define	HID_SDE                 RADIX_HID_M13

#define RADIX_CFGC_BASE		(RADIX_BASE + (RADIX_HID_M0 << 15))
#define RADIX_VDMA_BASE		(RADIX_BASE + (RADIX_HID_M1 << 15))
#define RADIX_ODMA_BASE		(RADIX_BASE + (RADIX_HID_M2 << 15))
#define RADIX_TMC_BASE		(RADIX_BASE + (RADIX_HID_M3 << 15))
#define RADIX_EFE_BASE		(RADIX_BASE + (RADIX_HID_M4 << 15))
#define RADIX_JRFD_BASE		(RADIX_BASE + (RADIX_HID_M5 << 15))
#define RADIX_MCE_BASE		(RADIX_BASE + (RADIX_HID_M6 << 15))
#define RADIX_TFM_BASE		(RADIX_BASE + (RADIX_HID_M7 << 15))
#define RADIX_MD_BASE		(RADIX_BASE + (RADIX_HID_M8 << 15))
#define RADIX_DT_BASE		(RADIX_BASE + (RADIX_HID_M9 << 15))
#define RADIX_DBLK_BASE		(RADIX_BASE + (RADIX_HID_M10 << 15))
#define RADIX_SAO_BASE		(RADIX_BASE + (RADIX_HID_M11 << 15))
#define RADIX_BC_BASE		(RADIX_BASE + (RADIX_HID_M12 << 15))
#define RADIX_SDE_BASE		(RADIX_BASE + (RADIX_HID_M13 << 15))
#define RADIX_IPRED_BASE	(RADIX_BASE + (RADIX_HID_M14 << 15))
#define RADIX_STC_BASE		(RADIX_BASE + (RADIX_HID_M15 << 15))
#define RADIX_VMAU_BASE		(RADIX_BASE + (RADIX_HID_M16 << 15))
#define RADIX_EMC_BASE		(RADIX_BASE + (RADIX_HID_M18 << 15))
#define RADIX_JPEG_BASE         (RADIX_BASE + (RADIX_HID_M19 << 15))

/*************************************************************************************
                       Writing/Reading Registers
**************************************************************************************/
#define RADIX_SET_CFGC_REG(ofst, val)		({write_reg((RADIX_CFGC_BASE + (ofst)), (val));})
#define RADIX_GET_CFGC_REG(ofst)		({read_reg((RADIX_CFGC_BASE + (ofst)), (0));})

#define RADIX_SET_VDMA_REG(ofst, val)		({write_reg((RADIX_VDMA_BASE + (ofst)), (val));})
#define RADIX_GET_VDMA_REG(ofst)		({read_reg((RADIX_VDMA_BASE + (ofst)), (0));})

#define RADIX_SET_ODMA_REG(ofst, val)		({write_reg((RADIX_ODMA_BASE + (ofst)), (val));})
#define RADIX_GET_ODMA_REG(ofst)		({read_reg((RADIX_ODMA_BASE + (ofst)), (0));})

#define RADIX_SET_TMC_REG(ofst, val)		({write_reg((RADIX_TMC_BASE + (ofst)), (val));})
#define RADIX_GET_TMC_REG(ofst)		        ({read_reg((RADIX_TMC_BASE + (ofst)), (0));})

#define RADIX_SET_EFE_REG(ofst, val)		({write_reg((RADIX_EFE_BASE + (ofst)), (val));})
#define RADIX_GET_EFE_REG(ofst)		        ({read_reg((RADIX_EFE_BASE + (ofst)), (0));})

#define RADIX_SET_JRFD_REG(ofst, val)		({write_reg((RADIX_JRFD_BASE + (ofst)), (val));})
#define RADIX_GET_JRFD_REG(ofst)		({read_reg((RADIX_JRFD_BASE + (ofst)), (0));})

#define RADIX_SET_IPRED_REG(ofst, val)	        ({write_reg((RADIX_IPRED_BASE + (ofst)), (val));})
#define RADIX_GET_IPRED_REG(ofst)		({read_reg((RADIX_IPRED_BASE + (ofst)), (0));})

#define RADIX_SET_MCE_REG(ofst, val)		({write_reg((RADIX_MCE_BASE + (ofst)), (val));})
#define RADIX_GET_MCE_REG(ofst)		        ({read_reg((RADIX_MCE_BASE + (ofst)), (0));})

#define RADIX_SET_TFM_REG(ofst, val)		({write_reg((RADIX_TFM_BASE + (ofst)), (val));})
#define RADIX_GET_TFM_REG(ofst)		        ({read_reg((RADIX_TFM_BASE + (ofst)), (0));})

#define RADIX_SET_MD_REG(ofst, val)		({write_reg((RADIX_MD_BASE + (ofst)), (val));})
#define RADIX_GET_MD_REG(ofst)		        ({read_reg((RADIX_MD_BASE + (ofst)), (0));})

#define RADIX_SET_DT_REG(ofst, val)		({write_reg((RADIX_DT_BASE + (ofst)), (val));})
#define RADIX_GET_DT_REG(ofst)		        ({read_reg((RADIX_DT_BASE + (ofst)), (0));})

#define RADIX_SET_DBLK_REG(ofst, val)		({write_reg((RADIX_DBLK_BASE + (ofst)), (val));})
#define RADIX_GET_DBLK_REG(ofst)		({read_reg((RADIX_DBLK_BASE + (ofst)), (0));})

#define RADIX_SET_SAO_REG(ofst, val)		({write_reg((RADIX_SAO_BASE + (ofst)), (val));})
#define RADIX_GET_SAO_REG(ofst)		        ({read_reg((RADIX_SAO_BASE + (ofst)), (0));})

#define RADIX_SET_BC_REG(ofst, val)		({write_reg((RADIX_BC_BASE + (ofst)), (val));})
#define RADIX_GET_BC_REG(ofst)		        ({read_reg((RADIX_BC_BASE + (ofst)), (0));})

#define RADIX_SET_SDE_REG(ofst, val)		({write_reg((RADIX_SDE_BASE + (ofst)), (val));})
#define RADIX_GET_SDE_REG(ofst)		        ({read_reg((RADIX_SDE_BASE + (ofst)), (0));})

#define RADIX_SET_STC_REG(ofst, val)		({write_reg((RADIX_STC_BASE + (ofst)), (val));})
#define RADIX_GET_STC_REG(ofst)		        ({read_reg((RADIX_STC_BASE + (ofst)), (0));})

#define RADIX_SET_EMC_REG(ofst, val)		({write_reg((RADIX_EMC_BASE + (ofst)), (val));})
#define RADIX_GET_EMC_REG(ofst)		        ({read_reg((RADIX_EMC_BASE + (ofst)), (0));})

#define RADIX_SET_JPEG_REG(ofst, val)		({write_reg((RADIX_JPEG_BASE + (ofst)), (val));})
#define RADIX_GET_JPEG_REG(ofst)		({read_reg((RADIX_JPEG_BASE + (ofst)), (0));})

/*************************************************************************************
                  CFGC Module
*************************************************************************************/
#define	RADIX_REG_CFGC_GLB_CTRL	        (0x00 << 2)
#define	RADIX_REG_CFGC_ACM_CTRL         (0x02 << 2)
#define	RADIX_REG_CFGC_VTYPE            (0x03 << 2)
#define	T32_RADIX_REG_CFGC_TLB_TBA      (0x04 << 2)
#define	T32_RADIX_REG_CFGC_TLB_CTRL     (0x05 << 2)
#define	T32_RADIX_REG_CFGC_TLBV         (0x06 << 2)
#define	T32_RADIX_REG_CFGC_TLBE         (0x07 << 2)
#define	T33_RADIX_REG_CFGC_QOS          (0x08 << 2)
#define RADIX_REG_CFGC_INTR_EN          (0x0A << 2)
#define RADIX_REG_CFGC_INTR_STAT        (0x0B << 2)
#define RADIX_REG_CFGC_INTR_CLR         (0x0C << 2)
#define	RADIX_REG_CFGC_STAT             (0x10 << 2)
#define	RADIX_REG_CFGC_BSLEN            (0x11 << 2)
#define	RADIX_REG_CFGC_SDBG_0           (0x12 << 2)
#define	RADIX_REG_CFGC_SDBG_1           (0x13 << 2)
#define	RADIX_REG_CFGC_SSE_0            (0x14 << 2)
#define	RADIX_REG_CFGC_SSE_1            (0x15 << 2)
#define	RADIX_REG_CFGC_SA8D             (0x16 << 2)
#define	RADIX_REG_CFGC_EARB             (0x23 << 2)
#define	RADIX_REG_CFGC_VERSION          (0x24 << 2)
#define	RADIX_REG_CFGC_WORK_ALL_CNT     (0x28 << 2)
#define	RADIX_REG_CFGC_WORK_FRM_CNT     (0x29 << 2)
#define	RADIX_REG_CFGC_WORK_SHD_CNT     (0x2A << 2)
#define	RADIX_REG_CFGC_IDLE_ALL_CNT     (0x2B << 2)
#define	RADIX_REG_CFGC_IDLE_FRM_CNT     (0x2C << 2)
#define	RADIX_REG_CFGC_IDLE_SHD_CNT     (0x2D << 2)
#define	RADIX_REG_CFGC_TEST_CNT0        (0x2E << 2)
#define	RADIX_REG_CFGC_TEST_CNT1        (0x2F << 2)
#define	RADIX_REG_CFGC_MCE_PREFY_CNT    (0x3C << 2)
#define	RADIX_REG_CFGC_MCE_PREFC_CNT    (0x3D << 2)
#define	RADIX_REG_CFGC_MCE_MISSY_CNT    (0x3E << 2)
#define	RADIX_REG_CFGC_MCE_MISSC_CNT    (0x3F << 2)
#define RADIX_REG_CFGC_TIMEOUT_CTRL     (0x48 << 2)
#define RADIX_REG_CFGC_TIMEOUT_THRD     (0x49 << 2)
#define RADIX_REG_CFGC_TIMEOUT_CNT      (0x4a << 2)
#define	RADIX_REG_VMAU_MD_STA4          (0x170<< 2)
#define	T32_RADIX_REG_VMAU_MD_STA4      (0x77 << 2)
#define RADIX_CFGC_INTE_TMOT(a)         (a<<22)    //time out threshold
#define RADIX_CFGC_TOE_WORK             (0x1<<30)  //time out work enable
#define RADIX_CFGC_INTE_ENDF            (0x1<<0)
#define RADIX_CFGC_INTE_BSFULL          (0x1<<1)
#define RADIX_CFGC_INTE_AMCERR          (0x1<<2)
#define RADIX_CFGC_INTE_TOE             (0x1<<3)
#define RADIX_CFGC_INTE_ODMAEND         (0x1<<4)
#define RADIX_CFGC_INTE_SDEEND          (0x1<<5)
#define RADIX_CFGC_INTE_IVDCRST         (0x1<<6)
#define RADIX_CFGC_INTE_RLSTERR         (0x1<<7)
#define RADIX_CFGC_INTE_HBRGERR         (0x1<<8)
#define RADIX_CFGC_INTE_TIMEOUT         (0x1<<9)

#define MSCOPE_START(mbnum)  write_reg(VPU_BASE+0x24, mbnum)
#define MSCOPE_STOP()        write_reg(VPU_BASE+0x28, 0)

/*************************************************************************************
                  VDMA Module
*************************************************************************************/
#define RADIX_VDMA_ACFG_RUN        (0x1)
#define RADIX_VDMA_ACFG_CHECKSUM   (0x2)
#define RADIX_VDMA_ACFG_VLD        (0x1<<31)
#define RADIX_VDMA_ACFG_TERM       (0x1<<30)
#define RADIX_VDMA_ACFG_IDX(a)     (((uint32_t)(a)) & 0xFFFFC)
#define RADIX_VDMA_ACFG_DHA(a)     (((uint32_t)(a)) & 0xFFFFFF00)

#define VDMA_ACFG_RUN        (0x1)
#define VDMA_ACFG_CHECKSUM   (0x2)
#define VDMA_ACFG_VLD        (0x1<<31)
#define VDMA_ACFG_TERM       (0x1<<30)
#define VDMA_ACFG_IDX(a)     (((unsigned int)(a)) & 0xFFFFC)
#define VDMA_ACFG_DHA(a)     (((unsigned int)(a)) & 0xFFFFFF80)

//#define RW_REG_TEST
#ifdef RW_REG_TEST
#define GEN_VDMA_ACFG(chn, reg, term, val)  write_reg(VPU_BASE+(reg), val)
#else
#define RADIX_ACM_CHECKSUM_EN
#ifdef RADIX_ACM_CHECKSUM_EN
#define GEN_VDMA_ACFG(chn, reg, term, val)			     \
  ({*chn++ = val;						     \
    int checksum = (((((unsigned int)val) >> 0) & 0xff) +	     \
		    ((((unsigned int)val) >> 8) & 0xff) +	     \
		    ((((unsigned int)val) >> 16) & 0xff) +	     \
		    ((((unsigned int)val) >> 24) & 0xff) +	     \
		    ((((unsigned int)reg) >> 0) & 0xff) +	     \
		    ((((unsigned int)reg) >> 8) & 0xff) );	     \
    *chn++ = (VDMA_ACFG_VLD | (term) |				     \
	      ((checksum & 0x3FF) << 20) |			     \
	      VDMA_ACFG_IDX(reg));				     \
  })
#else // RADIX_ACM_CHECKSUM_EN
#define GEN_VDMA_ACFG(chn, reg, term, val)			\
  ({*chn++ = val;						\
    *chn++ = (VDMA_ACFG_VLD | (term) | VDMA_ACFG_IDX(reg));	\
  })
#endif // RADIX_ACM_CHECKSUM_EN
#endif
#define RADIX_GEN_VDMA_ACFG GEN_VDMA_ACFG

/*************************************************************************************
                  EFE Module
*************************************************************************************/
#define	RADIX_REG_EFE_CTRL		0x0
#define	RADIX_REG_EFE_STAT		0x4
#define RADIX_REG_EFE_FRM_SIZE		0x8
#define RADIX_REG_EFE_RAWY_ADDR		0xc
#define RADIX_REG_EFE_RAWC_ADDR		0x10
#define RADIX_REG_EFE_RAW_STRD		0x14
#define RADIX_REG_EFE_REF_ADDR		0x18
#define RADIX_REG_EFE_FRM_QP		0x1c
#define RADIX_REG_EFE_RRS_CFG		0x20
#define RADIX_REG_EFE_RAWY_WRAP		0x24
#define RADIX_REG_EFE_RAWC_WRAP		0x28
#define RADIX_REG_EFE_WRAP_CFG		0x2c
#define RADIX_REG_EFE_RFH_CFG0		0x30
#define RADIX_REG_EFE_RFH_CFG1		0x34
#define RADIX_REG_EFE_QPG_CFG		0x38
#define RADIX_REG_EFE_CPLX_QP0		0x3c
#define RADIX_REG_EFE_CPLX_QP1		0x40
#define RADIX_REG_EFE_SMD_CPLX_THRD0	0x44
#define RADIX_REG_EFE_SMD_CPLX_THRD1	0x48
#define RADIX_REG_EFE_SMD_CPLX_THRD2	0x4c
#define RADIX_REG_EFE_SMD_CPLX_THRD3	0x50
#define RADIX_REG_EFE_ROI_INFOA		0x54
#define RADIX_REG_EFE_ROI_INFOB		0x58
#define RADIX_REG_EFE_ROI_INFOC		0x5c
#define RADIX_REG_EFE_ROI_INFOD		0x60
#define RADIX_REG_EFE_ROI0_POS		0x64
#define RADIX_REG_EFE_ROI1_POS		0x68
#define RADIX_REG_EFE_ROI2_POS		0x6c
#define RADIX_REG_EFE_ROI3_POS		0x70
#define RADIX_REG_EFE_ROI4_POS		0x74
#define RADIX_REG_EFE_ROI5_POS		0x78
#define RADIX_REG_EFE_ROI6_POS		0x7c
#define RADIX_REG_EFE_ROI7_POS		0x80
#define RADIX_REG_EFE_ROI8_POS		0x84
#define RADIX_REG_EFE_ROI9_POS		0x88
#define RADIX_REG_EFE_ROI10_POS		0x8c
#define RADIX_REG_EFE_ROI11_POS		0x90
#define RADIX_REG_EFE_ROI12_POS		0x94
#define RADIX_REG_EFE_ROI13_POS		0x98
#define RADIX_REG_EFE_ROI14_POS		0x9c
#define RADIX_REG_EFE_ROI15_POS		0xa0
#define RADIX_REG_EFE_IFA_CFG		0xa4
#define RADIX_REG_EFE_SKIN_FTR		0xa8
#define RADIX_REG_EFE_SKIN_LVL0		0xac
#define RADIX_REG_EFE_SKIN_LVL1		0xb0
#define RADIX_REG_EFE_SKIN_THRD0	0xb4
#define RADIX_REG_EFE_SKIN_THRD1	0xb8
#define RADIX_REG_EFE_SKIN_THRD2	0xbc
#define RADIX_REG_EFE_ROI_EN            0xc0
#define RADIX_REG_EFE_SOBEL_CPLX_THRD0	0xc4
#define RADIX_REG_EFE_SOBEL_CPLX_THRD1	0xc8
#define RADIX_REG_EFE_SOBEL_CPLX_THRD2	0xcc
#define RADIX_REG_EFE_SOBEL_CPLX_THRD3	0xd0

#define RADIX_REG_EFE_CPLX_THRD    	0xd4
#define RADIX_REG_EFE_SLV_RD	     	0xd8
#define RADIX_REG_EFE_SLV_RDATA	     	0xdc
#define RADIX_REG_EFE_DEBUG_INFO0    	0xe0
#define RADIX_REG_EFE_DEBUG_INFO1    	0xe4
#define RADIX_REG_EFE_DEBUG_INFO2    	0xe8
#define RADIX_REG_EFE_DEBUG_INFO3    	0xec
#define RADIX_REG_EFE_DEBUG_INFO4    	0xf0
#define RADIX_REG_EFE_DEBUG_INFO5    	0xf4
#define RADIX_REG_EFE_DEBUG_RST_INFO0	0xf8
#define RADIX_REG_EFE_DEBUG_RST_INFO1	0xfc
#define RADIX_REG_EFE_DEBUG_RST_INFO2	0x100
#define RADIX_REG_EFE_DEBUG_RST_INFO3	0x104
#define RADIX_REG_EFE_DEBUG_RST_INFO4	0x108
#define RADIX_REG_EFE_DEBUG_RST_INFO5	0x10c
#define RADIX_REG_EFE_QPG_INFO0	        0x110
#define RADIX_REG_EFE_QPG_INFO1	        0x114
#define RADIX_REG_EFE_QPG_INFO2	        0x118
#define RADIX_REG_EFE_QPG_INFO3	        0x11c
#define RADIX_REG_EFE_QPG_INFO4	        0x120
#define RADIX_REG_EFE_FRM_CPLX0	        0x124
#define RADIX_REG_EFE_FRM_CPLX1	        0x128
#define RADIX_REG_EFE_FRM_CPLX2	        0x12c
#define RADIX_REG_EFE_FRM_CPLX3	        0x130
#define RADIX_REG_EFE_FRM_CPLX4	        0x134
#define RADIX_REG_EFE_MT_THRD0	        0x138
#define RADIX_REG_EFE_MT_THRD1	        0x13c
#define RADIX_REG_EFE_MT_CNT0	        0x140
#define RADIX_REG_EFE_MT_CNT1	        0x144
#define RADIX_REG_EFE_MT_CNT2	        0x148
#define RADIX_REG_EFE_MT_CNT3	        0x14c

#define RADIX_REG_EFE_DFT_CFG0		0x160
#define RADIX_REG_EFE_DFT_CFG1		0x164
#define RADIX_REG_EFE_DFT_CFG2		0x168
#define RADIX_REG_EFE_DFT_CFG3		0x16c
#define RADIX_REG_EFE_DFT_CFG4		0x170
#define RADIX_REG_EFE_DFT_CFG5		0x174
#define RADIX_REG_EFE_DFT_CFG6		0x178
#define RADIX_REG_EFE_DFT_CFG7		0x17c
#define RADIX_REG_EFE_EXTRA_CFG0	0x180
#define T33_RADIX_REG_EFE_EXTRA_CFG1	0x184
//IFAC
#define RADIX_REG_EFE_IFAC_REF_ADDR	0x1A8
#define RADIX_REG_EFE_IFAC_ALL_EN	0x1AC

//PETBC zhliang
#define RADIX_REG_EFE_PET_CPLX_THRD00	0x1B0
#define RADIX_REG_EFE_PET_CPLX_THRD01	0x1B4
#define RADIX_REG_EFE_PET_CPLX_THRD02	0x1B8
#define RADIX_REG_EFE_PET_CPLX_THRD10	0x1Bc
#define RADIX_REG_EFE_PET_CPLX_THRD11	0x1C0
#define RADIX_REG_EFE_PET_CPLX_THRD12	0x1C4
#define RADIX_REG_EFE_PET_CPLX_THRD20	0x1C8
#define RADIX_REG_EFE_PET_CPLX_THRD21	0x1CC
#define RADIX_REG_EFE_PET_CPLX_THRD22	0x1D0
#define RADIX_REG_EFE_PET_QP_OFST0	0x1D4
#define RADIX_REG_EFE_PET_QP_OFST1	0x1D8
#define RADIX_REG_EFE_PET_QP_OFST2	0x1DC

#define RADIX_REG_EFE_PET_QP_OFST3	0x1E0
#define RADIX_REG_EFE_PET_QP_OFST4	0x1E4
#define RADIX_REG_EFE_PET_QP_OFST_MT0	0x1E8
#define RADIX_REG_EFE_PET_QP_OFST_MT1	0x1EC
#define RADIX_REG_EFE_PET_QP_OFST_MT2	0x1F0
#define RADIX_REG_EFE_PET_QP_OFST_MT3	0x1F4
#define RADIX_REG_EFE_PET_QP_OFST_MT4	0x1F8

#define RADIX_REG_EFE_QPG_CFG1          0x29c

#define RADIX_REG_EFE_PETBC_VAR_THR     0x2a0
#define RADIX_REG_EFE_PETBC_SSM_THR0    0x2a4
#define RADIX_REG_EFE_PETBC_SSM_THR1    0x2a8
#define RADIX_REG_EFE_PETBC_SSM_THR2    0x2ac

#define RADIX_REG_EFE_RRC_MOTION_THR0	0x2d8
#define RADIX_REG_EFE_RRC_MOTION_THR1	0x2dc
#define RADIX_REG_EFE_RRC_MOTION_THR2	0x2e0
#define RADIX_REG_EFE_RRC_MOTION_THR3	0x2e4

#define RADIX_REG_EFE_PETBC2_VAR_THR	0x2ec
#define RADIX_REG_EFE_PETBC2_SSM_THR0	0x2f0
#define RADIX_REG_EFE_PETBC2_SSM_THR1	0x2f4
#define RADIX_REG_EFE_PETBC2_SSM_THR2	0x2f8
#define RADIX_REG_EFE_PET_FILTER_VALID  0x2fc

#define RADIX_REG_EFE_QPG_DLT_THRD      0x300
#define RADIX_REG_EFE_IFA_CFG1		0x304
#define RADIX_REG_EFE_PLAIN_UNIFOR_THRD 0x308 //(194*4)
#define RADIX_REG_EFE_RAS_SAD_DIF_THR   0x30c //(195*4)
#define RADIX_REG_EFE_RAS_EDGE_CNT_THR  0x310 //(196*4)
#define RADIX_REG_EFE_PET_QP_IDX        0x314
#define RADIX_REG_EFE_AI_MT_SMD_OFST0   0x318
#define RADIX_REG_EFE_AI_MT_SMD_OFST1   0x31c
#define RADIX_REG_EFE_AI_MT_SMD_OFST2   0x320
#define RADIX_REG_EFE_AI_MT_SMD_OFST3   0x324
#define RADIX_REG_EFE_AI_MT_SMD_OFST4   0x328
#define RADIX_REG_EFE_AI_MT_SMD_OFST5   0x32c
#define RADIX_REG_EFE_AI_MT_SMD_OFST6   0x330
#define RADIX_REG_EFE_AI_MT_SMD_OFST7   0x334
#define RADIX_REG_EFE_AI_MT_SMD_OFST8   0x338
#define RADIX_REG_EFE_AI_MT_SMD_OFST9   0x33c

#define RADIX_REG_EFE_FRMB_IP_PARAM0    0x380 //(224*4)
#define RADIX_REG_EFE_FRMB_IP_PARAM1    0x384 //(225*4)
#define RADIX_REG_EFE_FRMB_IP_PARAM2    0x388 //(226*4)
#define RADIX_REG_EFE_FRMB_IP_PARAM3    0x38c //(227*4)
#define RADIX_REG_EFE_REF1_ADDR         0x390
#define RADIX_REG_EFE_REF1C_ADDR        0x394
#define RADIX_REG_EFE_IFAC_ALL1_EN      0x398

#define T33_RADIX_REG_EFE_ROW_SMD_RD_STATE  0x3B0
#define T33_RADIX_REG_EFE_SMD_WR_BASE_ADDR  0x400

#define T33_RADIX_REG_EFE_FPDBG_CHNG_POS0        576*4   //dbg fpga input info from 0x900
#define T33_RADIX_REG_EFE_FPDBG_CHNG_POS1        577*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_POS2        578*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_MCE   579*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_PP    580*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_IP0   581*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_IP1   582*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_IP2   583*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_DT    584*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_TFM0  585*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_TFM1  586*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_TFM2  587*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_TFM3  588*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_TFM4  589*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_POS_TFM5  590*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_FET_POS      591*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RDOND_CNT_POS 592*4

#define T33_RADIX_REG_EFE_FPDBG_CHNG_PP0         608*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_PP1         609*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_PP2         610*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_IP0         611*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_IP1         612*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_IP2         613*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_IP3         614*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_IP4         615*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_IP5         616*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_IP6         617*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_IP7         618*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE0        619*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE1        620*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE2        621*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE3        622*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE4        623*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE5        624*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE6        625*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE7        626*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE8        627*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE9        628*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MCE10       629*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_TFM0        630*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_TFM1        631*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD0         631*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD1         633*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD2         634*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD3         635*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD4         636*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD5         637*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD6         638*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD7         639*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD8         640*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD9         641*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD10        642*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD11        643*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD12        644*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD13        645*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD14        646*4
#define T33_RADIX_REG_EFE_FPDBG_CHNG_MD15        647*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_MCE0 648*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_MCE1 649*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_MCE2 650*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_MCE3 651*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_PP0  652*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_PP1  653*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_PP2  654*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_PP3  655*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP00 656*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP01 657*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP02 658*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP03 659*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP10 660*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP11 661*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP12 662*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP13 663*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP20 664*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP21 665*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP22 666*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_IP23 667*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_DT0  668*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_DT1  669*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_DT2  670*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_DT3  671*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM00 672*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM01 673*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM02 674*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM03 675*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM10 676*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM11 677*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM12 678*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM13 679*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM20 680*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM21 681*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM22 682*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM23 683*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM30 684*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM31 685*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM32 686*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM33 687*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM40 688*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM41 689*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM42 690*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM43 691*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM50 692*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM51 693*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM52 694*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_RD_DATA_TFM53 695*4

#define T33_RADIX_REG_EFE_FPDBG_RAW_FET_DATA0     696*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_FET_DATA1     697*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_FET_DATA2     698*4
#define T33_RADIX_REG_EFE_FPDBG_RAW_FET_DATA3     699*4


#define RADIX_EFE_CTRL_YUV(a)    ((a & 1 ) << 0)
#define RADIX_EFE_CTRL_IP(a)     ((a & 1 ) << 1)
#define RADIX_EFE_CTRL_PP(a)     ((a & 1 ) << 2)
#define RADIX_EFE_CTRL_MCE(a)    ((a & 1 ) << 3)
#define RADIX_EFE_CTRL_CKG(a)    ((a & 1 ) << 4)
#define RADIX_EFE_CTRL_SAO(a)    ((a & 1 ) << 5)
#define RADIX_EFE_CTRL_WM(a)     ((a & 3 ) << 6)
#define RADIX_EFE_CTRL_GRAY(a)   ((a & 1 ) << 8)
#define RADIX_EFE_CTRL_CRC(a)    ((a & 1 ) << 9)
#define RADIX_EFE_CTRL_MVC(a)    ((a & 1 ) << 10)
#define RADIX_EFE_CTRL_CHNG(a)   ((a & 1 ) << 11)
#define RADIX_EFE_CTRL_IPC(a)    ((a & 1 ) << 12)
#define RADIX_EFE_CTRL_MDC(a)    ((a & 1 ) << 13)
#define RADIX_EFE_CTRL_MCEC(a)   ((a & 1 ) << 14)
#define RADIX_EFE_CTRL_FMT(a)    ((a & 1 ) << 15)
#define RADIX_EFE_CTRL_DBGX(a)   ((a & 1 ) << 16)
#define RADIX_EFE_CTRL_DBGY(a)   ((a & 1 ) << 22)
#define RADIX_EFE_CTRL_DBGI(a)   ((a & 1 ) << 28)
#define RADIX_EFE_CTRL_ROTATE(a) ((a & 3 ) << 30)

#define REG_EFE_QPG_DLT_THRD            0x0300 //265 RADIX_REG_EFE_QPG_DLT_THRD
#define REG_EFE_IFA_CFG1                0x0304 //265 RADIX_REG_EFE_IFA_CFG1
#define REG_EFE_CFM_CPLX_IDX_CFG0       0x0308 //265 RADIX_REG_EFE_PLAIN_UNIFOR_THRD
#define REG_EFE_CFM_CPLX_IDX_CFG1       0x030c //265 RADIX_REG_EFE_RAS_SAD_DIF_THR
#define REG_EFE_CFM_CPLX_IDX_CFG2       0x0310 //265 RADIX_REG_EFE_RAS_EDGE_CNT_THR

#define REG_EFE_CFM_CPLX_THRD_CFG0      0x0380 //265 RADIX_REG_EFE_FRMB_IP_PARAM0
#define REG_EFE_CFM_CPLX_THRD_CFG1      0x0384 //265 RADIX_REG_EFE_FRMB_IP_PARAM1

#define REG_EFE_CTRL                    0x0000 //265 RADIX_REG_EFE_CTRL
#define REG_EFE_STAT                    0x0004 //265 RADIX_REG_EFE_STAT
#define REG_EFE_FRM_SIZE                0x0008 //265 RADIX_REG_EFE_FRM_SIZE
#define REG_EFE_RAWY_ADDR               0x000c //265 RADIX_REG_EFE_RAWY_ADDR
#define REG_EFE_RAWC_ADDR               0x0010 //265 RADIX_REG_EFE_RAWC_ADDR
#define REG_EFE_RAW_STRD                0x0014 //265 RADIX_REG_EFE_RAW_STRD
#define REG_EFE_FRM_QP	                0x001c //265 RADIX_REG_EFE_FRM_QP
#define REG_EFE_RRS_CFG	                0x0020 //265 RADIX_REG_EFE_RRS_CFG

#define REG_EFE_RFH_CFG1                0x0034 //265 RADIX_REG_EFE_RFH_CFG1
#define REG_EFE_QPG_CFG	                0x0038 //265 RADIX_REG_EFE_QPG_CFG
#define REG_EFE_CPLX_QP0                0x003c //265 RADIX_REG_EFE_CPLX_QP0
#define REG_EFE_CPLX_QP1                0x0040 //265 RADIX_REG_EFE_CPLX_QP1

#define REG_EFE_SMD_CPLX_THRD0          0x0044 //265 RADIX_REG_EFE_SMD_CPLX_THRD0
#define REG_EFE_SMD_CPLX_THRD1          0x0048 //265 RADIX_REG_EFE_SMD_CPLX_THRD1
#define REG_EFE_SMD_CPLX_THRD2          0x004c //265 RADIX_REG_EFE_SMD_CPLX_THRD2
#define REG_EFE_SMD_CPLX_THRD3          0x0050 //265 RADIX_REG_EFE_SMD_CPLX_THRD3

#define REG_EFE_ROI_INFOA               0x0054 //265 RADIX_REG_EFE_ROI_INFOA
#define REG_EFE_ROI_INFOB               0x0058 //265 RADIX_REG_EFE_ROI_INFOB
#define REG_EFE_ROI_INFOC               0x005c //265 RADIX_REG_EFE_ROI_INFOC
#define REG_EFE_ROI_INFOD               0x0060 //265 RADIX_REG_EFE_ROI_INFOD
#define REG_EFE_ROI0_POS                0x0064 //265 RADIX_REG_EFE_ROI0_POS
#define REG_EFE_ROI1_POS                0x0068 //265 RADIX_REG_EFE_ROI1_POS
#define REG_EFE_ROI2_POS                0x006c //265 RADIX_REG_EFE_ROI2_POS
#define REG_EFE_ROI3_POS                0x0070 //265 RADIX_REG_EFE_ROI3_POS
#define REG_EFE_ROI4_POS                0x0074 //265 RADIX_REG_EFE_ROI4_POS
#define REG_EFE_ROI5_POS                0x0078 //265 RADIX_REG_EFE_ROI5_POS
#define REG_EFE_ROI6_POS                0x007c //265 RADIX_REG_EFE_ROI6_POS
#define REG_EFE_ROI7_POS                0x0080 //265 RADIX_REG_EFE_ROI7_POS
#define REG_EFE_ROI8_POS                0x0084 //265 RADIX_REG_EFE_ROI8_POS
#define REG_EFE_ROI9_POS                0x0088 //265 RADIX_REG_EFE_ROI9_POS
#define REG_EFE_ROI10_POS               0x008c //265 RADIX_REG_EFE_ROI10_POS
#define REG_EFE_ROI11_POS               0x0090 //265 RADIX_REG_EFE_ROI11_POS
#define REG_EFE_ROI12_POS               0x0094 //265 RADIX_REG_EFE_ROI12_POS
#define REG_EFE_ROI13_POS               0x0098 //265 RADIX_REG_EFE_ROI13_POS
#define REG_EFE_ROI14_POS               0x009c //265 RADIX_REG_EFE_ROI14_POS
#define REG_EFE_ROI15_POS               0x00a0 //265 RADIX_REG_EFE_ROI15_POS

#define REG_EFE_IFA_CFG	                0x00a4 //265 RADIX_REG_EFE_IFA_CFG
#define REG_EFE_SKIN_FTR                0x00a8 //265 RADIX_REG_EFE_SKIN_FTR
#define REG_EFE_SKIN_LVL0               0x00ac //265 RADIX_REG_EFE_SKIN_LVL0
#define REG_EFE_SKIN_LVL1               0x00b0 //265 RADIX_REG_EFE_SKIN_LVL1
#define REG_EFE_SKIN_THRD0              0x00b4 //265 RADIX_REG_EFE_SKIN_THRD0
#define REG_EFE_SKIN_THRD1              0x00b8 //265 RADIX_REG_EFE_SKIN_THRD1
#define REG_EFE_SKIN_THRD2              0x00bc //265 RADIX_REG_EFE_SKIN_THRD2
#define REG_EFE_ROI_EN                  0xc0   //265 RADIX_REG_EFE_ROI_EN
#define REG_EFE_SOBEL_CPLX_THRD0        0x00c4 //265 RADIX_REG_EFE_SOBEL_CPLX_THRD0
#define REG_EFE_SOBEL_CPLX_THRD1        0x00c8 //265 RADIX_REG_EFE_SOBEL_CPLX_THRD1
#define REG_EFE_SOBEL_CPLX_THRD2        0x00cc //265 RADIX_REG_EFE_SOBEL_CPLX_THRD2
#define REG_EFE_SOBEL_CPLX_THRD3        0x00d0 //265 RADIX_REG_EFE_SOBEL_CPLX_THRD3

#define T33_REG_EFE_DFT_CFG0                0x160 //265 RADIX_REG_EFE_DFT_CFG0
#define T33_REG_EFE_DFT_CFG1                0x164 //265 RADIX_REG_EFE_DFT_CFG1
#define T33_REG_EFE_DFT_CFG2                0x168 //265 RADIX_REG_EFE_DFT_CFG2
#define T33_REG_EFE_DFT_CFG3                0x16c //265 RADIX_REG_EFE_DFT_CFG3
#define T33_REG_EFE_DFT_CFG4                0x170 //265 RADIX_REG_EFE_DFT_CFG4
#define T33_REG_EFE_DFT_CFG5                0x174 //265 RADIX_REG_EFE_DFT_CFG5
#define T33_REG_EFE_DFT_CFG6                0x178 //265 RADIX_REG_EFE_DFT_CFG6
#define T33_REG_EFE_DFT_CFG7                0x17c //265 RADIX_REG_EFE_DFT_CFG7

#define REG_EFE_EXTRA_CFG0              0x0180 //265 RADIX_REG_EFE_EXTRA_CFG0
#define T33_REG_EFE_EXTRA_CFG1              0x0184 //265 RADIX_REG_EFE_EXTRA_CFG1
#define REG_EFE_IFAC_ALL_EN             0x01ac //265 RADIX_REG_EFE_IFAC_ALL_EN

#define REG_EFE_PET_CPLX_THRD00	        0x01B0 //265 RADIX_REG_EFE_PET_CPLX_THRD00
#define REG_EFE_PET_CPLX_THRD01	        0x01B4 //265 RADIX_REG_EFE_PET_CPLX_THRD01
#define REG_EFE_PET_CPLX_THRD02	        0x01B8 //265 RADIX_REG_EFE_PET_CPLX_THRD02
#define REG_EFE_PET_CPLX_THRD10	        0x01Bc //265 RADIX_REG_EFE_PET_CPLX_THRD10
#define REG_EFE_PET_CPLX_THRD11	        0x01C0 //265 RADIX_REG_EFE_PET_CPLX_THRD11
#define REG_EFE_PET_CPLX_THRD12	        0x01C4 //265 RADIX_REG_EFE_PET_CPLX_THRD12
#define REG_EFE_PET_CPLX_THRD20	        0x01C8 //265 RADIX_REG_EFE_PET_CPLX_THRD20
#define REG_EFE_PET_CPLX_THRD21	        0x01CC //265 RADIX_REG_EFE_PET_CPLX_THRD21
#define REG_EFE_PET_CPLX_THRD22	        0x01D0 //265 RADIX_REG_EFE_PET_CPLX_THRD22
#define REG_EFE_PET_QP_OFST0	        0x01D4 //265 RADIX_REG_EFE_PET_QP_OFST0
#define REG_EFE_PET_QP_OFST1	        0x01D8 //265 RADIX_REG_EFE_PET_QP_OFST1
#define REG_EFE_PET_QP_OFST2	        0x01DC //265 RADIX_REG_EFE_PET_QP_OFST2

#define REG_EFE_PET_QP_OFST3	        0x01E0 //265 RADIX_REG_EFE_PET_QP_OFST3
#define REG_EFE_PET_QP_OFST4	        0x01E4 //265 RADIX_REG_EFE_PET_QP_OFST4
#define REG_EFE_PET_QP_OFST_MT0	        0x01E8 //265 RADIX_REG_EFE_PET_QP_OFST_MT0
#define REG_EFE_PET_QP_OFST_MT1	        0x01EC //265 RADIX_REG_EFE_PET_QP_OFST_MT1
#define REG_EFE_PET_QP_OFST_MT2	        0x01F0 //265 RADIX_REG_EFE_PET_QP_OFST_MT2
#define REG_EFE_PET_QP_OFST_MT3	        0x01F4 //265 RADIX_REG_EFE_PET_QP_OFST_MT3
#define REG_EFE_PET_QP_OFST_MT4	        0x01F8 //265 RADIX_REG_EFE_PET_QP_OFST_MT4

#define REG_EFE_QPG_CFG1                0x029c //265 RADIX_REG_EFE_QPG_CFG1
#define REG_EFE_PETBC_VAR_THR           0x02a0 //265 RADIX_REG_EFE_PETBC_VAR_THR
#define REG_EFE_PETBC_SSM_THR0          0x02a4 //265 RADIX_REG_EFE_PETBC_SSM_THR0
#define REG_EFE_PETBC_SSM_THR1          0x02a8 //265 RADIX_REG_EFE_PETBC_SSM_THR1
#define REG_EFE_PETBC_SSM_THR2          0x02ac //265 RADIX_REG_EFE_PETBC_SSM_THR2

#define REG_EFE_RRC_MOTION_THR0         0x02d8 //265 RADIX_REG_EFE_RRC_MOTION_THR0
#define REG_EFE_RRC_MOTION_THR1         0x02dc //265 RADIX_REG_EFE_RRC_MOTION_THR1
#define REG_EFE_RRC_MOTION_THR2         0x02e0 //265 RADIX_REG_EFE_RRC_MOTION_THR2
#define REG_EFE_RRC_MOTION_THR3         0x02e4 //265 RADIX_REG_EFE_RRC_MOTION_THR3

#define REG_EFE_PETBC2_VAR_THR	        0x02ec //265 RADIX_REG_EFE_PETBC2_VAR_THR
#define REG_EFE_PETBC2_SSM_THR0	        0x02f0 //265 RADIX_REG_EFE_PETBC2_SSM_THR0
#define REG_EFE_PETBC2_SSM_THR1	        0x02f4 //265 RADIX_REG_EFE_PETBC2_SSM_THR1
#define REG_EFE_PETBC2_SSM_THR2	        0x02f8 //265 RADIX_REG_EFE_PETBC2_SSM_THR2
#define REG_EFE_PET_FILTER_VALID        0x02fc //265 RADIX_REG_EFE_PET_FILTER_VALID

#define REG_EFE_PET_QP_IDX              0x0314 //265 RADIX_REG_EFE_PET_QP_IDX
#define REG_EFE_AI_MT_SMD_OFST0         0x0318 //265 RADIX_REG_EFE_AI_MT_SMD_OFST0
#define REG_EFE_AI_MT_SMD_OFST1         0x031c //265 RADIX_REG_EFE_AI_MT_SMD_OFST1
#define REG_EFE_AI_MT_SMD_OFST2         0x0320 //265 RADIX_REG_EFE_AI_MT_SMD_OFST2
#define REG_EFE_AI_MT_SMD_OFST3         0x0324 //265 RADIX_REG_EFE_AI_MT_SMD_OFST3
#define REG_EFE_AI_MT_SMD_OFST4         0x0328 //265 RADIX_REG_EFE_AI_MT_SMD_OFST4
#define REG_EFE_AI_MT_SMD_OFST5         0x032c //265 RADIX_REG_EFE_AI_MT_SMD_OFST5
#define REG_EFE_AI_MT_SMD_OFST6         0x0330 //265 RADIX_REG_EFE_AI_MT_SMD_OFST6
#define REG_EFE_AI_MT_SMD_OFST7         0x0334 //265 RADIX_REG_EFE_AI_MT_SMD_OFST7
#define REG_EFE_AI_MT_SMD_OFST8         0x0338 //265 RADIX_REG_EFE_AI_MT_SMD_OFST8
#define REG_EFE_AI_MT_SMD_OFST9         0x033c //265 RADIX_REG_EFE_AI_MT_SMD_OFST9

#define REG_EFE_IFAC_ALL1_EN            0x0398 //265 RADIX_REG_EFE_IFAC_ALL1_EN
#define REG_EFE_CFM_CFG0                0x039c // only 264
#define REG_EFE_CFM_CFG1                0x03a0 // only 264

#define REG_EFE_FRM_SMD_SUM             0x03a4
#define REG_EFE_FRM_SMDC_SUM            0x03a8

#define EFE_PLANE_TILE       (0x0<<2)
#define EFE_PLANE_420P       (0x1<<2)
#define EFE_PLANE_NV12       (0x2<<2)
#define EFE_PLANE_NV21       (0x3<<2)

#define REG_EFE_QPG_RGNC_A       0x0094
#define REG_EFE_QPG_RGNC_B       0x0098
#define REG_EFE_QPG_RGNC_C       0x009C
#define REG_EFE_QPG_RGNC_D       0x00A0
#define REG_EFE_QP_SUM           0x00A4

#define EFE_RAW_STRDY(y)     (((y) & 0xFFFF)<<0)
#define EFE_RAW_STRDC(c)     (((c) & 0xFFFF)<<16)

#define REG_EFE_SSAD         0x0108

#define EFE_RC_QPO_CFG(c4, c3, c2, c1)		\
( ((c4) & 0x3F)<<18 |                           \
  ((c3) & 0x3F)<<12 |                           \
  ((c2) & 0x3F)<<6 |                            \
  ((c1) & 0x3F)<<0                              \
)

/*************************************************************************************
                  TMC Module
*************************************************************************************/
#define HERA_REG_TMC_CTRL               0x0000

/*************************************************************************************
                  EMC Module
*************************************************************************************/
#define REG_EMC_FRM_SIZE                0x0000
#define REG_EMC_BS_ADDR                 0x0004
#define REG_EMC_DBLK_ADDR               0x0008
#define REG_EMC_RECON_ADDR              0x000c
#define REG_EMC_MV_ADDR                 0x0010
#define REG_EMC_SE_ADDR                 0x0014
#define REG_EMC_QPT_ADDR                0x0018
#define REG_EMC_RC_RADDR                0x001c
#define REG_EMC_MOS_ADDR                0x0020
#define REG_EMC_SLV_INIT                0x0024
#define REG_EMC_DEBUG_INFO0             0x0028
#define REG_EMC_DEBUG_INFO1             0x002c
#define REG_EMC_CRC_INFO0               0x0030
#define REG_EMC_CRC_INFO1               0x0034
#define REG_EMC_CRC_INFO2               0x0038
#define REG_EMC_CRC_INFO3               0x003c
#define REG_EMC_BS_SIZE                 0x0040
#define REG_EMC_BS_STAT                 0x0044
#define REG_EMC_RC_WADDR                0x0048
#define REG_EMC_CPX_ADDR                0x004c
#define REG_EMC_MOD_ADDR                0x0050
#define REG_EMC_SAD_ADDR                0x0054
#define REG_EMC_NCU_ADDR                0x0058
#define REG_EMC_RRS_ADDR                0x005c

#define RADIX_REG_EMC_CFG               0x0
#define RADIX_REG_EMC_STAT              0x4
#define RADIX_REG_EMC_ADDR_BS0          0x8
#define RADIX_REG_EMC_ADDR_CPS          0xc
#define RADIX_REG_EMC_ADDR_SOBEL        0x10
#define RADIX_REG_EMC_ADDR_MIX          0x14
#define RADIX_REG_EMC_ADDR_MVO          0x18
#define RADIX_REG_EMC_ADDR_BS1          0x1c
#define RADIX_REG_EMC_ADDR_FLAG         0x20
#define RADIX_REG_EMC_ADDR_MDC          0x24
#define RADIX_REG_EMC_ADDR_QPT          0x28
#define RADIX_REG_EMC_ADDR_MCEC         0x2c
#define RADIX_REG_EMC_ADDR_IPC          0x30
#define RADIX_REG_EMC_ADDR_MVI          0x34
#define RADIX_REG_EMC_ADDR_RRS0         0x38
#define RADIX_REG_EMC_ADDR_RRS1         0x3c
#define RADIX_REG_EMC_ADDR_DBG_CFG      0x40
#define RADIX_REG_EMC_ADDR_AI           0x50
#define RADIX_REG_EMC_ADDR_MIXC         0x54
#define RADIX_REG_EMC_BSFULL_EN         0x5c
#define RADIX_REG_EMC_BSFULL_STAT       0x60
#define RADIX_REG_EMC_BSFULL_SIZE       0x64
#define RADIX_REG_EMC_ADDR_CHN          0x68

#define HERA_REG_EMC_CFG                0x0 //265 RADIX_REG_EMC_CFG
#define HERA_REG_EMC_STAT               0x4 //265 RADIX_REG_EMC_STAT
#define HERA_REG_EMC_ADDR_BS0           0x8 //265 RADIX_REG_EMC_ADDR_BS0
#define HERA_REG_EMC_ADDR_CPS           0xc //265 RADIX_REG_EMC_ADDR_CPS
#define HERA_REG_EMC_ADDR_SOBEL         0x10 //265 RADIX_REG_EMC_ADDR_SOBEL
#define HERA_REG_EMC_ADDR_MIX           0x14 //265 RADIX_REG_EMC_ADDR_MIX
#define HERA_REG_EMC_ADDR_MVO           0x18 //265 RADIX_REG_EMC_ADDR_MVO
#define HERA_REG_EMC_ADDR_BS1           0x1c //265 RADIX_REG_EMC_ADDR_BS1
#define HERA_REG_EMC_ADDR_FLAG          0x20 //265 RADIX_REG_EMC_ADDR_FLAG
#define HERA_REG_EMC_ADDR_MOD           0x24 //265 RADIX_REG_EMC_ADDR_MDC
#define HERA_REG_EMC_ADDR_QPT           0x28 //265 RADIX_REG_EMC_ADDR_QPT
#define HERA_REG_EMC_ADDR_MCEC          0x2c //265 RADIX_REG_EMC_ADDR_MCEC
#define HERA_REG_EMC_ADDR_IPC           0x30 //265 RADIX_REG_EMC_ADDR_IPC
#define HERA_REG_EMC_ADDR_MVI           0x34 //265 RADIX_REG_EMC_ADDR_MVI
#define HERA_REG_EMC_ADDR_RRS0          0x38 //265 RADIX_REG_EMC_ADDR_RRS0
#define HERA_REG_EMC_ADDR_RRS1          0x3c //265 RADIX_REG_EMC_ADDR_RRS1
#define HERA_REG_EMC_ADDR_DBG_CFG       0x40 //265 RADIX_REG_EMC_ADDR_DBG_CFG
#define HERA_REG_EMC_ADDR_AI            0x50 //265 RADIX_REG_EMC_ADDR_AI
#define HERA_REG_EMC_ADDR_MIXC          0x54 //265 RADIX_REG_EMC_ADDR_MIXC
#define HERA_REG_EMC_BSFULL_EN          0x5c //265 RADIX_REG_EMC_BSFULL_EN
#define HERA_REG_EMC_BSFULL_STAT        0x60 //265 RADIX_REG_EMC_BSFULL_STAT
#define HERA_REG_EMC_BSFULL_SIZE        0x64 //265 RADIX_REG_EMC_BSFULL_SIZE
#define HERA_REG_EMC_ADDR_CHN           0x68 //265 RADIX_REG_EMC_ADDR_CHN

/*************************************************************************************
                  ODMA Module
*************************************************************************************/
#define RADIX_REG_ODMA_GLB_TRIG              0x0   //software reset
#define RADIX_REG_ODMA_GLB_CTRL              0x4   //global control
#define RADIX_REG_ODMA_TLY_BA                0x8   //tile y start address / jrfc y body start address
#define RADIX_REG_ODMA_TLC_BA                0xC   //tile c start address / jrfc c body start address
#define RADIX_REG_ODMA_TL_STR                0x10  //tile stride
#define RADIX_REG_ODMA_NVY_BA                0x14  //nv12 y start address
#define RADIX_REG_ODMA_NVC_BA                0x18  //nv12 c start address
#define RADIX_REG_ODMA_NV_STR                0x1C  //tile stride
#define RADIX_REG_ODMA_JRFC_HEAD_Y_BA        0x20  //y head base address
#define RADIX_REG_ODMA_JRFC_HEAD_C_BA        0x24  //c head base address
#define T33_RADIX_REG_ODMA_BUSH_BODY_Y_SPACE     0x28  //bufshare body y sapce
#define T33_RADIX_REG_ODMA_BUSH_BODY_C_SPACE     0x2C  //bufshare body c sapce
#define T32_RADIX_REG_ODMA_JRFC_SP_HEAD_Y_BA     0x28  //sp y head base address
#define T32_RADIX_REG_ODMA_JRFC_SP_HEAD_C_BA     0x2C  //sp c head base address
#define RADIX_REG_ODMA_BFSH_BASE_Y_BA        0x30  //bufshare addr base y
#define RADIX_REG_ODMA_BFSH_BASE_C_BA        0x34  //bufshare addr base c
#define RADIX_REG_ODMA_BFSH_BYND_Y_BA        0x38  //bufshare addr beyond y
#define RADIX_REG_ODMA_BFSH_BYND_C_BA        0x3C  //bufshare addr beyond c

#define RADIX_REG_ODMA_ST                    0x100  //odma status address
#define RADIX_REG_ODMA_IST                   0x104  //input address
#define RADIX_REG_ODMA_OST                   0x108  //output address
#define RADIX_REG_ODMA_JRFC_ST               0x10C  //jrfc status address
#define RADIX_REG_ODMA_HEAD_Y_SUM            0x110  //head y sum address
#define RADIX_REG_ODMA_HEAD_C_SUM            0x114  //head c sum address
#define RADIX_REG_ODMA_BODY_Y_SUM            0x118  //body y sum address
#define RADIX_REG_ODMA_BODY_C_SUM            0x11C  //body c sum address
#define RADIX_REG_ODMA_HEAD_Y_CRC            0x120  //head y sum address
#define RADIX_REG_ODMA_HEAD_C_CRC            0x124  //head c sum address
#define RADIX_REG_ODMA_BODY_Y_CRC            0x128  //body y sum address
#define RADIX_REG_ODMA_BODY_C_CRC            0x12C  //body c sum address
#define BFSH_LAST_YBODY_ADDR                 0x168  //T33,busf next frame body start addr
#define BFSH_LAST_CBODY_ADDR                 0x16C  //T33,busf next frame body start addr
#define T33_RADIX_REG_ODMA_REF_HEAD_CFG          0x170  //ref frm overlay check cfg
#define T33_RADIX_REG_ODMA_REF_HEAD_BASE         0x174  //ref frm head base addr
#define T33_RADIX_REG_ODMA_REF_BODY_BASE         0x178  //ref frm body base addr
#define T33_RADIX_REG_ODMA_REF_BODY_LAST         0x17C  //ref frm body last addr
#define T33_RADIX_REG_ODMA_REF_BODY_START        0x180  //ref frm body start addr
#define T33_RADIX_REG_ODMA_MAX_BODY_SIZE         0x184  //max curr body size
#define T33_RADIX_REG_ODMA_REC_REF_SPACE         0x188  //max curr body size
#define T33_RADIX_REG_ODMA_REC_EXCD_INFO0        0x18C  //recon exceed ref frm info
#define T33_RADIX_REG_ODMA_REC_EXCD_INFO1        0x190  //recon exceed max size info
#define RADIX_REG_ODMA_NXT_FRM_YHEAD_INFO    0x194  //jrfc exceed max. for frm jrfd read y head
#define RADIX_REG_ODMA_NXT_FRM_CHEAD_INFO    0x198  //jrfc exceed max. for frm jrfd read c head
#define REG_ODMA_TRIG                        0x0000 // 265 RADIX_REG_ODMA_GLB_TRIG
#define REG_ODMA_CTRL                        0x0004 // 265 RADIX_REG_ODMA_GLB_CTRL
#define REG_ODMA_BDYA                        0x0008 // 265 RADIX_REG_ODMA_TLY_BA
#define REG_ODMA_BDCA                        0x000C // 265 RADIX_REG_ODMA_TLC_BA
#define REG_ODMA_BSTR                        0x0010 // 265 RADIX_REG_ODMA_TL_STR
#define REG_ODMA_HDYA                        0x0020 // 265 RADIX_REG_ODMA_JRFC_HEAD_Y_BA
#define REG_ODMA_HDCA                        0x0024 // 265 RADIX_REG_ODMA_JRFC_HEAD_C_BA
#define T32_REG_ODMA_SPYA                        0x0028 // 265 RADIX_REG_ODMA_JRFC_SP_HEAD_Y_BA
#define T32_REG_ODMA_SPCA                        0x002c // 265 RADIX_REG_ODMA_JRFC_SP_HEAD_C_BA
#define T33_REG_JRFC_YSPACE                      0x0028 // 265 RADIX_REG_ODMA_JRFC_BFSH_BODY_Y_SPACE
#define T33_REG_JRFC_CSPACE                      0x002c // 265 RADIX_REG_ODMA_JRFC_BFSH_BODY_C_SPACE
#define REG_ODMA_BUFS_BASEY_ADDR             0x0030 // 265 RADIX_REG_ODMA_BFSH_BASE_Y_BA
#define REG_ODMA_BUFS_BASEC_ADDR             0x0034 // 265 RADIX_REG_ODMA_BFSH_BASE_C_BA
#define REG_ODMA_BUFS_BEYDY_ADDR             0x0038 // 265 RADIX_REG_ODMA_BFSH_BYND_Y_BA
#define REG_ODMA_BUFS_BEYDC_ADDR             0x003c // 265 RADIX_REG_ODMA_BFSH_BYND_C_BA

#define ODMA_REC_STRDY(y)     (((y) & 0xFFFF)<<16)
#define ODMA_REC_STRDC(c)     (((c) & 0xFFFF)<<0)

/*************************************************************************************
                  JRFD Module
*************************************************************************************/
#define RADIX_REG_JRFD_TRIG             0x0
#define RADIX_REG_JRFD_CTRL             0x4
#define RADIX_REG_JRFD_HDYA             0x8
#define RADIX_REG_JRFD_HDCA             0xC
#define T33_RADIX_REG_JRFD_BSPACE           0x10
#define T32_RADIX_REG_JRFD_HSTR             0x10
#define RADIX_REG_JRFD_BDYA             0x14
#define RADIX_REG_JRFD_BDCA             0x18
#define RADIX_REG_JRFD_BSTR             0x1C
#define RADIX_REG_JRFD_BUFS_BASEY_ADDR  0x20
#define RADIX_REG_JRFD_BUFS_BASEC_ADDR  0x24
#define RADIX_REG_JRFD_BUFS_BEYDY_ADDR  0x28
#define RADIX_REG_JRFD_BUFS_BEYDC_ADDR  0x2C

#define RADIX_REG_JRFD_MHDY             0x30
#define RADIX_REG_JRFD_MHDC             0x34
#define RADIX_REG_JRFD_MBDY             0x38
#define RADIX_REG_JRFD_MBDC             0x3C

#define RADIX_REG_JRFD_YHEAD_INFO       0x44
#define RADIX_REG_JRFD_CHEAD_INFO       0x48
#define RADIX_REG_JRFD_EXCEED0          0x4C
#define RADIX_REG_JRFD_EXCEED1          0x50

#define RADIX_REG_JRFD_MCER             0x100 //mce req cnt
#define RADIX_REG_JRFD_MCEB             0x104 //mce 128bits cnt
#define RADIX_REG_JRFD_JRFR             0x108 //jrfd req cnt
#define RADIX_REG_JRFD_JRFB             0x10C //jrfd 128bits cnt
#define RADIX_REG_JRFD_HEAD             0x110 //jrfd miss head & have head cnt
#define RADIX_REG_JRFD_BODY             0x114 //jrfd miss body & have body cnt

#define REG_JRFD_TRIG                   0x0000 //265 RADIX_REG_JRFD_TRIG
#define REG_JRFD_CTRL                   0x0004 //265 RADIX_REG_JRFD_CTRL
#define REG_JRFD_HDYA                   0x0008 //265 RADIX_REG_JRFD_HDYA
#define REG_JRFD_HDCA                   0x000C //265 RADIX_REG_JRFD_HDCA
#define T33_REG_JRFD_SPACE                  0x0010 //265 RADIX_REG_JRFD_SPACE
#define T32_REG_JRFD_HSTR                   0x0010 //265 RADIX_REG_JRFD_HSTR
#define REG_JRFD_BDYA                   0x0014 //265 RADIX_REG_JRFD_BDYA
#define REG_JRFD_BDCA                   0x0018 //265 RADIX_REG_JRFD_BDCA
#define REG_JRFD_BSTR                   0x001C //265 RADIX_REG_JRFD_BSTR
#define REG_JRFD_BUFS_BASEY_ADDR        0x0020 //265 RADIX_REG_JRFD_BUFS_BASEY_ADDR
#define REG_JRFD_BUFS_BASEC_ADDR        0x0024 //265 RADIX_REG_JRFD_BUFS_BASEC_ADDR
#define REG_JRFD_BUFS_BEYDY_ADDR        0x0028 //265 RADIX_REG_JRFD_BUFS_BEYDY_ADDR
#define REG_JRFD_BUFS_BEYDC_ADDR        0x002C //265 RADIX_REG_JRFD_BUFS_BEYDC_ADDR

#define REG_JRFD_MHDY                   0x0030 //265 RADIX_REG_JRFD_MHDY
#define REG_JRFD_MHDC                   0x0034 //265 RADIX_REG_JRFD_MHDC
#define REG_JRFD_MBDY                   0x0038 //265 RADIX_REG_JRFD_MBDY
#define REG_JRFD_MBDC                   0x003C //265 RADIX_REG_JRFD_MBDC

#define REG_JRFD_MCER                   0x0100 //265 RADIX_REG_JRFD_MCER (mce req cnt)
#define REG_JRFD_MCEB                   0x0104 //265 RADIX_REG_JRFD_MCEB (mce 128bits cnt)
#define REG_JRFD_JRFR                   0x0108 //265 RADIX_REG_JRFD_JRFR (jrfd req cnt)
#define REG_JRFD_JRFB                   0x010C //265 RADIX_REG_JRFD_JRFB (jrfd 128bits cnt)

#define JRFD_BODY_STRDY(y)     (((y) & 0xFFFF)<<16)
#define JRFD_BODY_STRDC(c)     (((c) & 0xFFFF)<<0)

/*************************************************************************************
                  MD Module
*************************************************************************************/
#define RADIX_REG_MD_INIT               0x0
#define RADIX_REG_MD_CFG0               0x4
#define RADIX_REG_MD_CFG1               0x8
#define RADIX_REG_MD_CFG2               0xc
#define RADIX_REG_MD_CFG3               0x10
#define RADIX_REG_MD_CFG4               0x14
#define RADIX_REG_MD_CFG5               0x18
#define RADIX_REG_MD_CFG6               0x1C
#define RADIX_REG_MD_CFG7               0x20
#define RADIX_REG_MD_CFG8               0x24
#define RADIX_REG_MD_SCFG0              0xC0
#define RADIX_REG_MD_SCFG1              0xC4
#define RADIX_REG_MD_SCFG2              0xC8
#define RADIX_REG_MD_SCFG3              0xCC
#define RADIX_REG_MD_RCFG0              0x100
#define RADIX_REG_MD_RCFG1              0x104
#define RADIX_REG_MD_RCFG2              0x108
#define RADIX_REG_MD_RCFG3              0x114
#define RADIX_REG_MD_RCFG4              0x118
#define RADIX_REG_MD_RCFG5              0x11C
#define RADIX_REG_MD_RCFG6              0x120
#define RADIX_REG_MD_RCFG7              0x124
#define MD_SLV_THR_CLR_OFST	        0x164

#define MD_SLV_PMD_QP_THRD_0	        0x5a*4 //0x168
#define MD_SLV_PMD_QP_THRD_1	        0x5b*4

#define MD_SLV_PMD_THR_1_0  	        0x5c*4
#define MD_SLV_PMD_THR_3_2	        0x5d*4
#define MD_SLV_PMD_THR_5_4	        0x5e*4
#define MD_SLV_PMD_THR_7_6	        0x5f*4
#define MD_SLV_PMD_THR_9_8	        0x60*4
#define MD_SLV_PMD_THR_11_10	        0x61*4
#define MD_SLV_PMD_THR_13_12	        0x62*4
#define MD_SLV_PMD_THR_15_14	        0x63*4
#define	MD_SLV_PMD_THR_17_16            0x64*4
#define MD_SLV_PMD_THR_19_18	        0x65*4
#define MD_SLV_PMD_THR_21_20	        0x66*4
#define MD_SLV_PMD_THR_23_22	        0x67*4
#define MD_SLV_PMD_THR_25_24	        0x68*4
#define MD_SLV_PMD_THR_27_26	        0x69*4
#define MD_SLV_PMD_THR_29_28	        0x6a*4

#define MD_SLV_PMD_THR_IDX0	        0x6b*4
#define MD_SLV_PMD_THR_IDX1	        0x6c*4
#define MD_SLV_PMD_THR_IDX2	        0x6d*4
#define	MD_SLV_PMD_THR_IDX3             0x6e*4
#define MD_SLV_PMD_THR_IDX4	        0x6f*4
#define MD_SLV_PMD_THR_IDX5	        0x70*4
#define MD_SLV_PMD_THR_IDX6	        0x71*4
#define MD_SLV_PMD_THR_IDX7	        0x72*4
#define MD_SLV_PMD_THR_IDX8	        0x73*4
#define MD_SLV_PMD_THR_IDX9	        0x74*4
#define MD_SLV_PMD_THR_IDX10	        0x75*4
#define MD_SLV_PMD_THR_IDX11	        0x76*4
#define MD_SLV_PMD_THR_IDX12	        0x77*4
#define MD_SLV_PMD_THR_IDX13	        0x78*4
#define MD_SLV_PMD_THR_IDX14	        0x79*4
#define MD_SLV_PMD_THR_IDX15	        0x7a*4
#define MD_SLV_PMD_THR_IDX16	        0x7b*4
#define MD_SLV_PMD_THR_IDX17	        0x7c*4
#define MD_SLV_PMD_THR_IDX18	        0x7d*4
#define MD_SLV_PMD_THR_IDX19	        0x7e*4
#define MD_SLV_PMD_THR_IDX20	        0x7f*4
#define MD_SLV_PMD_THR_IDX21	        0x80*4

#define MD_SLV_NED_SC_TBL_0	        0x81*4
#define MD_SLV_NED_SC_TBL_1		0x82*4
#define MD_SLV_NED_SC_TBL_2		0x83*4
#define MD_SLV_NED_SC_TBL_3		0x84*4
#define MD_SLV_NED_SC_TBL_4		0x85*4
#define	MD_SLV_NED_SC_TBL_5		0x86*4
#define	MD_SLV_NED_SC_TBL_6		0x87*4
#define	MD_SLV_NED_SC_TBL_7		0x88*4
#define	MD_SLV_NED_SC_TBL_8		0x89*4
#define MD_SLV_NED_SC_BIAS_0		0x8a*4
#define MD_SLV_NED_SC_BIAS_1		0x8b*4
#define MD_SLV_NED_DISA_0		0x8c*4
#define MD_SLV_NED_DISA_1		0x8d*4
#define MD_SLV_NED_DISA_2		0x8e*4
#define	MD_SLV_NED_DISA_3		0x8f*4
#define	MD_SLV_NED_RCN_PLAIN_CFG_0	0x90*4
#define	MD_SLV_NED_RCN_PLAIN_CFG_1	0x91*4
#define	MD_SLV_NED_RCN_PLAIN_CFG_2	0x92*4
#define	MD_SLV_NED_RCN_PLAIN_CFG_3	0x93*4
#define	MD_SLV_NED_RCN_PLAIN_CFG_4	0x94*4
#define	MD_SLV_CSD_QP_THRD_0	        0x95*4
#define	MD_SLV_CSD_QP_THRD_1     	0x96*4
#define	MD_SLV_MOSAIC_CFG_0		0x97*4
#define	MD_SLV_MOSAIC_CFG_1	        0x98*4
#define	MD_SLV_DLAMBDA_CFG0	        0x99*4
#define	MD_SLV_DLAMBDA_CFG1	        0xA0*4
#define MD_SLV_CSS_CFG0                 0xA1*4
#define MD_SLV_CSS_CFG1                 0xA2*4
#define MD_SLV_CSS_CFG2                 0xA3*4
#define MD_SLV_CSS_CFG3                 0xA4*4
#define T33_MD_SLV_CSE_CFG0                 0xA5*4
#define T33_MD_SLV_CSE_CFG1                 0xA6*4
#define T33_MD_SLV_CSE_CFG2                 0xA7*4
#define T33_RADIX_REG_MD_RCFG8              0xA8*4
#define T33_RADIX_REG_MD_RCFG9              0xA9*4

#define T33_RADIX_MD_FPGA_PMD_CSD_IDX       0x160*4
#define T33_RADIX_MD_FPGA_TFM_PM_IDX        0x161*4
#define T33_RADIX_MD_FPGA_BC_SE_IDX         0x162*4
#define T33_RADIX_MD_FPGA_BC_CF_IDX         0x163*4
#define T33_RADIX_MD_FPGA_SDE_SE_IDX        0x164*4
#define T33_RADIX_MD_FPGA_DBLK_ECHO_IDX     0x165*4

#define T33_RADIX_MD_FPGA_PMD_CSD_DATA0     0x180*4
#define T33_RADIX_MD_FPGA_PMD_CSD_DATA1     0x181*4
#define T33_RADIX_MD_FPGA_PMD_CSD_DATA2     0x182*4
#define T33_RADIX_MD_FPGA_PMD_CSD_DATA3     0x183*4


#define T33_RADIX_MD_FPGA_TFM_PM_DATA0      0x188*4
#define T33_RADIX_MD_FPGA_TFM_PM_DATA1      0x189*4
#define T33_RADIX_MD_FPGA_BC_SE_DATA0       0x190*4
#define T33_RADIX_MD_FPGA_BC_SE_DATA1       0x191*4
#define T33_RADIX_MD_FPGA_BC_SE_DATA2       0x192*4

#define T33_RADIX_MD_FPGA_BC_CF_DATA0       0x198*4
#define T33_RADIX_MD_FPGA_BC_CF_DATA1       0x199*4
#define T33_RADIX_MD_FPGA_BC_CF_DATA2       0x19a*4
#define T33_RADIX_MD_FPGA_BC_CF_DATA3       0x19b*4
#define T33_RADIX_MD_FPGA_BC_CF_DATA4       0x19c*4
#define T33_RADIX_MD_FPGA_BC_CF_DATA5       0x19d*4
#define T33_RADIX_MD_FPGA_BC_CF_DATA6       0x19e*4
#define T33_RADIX_MD_FPGA_BC_CF_DATA7       0x19f*4

#define T33_RADIX_MD_FPGA_SDE_SE_DATA0       0x1a0*4
#define T33_RADIX_MD_FPGA_SDE_SE_DATA1       0x1a1*4
#define T33_RADIX_MD_FPGA_SDE_SE_DATA2       0x1a2*4
#define T33_RADIX_MD_FPGA_SDE_SE_DATA3       0x1a3*4
#define T33_RADIX_MD_FPGA_SDE_SE_DATA4       0x1a4*4

#define T33_RADIX_MD_FPGA_DBLK_ECHO_DATA0    0x1a8*4
#define T33_RADIX_MD_FPGA_DBLK_ECHO_DATA1    0x1a9*4
#define T33_RADIX_MD_FPGA_DBLK_ECHO_DATA2    0x1aa*4
#define T33_RADIX_MD_FPGA_DBLK_ECHO_DATA3    0x1ab*4
#define T33_RADIX_MD_FPGA_DBLK_ECHO_DATA4    0x1ac*4
#define T33_RADIX_MD_FPGA_DBLK_ECHO_DATA5    0x1ad*4
#define T33_RADIX_MD_FPGA_DBLK_ECHO_DATA6    0x1ae*4
#define T33_RADIX_MD_FPGA_DBLK_ECHO_DATA7    0x1af*4

/*************************************************************************************
                  MCE Module
*************************************************************************************/
#define RADIX_EXPAND_SIZE             576 //512+64
#define RADIX_MAX_SECH_STEP           13  //64
#define RADIX_MAX_MVD                 2048

//GLB_CTRL
#define RADIX_REG_MCE_GLB_CTRL        0x0000
#define RADIX_MCE_GLB_CTRL_FSAD(a)    (((a) & 0x1)<<31)
#define RADIX_MCE_GLB_CTRL_MFS(a)     (((a) & 0x7)<<28)
#define RADIX_MCE_GLB_CTRL_FMZ(a)     (((a) & 0x1)<<27)
#define RADIX_MCE_GLB_CTRL_PREF(a)    (((a) & 0x1)<<26)
#define T33_RADIX_MCE_GLB_CTRL_SRM(a)     (((a) & 0x3)<<19)
#define RADIX_MCE_GLB_CTRL_VTYPE(a)   (((a) & 0x3)<<17)
#define RADIX_MCE_GLB_CTRL_SRD(a)     (((a) & 0x1)<<16)
#define RADIX_MCE_GLB_CTRL_MWT(a)     (((a) & 0x1)<<15)
#define RADIX_MCE_GLB_CTRL_RMV(a)     (((a) & 0x1)<<12)
#define RADIX_MCE_GLB_CTRL_TMVP(a)    (((a) & 0x1)<<11)
#define RADIX_MCE_GLB_CTRL_RESI(a)    (((a) & 0x7)<<8)  //(cu32, cu16, cu8)
#define RADIX_MCE_GLB_CTRL_FMT(a)     (((a) & 0x1)<<6)  //encode formate 00:x265, 01:svac
#define RADIX_MCE_GLB_CTRL_ED(a)      (((a) & 0x1)<<5)  //0:encode, 1:decode
#define RADIX_MCE_GLB_CTRL_BF         (0x1<<3)
#define RADIX_MCE_GLB_CTRL_CGE        (0x1<<2)
#define RADIX_MCE_GLB_CTRL_WM         (0x1<<1)
#define RADIX_MCE_GLB_CTRL_INIT       (0x1<<0)

#define REG_MCE_GLB_CTRL              0x0000 //265 RADIX_REG_MCE_GLB_CTRL
#ifdef USE_HERA_MCE
#define MCE_GLB_CTRL_DCT8(a)          (((a) & 0x1)<<14)
#define MCE_GLB_CTRL_MREF(a)          (((a) & 0x1)<<12)
#else
#define MCE_GLB_CTRL_MREF(a)          (((a) & 0x1)<<6)
#define MCE_GLB_CTRL_DCT8(a)          (((a) & 0x1)<<8)
#endif

#define MCE_GLB_CTRL_VTYPE(a)         (((a) & 0x3)<<17)
#define MCE_GLB_CTRL_PS(a)            (((a) & 0x1)<<13)
#define MCE_GLB_CTRL_PREF(a)          (((a) & 0x1)<<26) //265 RADIX_MCE_GLB_CTRL_PREF(a)
#define MCE_GLB_CTRL_TMVP(a)          (((a) & 0x1)<<11) //265 RADIX_MCE_GLB_CTRL_TMVP(a)
#define MCE_GLB_CTRL_RESI(a)          (((a) & 0x7)<<8)  //265 RADIX_MCE_GLB_CTRL_RESI(a) (cu32, cu16, cu8)
#define MCE_GLB_CTRL_FMT(a)           (((a) & 0x3)<<6)  //265 RADIX_MCE_GLB_CTRL_FMT(a)  encode formate 00:x265, 01:svac, 10:x264
#define MCE_GLB_CTRL_ED(a)            (((a) & 0x1)<<5)  //265 RADIX_MCE_GLB_CTRL_ED(a)   0:encode, 1:decode

#define MCE_GLB_CTRL_BF               (0x1<<3) //265 RADIX_MCE_GLB_CTRL_BF
#define MCE_GLB_CTRL_CGE              (0x1<<2) //265 RADIX_MCE_GLB_CTRL_CGE
#define MCE_GLB_CTRL_WM               (0x1<<1) //265 RADIX_MCE_GLB_CTRL_WM
#define MCE_GLB_CTRL_INIT             (0x1<<0) //265 RADIX_MCE_GLB_CTRL_INIT

#define T33_RADIX_REG_MCE_BUS_CTRL        0x0004
#define RADIX_MCE_BUS_CTRL_OTPY(a)    (((a) & 0x1F)<<0) //T33
#define RADIX_MCE_BUS_CTRL_OTPC(a)    (((a) & 0x1F)<<5) //T33
#define RADIX_MCE_BUS_CTRL_OTMY(a)    (((a) & 0x1F)<<10) //T33
#define RADIX_MCE_BUS_CTRL_OTMC(a)    (((a) & 0x1F)<<15) //T33
#define RADIX_MCE_BUS_CTRL_PROR(a)    (((a) & 0x1 )<<20) //T33
#define RADIX_MCE_BUS_CTRL_ATHR(a)    (((a) & 0x7 )<<21) //T33
#define RADIX_MCE_BUS_CTRL_SENA(a)    (((a) & 0x1 )<<25) //T33
#define RADIX_MCE_BUS_CTRL_BFSH(a)    (((a) & 0x1 )<<26) //T33

//COMP_CTRL
#define RADIX_REG_MCE_COMP_CTRL       0x0010
#define RADIX_MCE_COMP_CTRL_CCE       (0x1<<31)
#define RADIX_MCE_COMP_CTRL_CWT(a)    (((a) & 0x3)<<26)
#define RADIX_MCE_COMP_CTRL_CRR       (0x1<<25)
#define RADIX_MCE_COMP_CTRL_CIC       (0x1<<24)
#define RADIX_MCE_COMP_CTRL_CAT       (0x1<<23)
#define RADIX_MCE_COMP_CTRL_CTAP(a)   (((a) & 0x3)<<20)
#define RADIX_MCE_COMP_CTRL_CSPT(a)   (((a) & 0x3)<<18)
#define RADIX_MCE_COMP_CTRL_CSPP(a)   (((a) & 0x3)<<16)
#define RADIX_MCE_COMP_CTRL_YCE       (0x1<<15)
#define RADIX_MCE_COMP_CTRL_YWT(a)    (((a) & 0x3)<<10)
#define RADIX_MCE_COMP_CTRL_YRR       (0x1<<9)
#define RADIX_MCE_COMP_CTRL_YIC       (0x1<<8)
#define RADIX_MCE_COMP_CTRL_YAT       (0x1<<7)
#define RADIX_MCE_COMP_CTRL_YTAP(a)   (((a) & 0x3)<<4)
#define RADIX_MCE_COMP_CTRL_YSPT(a)   (((a) & 0x3)<<2)
#define RADIX_MCE_COMP_CTRL_YSPP(a)   (((a) & 0x3)<<0)

#define RADIX_MCE_WT_BIAVG            0
#define RADIX_MCE_WT_UNIWT            1
#define RADIX_MCE_WT_BIWT             2
#define RADIX_MCE_WT_IMWT             3

#define RADIX_MCE_TAP_TAP2            0
#define RADIX_MCE_TAP_TAP4            1
#define RADIX_MCE_TAP_TAP6            2
#define RADIX_MCE_TAP_TAP8            3

#define RADIX_MCE_SPT_AUTO            0
#define RADIX_MCE_SPT_SPEC            1
#define RADIX_MCE_SPT_BILI            2
#define RADIX_MCE_SPT_SYMM            3

#define RADIX_MCE_SPP_HPEL            0
#define RADIX_MCE_SPP_QPEL            1
#define RADIX_MCE_SPP_EPEL            2

#define REG_MCE_COMP_CTRL             0x0010            //265 RADIX_REG_MCE_COMP_CTRL
#define MCE_COMP_CTRL_CCE             (0x1<<31)         //265 RADIX_MCE_COMP_CTRL_CCE
#define MCE_COMP_CTRL_CWT(a)          (((a) & 0x3)<<26) //265 RADIX_MCE_COMP_CTRL_CWT(a)
#define MCE_COMP_CTRL_CRR             (0x1<<25)         //265 RADIX_MCE_COMP_CTRL_CRR
#define MCE_COMP_CTRL_CIC             (0x1<<24)         //265 RADIX_MCE_COMP_CTRL_CIC
#define MCE_COMP_CTRL_CAT             (0x1<<23)         //265 RADIX_MCE_COMP_CTRL_CAT
#define MCE_COMP_CTRL_CTAP(a)         (((a) & 0x3)<<20) //265 RADIX_MCE_COMP_CTRL_CTAP(a)
#define MCE_COMP_CTRL_CSPT(a)         (((a) & 0x3)<<18) //265 RADIX_MCE_COMP_CTRL_CSPT(a)
#define MCE_COMP_CTRL_CSPP(a)         (((a) & 0x3)<<16) //265 RADIX_MCE_COMP_CTRL_CSPP(a)
#define MCE_COMP_CTRL_YCE             (0x1<<15)         //265 RADIX_MCE_COMP_CTRL_YCE
#define MCE_COMP_CTRL_YWT(a)          (((a) & 0x3)<<10) //265 RADIX_MCE_COMP_CTRL_YWT(a)
#define MCE_COMP_CTRL_YRR             (0x1<<9)          //265 RADIX_MCE_COMP_CTRL_YRR
#define MCE_COMP_CTRL_YIC             (0x1<<8)          //265 RADIX_MCE_COMP_CTRL_YIC
#define MCE_COMP_CTRL_YAT             (0x1<<7)          //265 RADIX_MCE_COMP_CTRL_YAT
#define MCE_COMP_CTRL_YTAP(a)         (((a) & 0x3)<<4)  //265 RADIX_MCE_COMP_CTRL_YTAP(a)
#define MCE_COMP_CTRL_YSPT(a)         (((a) & 0x3)<<2)  //265 RADIX_MCE_COMP_CTRL_YSPT(a)
#define MCE_COMP_CTRL_YSPP(a)         (((a) & 0x3)<<0)  //265 RADIX_MCE_COMP_CTRL_YSPP(a)

#define MCE_WT_BIAVG                  0 //265 RADIX_MCE_WT_BIAVG
#define MCE_WT_UNIWT                  1 //265 RADIX_MCE_WT_UNIWT
#define MCE_WT_BIWT                   2 //265 RADIX_MCE_WT_BIWT
#define MCE_WT_IMWT                   3 //265 RADIX_MCE_WT_IMWT

#define MCE_TAP_TAP2                  0 //265 RADIX_MCE_TAP_TAP2
#define MCE_TAP_TAP4                  1 //265 RADIX_MCE_TAP_TAP4
#define MCE_TAP_TAP6                  2 //265 RADIX_MCE_TAP_TAP6
#define MCE_TAP_TAP8                  3 //265 RADIX_MCE_TAP_TAP8

#define MCE_SPT_AUTO                  0 //265 RADIX_MCE_SPT_AUTO
#define MCE_SPT_SPEC                  1 //265 RADIX_MCE_SPT_SPEC
#define MCE_SPT_BILI                  2 //265 RADIX_MCE_SPT_BILI
#define MCE_SPT_SYMM                  3 //265 RADIX_MCE_SPT_SYMM

#define MCE_SPP_HPEL                  0 //265 RADIX_MCE_SPP_HPEL
#define MCE_SPP_QPEL                  1 //265 RADIX_MCE_SPP_QPEL
#define MCE_SPP_EPEL                  2 //265 RADIX_MCE_SPP_EPEL

#define T33_RADIX_REG_MCE_RCC_CTRL        0x002C
#define T33_RADIX_REG_MCE_RCC_CFG0        0x0030
#define T33_RADIX_REG_MCE_RCC_CFG1        0x0034
#define T33_RADIX_REG_MCE_RCC_CFG2        0x0038
#define T33_RADIX_REG_MCE_RCC_CFG3        0x003C

#define RADIX_MCE_RCC_TICK0(a)    (((a) & 0xFF)<<0)//T33
#define RADIX_MCE_RCC_TICK1(a)    (((a) & 0xFF)<<8)//T33
#define RADIX_MCE_RCC_TICK2(a)    (((a) & 0xFF)<<16)//T33
#define RADIX_MCE_RCC_TICK3(a)    (((a) & 0xFF)<<24)//T33

//ESTI_CTRL
#define RADIX_REG_MCE_ESTI_CTRL       0x0040
#define RADIX_MCE_ESTI_CTRL_FBG(a)    (((a) & 0x1)<<28)
#define RADIX_MCE_ESTI_CTRL_BDIR(a)   (((a) & 0x1)<<27)
#define RADIX_MCE_ESTI_CTRL_CLMV      (0x1<<26)
#define RADIX_MCE_ESTI_CTRL_SCL(a)    (((a) & 0x3)<<24)
#define RADIX_MCE_ESTI_CTRL_MSS(a)    (((a) & 0xFF)<<16)
#define RADIX_MCE_ESTI_CTRL_QRL(a)    (((a) & 0x3)<<14)
#define RADIX_MCE_ESTI_CTRL_HRL(a)    (((a) & 0x3)<<12)
#define RADIX_MCE_ESTI_CTRL_RF8(a)    (((a) & 0x1)<<29)
#define RADIX_MCE_ESTI_CTRL_RF16(a)   (((a) & 0x1)<<30)
#define RADIX_MCE_ESTI_CTRL_RF32(a)   (((a) & 0x1)<<31)
#define RADIX_MCE_ESTI_CTRL_GMV_EN(a) (((a) & 0x1)<<11)
#define RADIX_MCE_ESTI_CTRL_PUE_64X64 (0x1<<9)
#define RADIX_MCE_ESTI_CTRL_PUE_32X32 (0x1<<6)
#define RADIX_MCE_ESTI_CTRL_PUE_32X16 (0x1<<5)
#define RADIX_MCE_ESTI_CTRL_PUE_16X32 (0x1<<4)
#define RADIX_MCE_ESTI_CTRL_PUE_16X16 (0x1<<3)
#define RADIX_MCE_ESTI_CTRL_PUE_16X8  (0x1<<2)
#define RADIX_MCE_ESTI_CTRL_PUE_8X16  (0x1<<1)
#define RADIX_MCE_ESTI_CTRL_PUE_8X8   (0x1<<0)

#define REG_MCE_ESTI_CTRL             0x0040             //265 RADIX_REG_MCE_ESTI_CTRL
#define MCE_ESTI_CTRL_LSP(a)          (((a) & 0xF)<<28)
#define MCE_ESTI_CTRL_FBG(a)          (((a) & 0x1)<<28)  //265 RADIX_MCE_ESTI_CTRL_FBG(a)
#define MCE_ESTI_CTRL_CLMV            (0x1<<26)          //265 RADIX_MCE_ESTI_CTRL_CLMV
#define MCE_ESTI_CTRL_SCL(a)          (((a) & 0x3)<<24)  //265 RADIX_MCE_ESTI_CTRL_SCL(a)
#define MCE_ESTI_CTRL_MSS(a)          (((a) & 0xFF)<<16) //265 RADIX_MCE_ESTI_CTRL_MSS(a)
#define MCE_ESTI_CTRL_QRL(a)          (((a) & 0x3)<<14)  //265 RADIX_MCE_ESTI_CTRL_QRL(a)
#define MCE_ESTI_CTRL_HRL(a)          (((a) & 0x3)<<12)  //265 RADIX_MCE_ESTI_CTRL_HRL(a)
#define MCE_ESTI_CTRL_RF8(a)          (((a) & 0x1)<<29)  //265 RADIX_MCE_ESTI_CTRL_RF8(a)
#define MCE_ESTI_CTRL_RF16(a)         (((a) & 0x1)<<30)  //265 RADIX_MCE_ESTI_CTRL_RF16(a)
#define MCE_ESTI_CTRL_RF32(a)         (((a) & 0x1)<<31)  //265 RADIX_MCE_ESTI_CTRL_RF32(a)

#ifdef USE_HERA_MCE
#define MCE_ESTI_CTRL_BDIR(a)         (((a) & 0x1)<<27) //265 RADIX_MCE_ESTI_CTRL_BDIR(a)
#else
#define MCE_ESTI_CTRL_BDIR(a)         (((a) & 0x1)<<10) //265 RADIX_MCE_ESTI_CTRL_BDIR(a)
#endif

#define MCE_ESTI_CTRL_GMV_EN(a)       (((a) & 0x1)<<11) //265 RADIX_MCE_ESTI_CTRL_GMV_EN(a)
#define MCE_ESTI_CTRL_PUE_64X64       (0x1<<9)          //265 RADIX_MCE_ESTI_CTRL_PUE_64X64
#define MCE_ESTI_CTRL_PUE_32X32       (0x1<<6)          //265 RADIX_MCE_ESTI_CTRL_PUE_32X32
#define MCE_ESTI_CTRL_PUE_32X16       (0x1<<5)          //265 RADIX_MCE_ESTI_CTRL_PUE_32X16
#define MCE_ESTI_CTRL_PUE_16X32       (0x1<<4)          //265 RADIX_MCE_ESTI_CTRL_PUE_16X32
#define MCE_ESTI_CTRL_PUE_16X16       (0x1<<3)          //265 RADIX_MCE_ESTI_CTRL_PUE_16X16
#define MCE_ESTI_CTRL_PUE_16X8        (0x1<<2)          //265 RADIX_MCE_ESTI_CTRL_PUE_16X8
#define MCE_ESTI_CTRL_PUE_8X16        (0x1<<1)          //265 RADIX_MCE_ESTI_CTRL_PUE_8X16
#define MCE_ESTI_CTRL_PUE_8X8         (0x1<<0)          //265 RADIX_MCE_ESTI_CTRL_PUE_8X8
//MRGI
#define RADIX_REG_MCE_MRGI            0x0044

#define RADIX_MCE_MRGI_FPEL_CU32     (0x1<<26)
#define RADIX_MCE_MRGI_FPEL_CU16     (0x1<<25)
#define RADIX_MCE_MRGI_FPEL_CU8      (0x1<<24)

#define RADIX_MCE_MRGI_MRGE1_CU32    (0x1<<15)
#define RADIX_MCE_MRGI_MRGE0_CU32    (0x1<<14)
#define RADIX_MCE_MRGI_MRGE1_CU16    (0x1<<13)
#define RADIX_MCE_MRGI_MRGE0_CU16    (0x1<<12)
#define RADIX_MCE_MRGI_MRGE1_CU8     (0x1<<11)
#define RADIX_MCE_MRGI_MRGE0_CU8     (0x1<<10)
#define RADIX_MCE_MRGI_MRGE_64X64    (0x1<<9)
#define RADIX_MCE_MRGI_MRGE_32X32    (0x1<<6)
#define RADIX_MCE_MRGI_MRGE_32X16    (0x1<<5)
#define RADIX_MCE_MRGI_MRGE_16X32    (0x1<<4)
#define RADIX_MCE_MRGI_MRGE_16X16    (0x1<<3)
#define RADIX_MCE_MRGI_MRGE_16X8     (0x1<<2)
#define RADIX_MCE_MRGI_MRGE_8X16     (0x1<<1)
#define RADIX_MCE_MRGI_MRGE_8X8      (0x1<<0)

#define REG_MCE_MRGI                 0x0044   //265 RADIX_REG_MCE_MRGI

#define MCE_MRGI_MRGE_64X64          (0x1<<9) //265 RADIX_MCE_MRGI_MRGE_64X64
#define MCE_MRGI_MRGE_32X32          (0x1<<6) //265 RADIX_MCE_MRGI_MRGE_32X32
#define MCE_MRGI_MRGE_32X16          (0x1<<5) //265 RADIX_MCE_MRGI_MRGE_32X16
#define MCE_MRGI_MRGE_16X32          (0x1<<4) //265 RADIX_MCE_MRGI_MRGE_16X32
#define MCE_MRGI_MRGE_16X16          (0x1<<3) //265 RADIX_MCE_MRGI_MRGE_16X16
#define MCE_MRGI_MRGE_16X8           (0x1<<2) //265 RADIX_MCE_MRGI_MRGE_16X8
#define MCE_MRGI_MRGE_8X16           (0x1<<1) //265 RADIX_MCE_MRGI_MRGE_8X16
#define MCE_MRGI_MRGE_8X8            (0x1<<0) //265 RADIX_MCE_MRGI_MRGE_8X8
//MVR
#define RADIX_REG_MCE_MVR             0x0048
#define RADIX_MCE_MVR_MVRY(a)         (((a) & 0xFFFF)<<16)
#define RADIX_MCE_MVR_MVRX(a)         (((a) & 0xFFFF)<<0)

#define REG_MCE_MVR                   0x0048               //265 RADIX_REG_MCE_MVR
#define MCE_MVR_MVRY(a)               (((a) & 0xFFFF)<<16) //265 RADIX_MCE_MVR_MVRY
#define MCE_MVR_MVRX(a)               (((a) & 0xFFFF)<<0)  //265 RADIX_MCE_MVR_MVRX
//RESI THR
#define RADIX_REG_MCE_RESI_TST        0x004C
#define RADIX_MCE_RTST7(a)            (((a) & 0xF)<<28)
#define RADIX_MCE_RTST6(a)            (((a) & 0xF)<<24)
#define RADIX_MCE_RTST5(a)            (((a) & 0xF)<<20)
#define RADIX_MCE_RTST4(a)            (((a) & 0xF)<<16)
#define RADIX_MCE_RTST3(a)            (((a) & 0xF)<<12)
#define RADIX_MCE_RTST2(a)            (((a) & 0xF)<<8)
#define RADIX_MCE_RTST1(a)            (((a) & 0xF)<<4)
#define RADIX_MCE_RTST0(a)            (((a) & 0xF)<<0)
#define RADIX_REG_MCE_RESI_TDS        0x0050
#define RADIX_MCE_RTDF1(a)            (((a) & 0xFFFF)<<16)
#define RADIX_MCE_RTSC0(a)            (((a) & 0xFFFF)<<0)
#define RADIX_REG_MCE_RESI_TMAX       0x0054
#define RADIX_MCE_RTMAX1(a)           (((a) & 0xFFFF)<<16)
#define RADIX_MCE_RTMAX0(a)           (((a) & 0xFFFF)<<0)
//PREF_EXPAND
#define RADIX_REG_MCE_PREF_EXPD       0x005C
#define RADIX_MCE_PREF_EXPD_L(a)      (((a) & 0x7F)<<24)
#define RADIX_MCE_PREF_EXPD_R(a)      (((a) & 0x7F)<<16)
#define RADIX_MCE_PREF_EXPD_D(a)      (((a) & 0x7F)<<8)
#define RADIX_MCE_PREF_EXPD_T(a)      (((a) & 0x7F)<<0)

#define REG_MCE_PREF_EXPD             0x005C             //265 RADIX_REG_MCE_PREF_EXPD
#define MCE_PREF_EXPD_L(a)            (((a) & 0x7F)<<24) //265 RADIX_MCE_PREF_EXPD_L(a)
#define MCE_PREF_EXPD_R(a)            (((a) & 0x7F)<<16) //265 RADIX_MCE_PREF_EXPD_R(a)
#define MCE_PREF_EXPD_D(a)            (((a) & 0x7F)<<8)  //265 RADIX_MCE_PREF_EXPD_D(a)
#define MCE_PREF_EXPD_T(a)            (((a) & 0x7F)<<0)  //265 RADIX_MCE_PREF_EXPD_T(a)
//FRM_SIZE
#define RADIX_REG_MCE_FRM_SIZE        0x0060
#define RADIX_MCE_FRM_SIZE_FH(a)      (((a) & 0xFFFF)<<16)
#define RADIX_MCE_FRM_SIZE_FW(a)      (((a) & 0xFFFF)<<0)

#define REG_MCE_FRM_SIZE              0x0060               //265 RADIX_REG_MCE_FRM_SIZE
#define MCE_FRM_SIZE_FH(a)            (((a) & 0xFFFF)<<16) //265 RADIX_MCE_FRM_SIZE_FH(a)
#define MCE_FRM_SIZE_FW(a)            (((a) & 0xFFFF)<<0)  //265 RADIX_MCE_FRM_SIZE_FW(a)
#define MCE_FRM_SIZE_LRE(a)           (((a) & 0x1)<<14)
#define MCE_FRM_SIZE_RRE(a)           (((a) & 0x1)<<15)
#define MCE_FRM_SIZE_TRE(a)           (((a) & 0x1)<<30)
#define MCE_FRM_SIZE_BRE(a)           (((a) & 0x1)<<31)
//FRM_STRD
#define RADIX_REG_MCE_FRM_STRD        0x0064
#define RADIX_MCE_FRM_STRD_STRDC(a)   (((a) & 0xFFFF)<<16)
#define RADIX_MCE_FRM_STRD_STRDY(a)   (((a) & 0xFFFF)<<0)

#define REG_MCE_FRM_STRD              0x0064               //265 RADIX_REG_MCE_FRM_STRD
#define MCE_FRM_STRD_STRDC(a)         (((a) & 0xFFFF)<<16) //265 RADIX_MCE_FRM_STRD_STRDC(a)
#define MCE_FRM_STRD_STRDY(a)         (((a) & 0xFFFF)<<0)  //265 RADIX_MCE_FRM_STRD_STRDY(a)
//SLC_SPOS
#define RADIX_REG_MCE_SLC_SPOS        0x0068
#define RADIX_MCE_SLC_SPOS_CU64Y(a)   (((a) & 0xFF)<<8)
#define RADIX_MCE_SLC_SPOS_CU64X(a)   (((a) & 0xFF)<<0)

#define REG_MCE_SLC_SPOS              0x0068              //265 RADIX_REG_MCE_SLC_SPOS
#define MCE_SLC_SPOS_CU64Y(a)         (((a) & 0xFF)<<8)   //265 RADIX_MCE_SLC_SPOS_CU64Y(a)
#define MCE_SLC_SPOS_CU64X(a)         (((a) & 0xFF)<<0)   //265 RADIX_MCE_SLC_SPOS_CU64X(a)
//SLC_MV
#define RADIX_REG_MCE_SLC_MV          0x006C
#define RADIX_MCE_SLC_MVY(a)          (((a) & 0x3FF)<<16)
#define RADIX_MCE_SLC_MVX(a)          (((a) & 0x3FF)<<0)

#ifdef USE_HERA_MCE
#define REG_MCE_SLC_MV                0x006C              //265 RADIX_REG_MCE_SLC_MV
#define MCE_SLC_MVX(a)                (((a) & 0x3FF)<<0)  //265 RADIX_MCE_SLC_MVY(a)
#define MCE_SLC_MVY(a)                (((a) & 0x3FF)<<16) //265 RADIX_MCE_SLC_MVX(a)
#endif
//MCE BUffER SHARE
#define RADIX_REG_MCE_BFSH_ADRY       0x0070
#define RADIX_REG_MCE_BFSH_ADRC       0x0074
#define T32_RADIX_REG_MCE_BOTM_ID         0x0078
#define T33_RADIX_REG_MCE_BYND_YADDR      0x0078
#define T33_RADIX_REG_MCE_BYND_CADDR      0x007C
#define RADIX_REG_MCE_MREF_ADRY       0x01B8
#define RADIX_REG_MCE_MREF_ADRC       0x01BC

#define REG_MCE_BFSH_ADRY             0x0070 //265 RADIX_REG_MCE_BFSH_ADRY
#define REG_MCE_BFSH_ADRC             0x0074 //265 RADIX_REG_MCE_BFSH_ADRC
#define REG_MCE_BOTM_ID               0x0078 //265 RADIX_REG_MCE_BOTM_ID
#define REG_MCE_MREF_ADRY             0x01B8 //265 RADIX_REG_MCE_MREF_ADRY
#define REG_MCE_MREF_ADRC             0x01BC //265 RADIX_REG_MCE_MREF_ADRC
//RLUT
#define RADIX_SLUT_MCE_RLUT(l, i)     (0x0800 + (i)*8 + (l)*0x80)
#define SLUT_MCE_RLUT(l, i)           (0x0800 + (i)*8 + (l)*0x80) //265 RADIX_SLUT_MCE_RLUT
//ILUT
#define RADIX_SLUT_MCE_ILUT_Y         0x0900
#define RADIX_SLUT_MCE_ILUT_C         0x0980
#define RADIX_MCE_ILUT_INFO(fir, clip, idgl, edgl, dir,		\
			    rnd, sft, savg, srnd, sbias)	\
  ( ((fir) & 0x1)<<31 |						\
    ((clip) & 0x1)<<27 |					\
    ((idgl) & 0x1)<<26 |					\
    ((edgl) & 0x1)<<25 |					\
    ((dir) & 0x1)<<24 |						\
    ((rnd) & 0xFF)<<16 |					\
    ((sft) & 0xF)<<8 |						\
    ((savg) & 0x1)<<2 |						\
    ((srnd) & 0x1)<<1 |						\
    ((sbias) & 0x1)<<0						\
    )

#define SLUT_MCE_ILUT_Y               0x0900 //265 RADIX_SLUT_MCE_ILUT_Y
#define SLUT_MCE_ILUT_C               0x0980 //265 RADIX_SLUT_MCE_ILUT_C
#define MCE_ILUT_INFO(fir, clip, idgl, edgl, dir,        \
                      rnd, sft, savg, srnd, sbias)       \
( ((fir) & 0x1)<<31 |                                    \
  ((clip) & 0x1)<<27 |                                   \
  ((idgl) & 0x1)<<26 |                                   \
  ((edgl) & 0x1)<<25 |                                   \
  ((dir) & 0x1)<<24 |                                    \
  ((rnd) & 0xFF)<<16 |                                   \
  ((sft) & 0xF)<<8 |                                     \
  ((savg) & 0x1)<<2 |                                    \
  ((srnd) & 0x1)<<1 |                                    \
  ((sbias) & 0x1)<<0                                     \
)
//CLUT
#define RADIX_SLUT_MCE_CLUT_Y         0x0A00
#define RADIX_SLUT_MCE_CLUT_C         0x0B00
#define RADIX_MCE_CLUT_INFO(c4, c3, c2, c1)	\
  ( ((c4) & 0xFF)<<24 |				\
    ((c3) & 0xFF)<<16 |				\
    ((c2) & 0xFF)<<8 |				\
    ((c1) & 0xFF)<<0				\
    )

#define SLUT_MCE_CLUT_Y               0x0A00 //265 RADIX_SLUT_MCE_CLUT_Y
#define SLUT_MCE_CLUT_C               0x0B00 //265 RADIX_SLUT_MCE_CLUT_C
#define MCE_CLUT_INFO(c4, c3, c2, c1)                    \
( ((c4) & 0xFF)<<24 |                                    \
  ((c3) & 0xFF)<<16 |                                    \
  ((c2) & 0xFF)<<8 |                                     \
  ((c1) & 0xFF)<<0                                       \
)
//FSC
#define REG_MCE_FSC                   0x004C
#define MCE_FSC_FSE(a)                (((a) & 0x1)<<31)
#define MCE_FSC_FSMD(a)               (((a) & 0x1)<<30)
#define MCE_FSC_RECY(a)               (((a) & 0x7F)<<24)
#define MCE_FSC_RECX(a)               (((a) & 0xFF)<<16)
#define MCE_FSC_PERY(a)               (((a) & 0xFF)<<8)
#define MCE_FSC_PERX(a)               (((a) & 0xFF)<<0)
//REF
#define REG_MCE_REFY0                 0x006C
#define REG_MCE_REFC0                 0x0070

#define REG_MCE_REFY1                 0x0074
#define REG_MCE_REFC1                 0x0078

#define REG_MCE_REFY0_1               0x0110  //!!!
#define REG_MCE_REFC0_1               0x0114
//GLB MV
#define REG_MCE_GLB_MV                0x007C
#define MCE_GLB_MVX(a)                (((a) & 0xFFF)<<0)
#define MCE_GLB_MVY(a)                (((a) & 0xFFF)<<16)
//CHAIN/TMV/SYNC ADDR
#define REG_MCE_TMV_BA                0x0104
#define REG_MCE_CHN_SYNC              0x0108
#define REG_MCE_CHN_BA                0x010C
//COMP GAP NME & MRG1
#define T33_RADIX_REG_COMP_GAP            0x01c0
#define T33_RADIX_MCE_COMP_GAP_CU8(a)     (((a) & 0xF)<<0)
#define T33_RADIX_MCE_COMP_GAP_CU16(a)    (((a) & 0xF)<<4)
#define T33_RADIX_MCE_COMP_GAP_CU32(a)    (((a) & 0xF)<<8)
//FPGA
#define T33_RADIX_REG_MCE_MCE_FPGA_DBG_Y  0x0128
#define T33_RADIX_REG_MCE_MCE_FPGA_DBG_U  0x012c
#define T33_RADIX_REG_MCE_MCE_FPGA_DBG_V  0x0130

#define T33_RADIX_REG_MCE_FPGA_DBG_MPT_0  0x0200
#define	T33_RADIX_REG_MCE_FPGA_DBG_MPT_1  0x0204
#define	T33_RADIX_REG_MCE_FPGA_DBG_MPT_2  0x0208
#define	T33_RADIX_REG_MCE_FPGA_DBG_MPT_3  0x020c
#define	T33_RADIX_REG_MCE_FPGA_DBG_MPT_4  0x0210
#define	T33_RADIX_REG_MCE_FPGA_DBG_MPT_5  0x0214
#define	T33_RADIX_REG_MCE_FPGA_DBG_MPT_6  0x0218
#define	T33_RADIX_REG_MCE_FPGA_DBG_MPT_7  0x021c
#define T33_RADIX_REG_MCE_FPGA_DBG_RAU_0  0x0220
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAU_1  0x0224
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAU_2  0x0228
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAU_3  0x022c
#define T33_RADIX_REG_MCE_FPGA_DBG_RAV_0  0x0230
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAV_1  0x0234
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAV_2  0x0238
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAV_3  0x023c
#define T33_RADIX_REG_MCE_FPGA_DBG_RAY_0  0x0240
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_1  0x0244
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_2  0x0248
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_3  0x024c
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_4  0x0250
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_5  0x0254
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_6  0x0258
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_7  0x025c
#define T33_RADIX_REG_MCE_FPGA_DBG_RAY_8  0x0260
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_9  0x0264
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_a  0x0268
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_b  0x026c
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_c  0x0270
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_d  0x0274
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_e  0x0278
#define	T33_RADIX_REG_MCE_FPGA_DBG_RAY_f  0x027c


#define T33_RADIX_REG_MCE_SECH_GRP0        0x01c4
#define T33_RADIX_REG_MCE_SECH_GRP1        0x01c8

#define T33_RADIX_REG_MCE_SECH_CFG0_L      0x01cc
#define T33_RADIX_REG_MCE_SECH_CFG0_H      0x01d0

#define T33_RADIX_REG_MCE_SECH_CFG1_L      0x01d4
#define T33_RADIX_REG_MCE_SECH_CFG1_H      0x01d8

#define T33_RADIX_REG_MCE_SECH_CFG2_L      0x01dc
#define T33_RADIX_REG_MCE_SECH_CFG2_H      0x01e0

#define T33_RADIX_REG_MCE_SECH_CFG3_L      0x01e4
#define T33_RADIX_REG_MCE_SECH_CFG3_H      0x01e8

/*************************************************************************************
                  IPRED Module
*************************************************************************************/
#define RADIX_REG_IPRED_CTRL                0x0
#define RADIX_REG_IPRED_BC_ST               0x4
#define RADIX_REG_IPRED_BC_INIT             0x8
#define RADIX_REG_IPRED_FRM_SIZE            0xc
#define RADIX_REG_IPRED_SLV_MODE            0x14
#define RADIX_REG_IPRED_MOD_CTRL            0x18
#define RADIX_REG_IPRED_MODE0               0x1c
#define RADIX_REG_IPRED_MODE1               0x20
#define RADIX_REG_IPRED_MODE2               0x24
#define RADIX_REG_IPRED_IPMD_MPM_BIASS32    (4*0x36)
#define RADIX_REG_IPRED_IPMD_MPM_BIASW32    (4*0x37)
#define RADIX_REG_IPRED_IPMD_MPM_BIASS16    (4*0x38)
#define RADIX_REG_IPRED_IPMD_MPM_BIASW16    (4*0x39)
#define RADIX_REG_IPRED_IPMD_MPM_BIASS8	    (4*0x40)
#define RADIX_REG_IPRED_IPMD_MPM_BIASW8	    (4*0x41)
#define RADIX_REG_IPRED_IPMD_ANGLE_BIAS	    (4*0x42)
#define RADIX_REG_IPRED_IPMD_DM_BIAS32	    (4*0x43)
#define RADIX_REG_IPRED_IPMD_DM_BIAS16	    (4*0x44)
#define RADIX_REG_IPRED_IPMD_DM_BIAS8	    (4*0x45)
#define RADIX_REG_IPRED_BITS0		    (4*0x46)
#define RADIX_REG_IPRED_BITS1		    (4*0x47)
#define RADIX_REG_IPRED_BITS2		    (4*0x48)
#define RADIX_REG_IPRED_BITS3		    (4*0x49)
#define RADIX_REG_IPRED_BITS4		    (4*0x4a)
#define RADIX_REG_IPRED_BITS5		    (4*0x4b)
#define RADIX_REG_IPRED_BITS6		    (4*0x4c)
#define RADIX_REG_IPRED_BITS7		    (4*0x4d)
#define RADIX_REG_IPRED_BITS8		    (4*0x4e)
#define RADIX_REG_IPRED_BITS9		    (4*0x4f)
#define T33_RADIX_REG_IPRED_RAS_SAD_DIF_THR     (4*0x50)
#define T33_RADIX_REG_IPRED_RAS_EDGE_CNT_THR    (4*0x51)
//- for debug
#define T33_DBG_IP_EFE_RAWY_CTRL            (4*0x52)
#define T33_DBG_IP_EFE_RAWC_CTRL            (4*0x53)
#define T33_DBG_IP_PD8_CTRL                 (4*0x54)
#define T33_DBG_IP_PD16_CTRL                (4*0x55)
#define T33_DBG_IP_PD32_CTRL                (4*0x56)
#define T33_DBG_IP_PDC_CTRL                 (4*0x57)
#define T33_DBG_IP_PTY_CTRL                 (4*0x58)
#define T33_DBG_IP_PTC_CTRL                 (4*0x59)
#define T33_DBG_IP_EFE_RAWY_0               (4*0x5a)
#define T33_DBG_IP_EFE_RAWY_1               (4*0x5b)
#define T33_DBG_IP_EFE_RAWY_2               (4*0x5c)
#define T33_DBG_IP_EFE_RAWY_3               (4*0x5d)
#define T33_DBG_IP_EFE_RAWU_0               (4*0x5e)
#define T33_DBG_IP_EFE_RAWU_1               (4*0x5f)
#define T33_DBG_IP_EFE_RAWU_2               (4*0x60)
#define T33_DBG_IP_EFE_RAWU_3               (4*0x61)
#define T33_DBG_IP_EFE_RAWV_0               (4*0x62)
#define T33_DBG_IP_EFE_RAWV_1               (4*0x63)
#define T33_DBG_IP_EFE_RAWV_2               (4*0x64)
#define T33_DBG_IP_EFE_RAWV_3               (4*0x65)
#define T33_DBG_IP_PD8_DA_0                 (4*0x66)
#define T33_DBG_IP_PD8_DA_1                 (4*0x67)
#define T33_DBG_IP_PD8_DA_2                 (4*0x68)
#define T33_DBG_IP_PD8_DA_3                 (4*0x69)
#define T33_DBG_IP_PD8_DA_4                 (4*0x6a)
#define T33_DBG_IP_PD8_DA_5                 (4*0x6b)
#define T33_DBG_IP_PD8_DA_6                 (4*0x6c)
#define T33_DBG_IP_PD8_DA_7                 (4*0x6d)
#define T33_DBG_IP_PD8_DA_8                 (4*0x6e)
#define T33_DBG_IP_PD8_DA_9                 (4*0x6f)
#define T33_DBG_IP_PD8_DA_10                (4*0x70)
#define T33_DBG_IP_PD8_DA_11                (4*0x71)
#define T33_DBG_IP_PD8_DA_12                (4*0x72)
#define T33_DBG_IP_PD8_DA_13                (4*0x73)
#define T33_DBG_IP_PD8_DA_14                (4*0x74)
#define T33_DBG_IP_PD8_DA_15                (4*0x75)
#define T33_DBG_IP_PD8_DA_16                (4*0x76)
#define T33_DBG_IP_PD8_DA_17                (4*0x77)
#define T33_DBG_IP_PD8_DA_18                (4*0x78)
#define T33_DBG_IP_PD16_DA_0                (4*0x79)
#define T33_DBG_IP_PD16_DA_1                (4*0x7a)
#define T33_DBG_IP_PD16_DA_2                (4*0x7b)
#define T33_DBG_IP_PD16_DA_3                (4*0x7c)
#define T33_DBG_IP_PD16_DA_4                (4*0x7d)
#define T33_DBG_IP_PD16_DA_5                (4*0x7e)
#define T33_DBG_IP_PD16_DA_6                (4*0x7f)
#define T33_DBG_IP_PD16_DA_7                (4*0x80)
#define T33_DBG_IP_PD16_DA_8                (4*0x81)
#define T33_DBG_IP_PD16_DA_9                (4*0x82)
#define T33_DBG_IP_PD16_DA_10               (4*0x83)
#define T33_DBG_IP_PD16_DA_11               (4*0x84)
#define T33_DBG_IP_PD16_DA_12               (4*0x85)
#define T33_DBG_IP_PD16_DA_13               (4*0x86)
#define T33_DBG_IP_PD16_DA_14               (4*0x87)
#define T33_DBG_IP_PD16_DA_15               (4*0x88)
#define T33_DBG_IP_PD16_DA_16               (4*0x89)
#define T33_DBG_IP_PD16_DA_17               (4*0x8a)
#define T33_DBG_IP_PD16_DA_18               (4*0x8b)
#define T33_DBG_IP_PD32_DA_0                (4*0x8c)
#define T33_DBG_IP_PD32_DA_1                (4*0x8d)
#define T33_DBG_IP_PD32_DA_2                (4*0x8e)
#define T33_DBG_IP_PD32_DA_3                (4*0x8f)
#define T33_DBG_IP_PD32_DA_4                (4*0x90)
#define T33_DBG_IP_PD32_DA_5                (4*0x91)
#define T33_DBG_IP_PD32_DA_6                (4*0x92)
#define T33_DBG_IP_PD32_DA_7                (4*0x93)
#define T33_DBG_IP_PD32_DA_8                (4*0x94)
#define T33_DBG_IP_PD32_DA_9                (4*0x95)
#define T33_DBG_IP_PD32_DA_10               (4*0x96)
#define T33_DBG_IP_PD32_DA_11               (4*0x97)
#define T33_DBG_IP_PD32_DA_12               (4*0x98)
#define T33_DBG_IP_PD32_DA_13               (4*0x99)
#define T33_DBG_IP_PD32_DA_14               (4*0x9a)
#define T33_DBG_IP_PD32_DA_15               (4*0x9b)
#define T33_DBG_IP_PD32_DA_16               (4*0x9c)
#define T33_DBG_IP_PD32_DA_17               (4*0x9d)
#define T33_DBG_IP_PD32_DA_18               (4*0x9e)
#define T33_DBG_IP_PDU_DA_0                 (4*0x9f)
#define T33_DBG_IP_PDU_DA_1                 (4*0xa0)
#define T33_DBG_IP_PDU_DA_2                 (4*0xa1)
#define T33_DBG_IP_PDU_DA_3                 (4*0xa2)
#define T33_DBG_IP_PDU_DA_4                 (4*0xa3)
#define T33_DBG_IP_PDU_DA_5                 (4*0xa4)
#define T33_DBG_IP_PDU_DA_6                 (4*0xa5)
#define T33_DBG_IP_PDU_DA_7                 (4*0xa6)
#define T33_DBG_IP_PDU_DA_8                 (4*0xa7)
#define T33_DBG_IP_PDU_DA_9                 (4*0xa8)
#define T33_DBG_IP_PDU_DA_10                (4*0xa9)
#define T33_DBG_IP_PDV_DA_0                 (4*0xaa)
#define T33_DBG_IP_PDV_DA_1                 (4*0xab)
#define T33_DBG_IP_PDV_DA_2                 (4*0xac)
#define T33_DBG_IP_PDV_DA_3                 (4*0xad)
#define T33_DBG_IP_PDV_DA_4                 (4*0xae)
#define T33_DBG_IP_PDV_DA_5                 (4*0xaf)
#define T33_DBG_IP_PDV_DA_6                 (4*0xb0)
#define T33_DBG_IP_PDV_DA_7                 (4*0xb1)
#define T33_DBG_IP_PDV_DA_8                 (4*0xb2)
#define T33_DBG_IP_PDV_DA_9                 (4*0xb3)
#define T33_DBG_IP_PDV_DA_10                (4*0xb4)
#define T33_DBG_IP_PTY_DA_0                 (4*0xb5)
#define T33_DBG_IP_PTY_DA_1                 (4*0xb6)
#define T33_DBG_IP_PTY_DA_2                 (4*0xb7)
#define T33_DBG_IP_PTU_DA                   (4*0xb8)
#define T33_DBG_IP_PTV_DA                   (4*0xb9)
/*************************************************************************************
                  TFM Module
*************************************************************************************/
#define RADIX_TFM_REG_SCTRL		(4*0x00) //0
#define RADIX_TFM_REG_SINIT		(4*0x01) //1
#define RADIX_TFM_REG_STATUS0	        (4*2)
#define RADIX_TFM_REG_STATUS1	        (4*3)
#define RADIX_TFM_REG_STATUS2	        (4*4)
#define RADIX_TFM_REG_STATUS3	        (4*5)
#define RADIX_TFM_REG_STATUS4	        (4*6)
#define RADIX_TFM_REG_STATUS5	        (4*7)
#define RADIX_TFM_REG_STATUS6	        (4*8)
#define RADIX_TFM_REG_STATUS7	        (4*9)
#define RADIX_TFM_REG_STATUS8	        (4*10)
#define RADIX_TFM_REG_STATUS9	        (4*11)
#define RADIX_TFM_REG_STATUS10	        (4*12)
#define RADIX_TFM_RAM_ADDR		(4*24)
#define RADIX_TFM_RAM_EN		(4*25)
#define T33_RADIX_TFM_DEADZONE_CFG0		(4*26)
#define T33_RADIX_TFM_DEADZONE_CFG1		(4*27)
#define T32_RADIX_TFM_MD0_CRC		(4*26)
#define T32_RADIX_TFM_MD1_CRC		(4*27)
#define RADIX_TFM_MD2_CRC		(4*28)
#define RADIX_TFM_MD3_CRC		(4*29)
#define RADIX_TFM_MD4_CRC		(4*30)
#define RADIX_TFM_FRM_SIZE		(4*31)
#define RADIX_TFM_FRM_CTL		(4*32)
#define T33_RADIX_TFM_EFE_RA_CFG		(4*112)
#define T33_RADIX_TFM_MD_CFRN_CFG		(4*113)
#define T33_RADIX_TFM_MD_SSE_CFG		(4*114)
#define T33_RADIX_TFM_MCE_PD_CFG		(4*115)
#define T33_RADIX_TFM_EFE_RA_DATA		(4*128)
#define T33_RADIX_TFM_MD_CFRN_DATA		(4*136)
#define T33_RADIX_TFM_MD_SSE_DATA		(4*144)
#define T33_RADIX_TFM_MCE_PD_DATA		(4*152)
/*************************************************************************************
                  DBLK Module
*************************************************************************************/
#define RADIX_REG_DBLK_GLB_TRIG         0x0   //software reset
#define RADIX_REG_DBLK_GLB_CTRL         0x4   //global control
#define RADIX_REG_DBLK_MB_SIZE          0x8   //frame size
#define RADIX_REG_DBLK_CHN_BA           0xc   //chain base address, not used now
#define RADIX_REG_DBLK_FLT_PARA         0x10  //filter parameters
#define RADIX_REG_DBLK_TILE_BA          0x14  //backup tile right base address
#define RADIX_REG_DBLK_FRM_SIZE         0x18   //frame size
#define SET_NEW_UDBLK

#define REG_DBLK_TRIG                   0x0000
#define REG_DBLK_CFG                    0x0004
#define REG_DBLK_DHA                    0x0008
#define DBLK_CFG_ALPHA(a)               (((a) & 0x1F)<<0)
#define DBLK_CFG_BETA(a)                (((a) & 0x1F)<<8)
#define DBLK_CFG_NO_LFT(a)              (((a) & 0x1)<<17)

#define DBLK_RUN                        0x1
#define DBLK_STOP                       0x2
#define DBLK_RESET                      0x4
#define DBLK_SLICE_RUN                  0x8

#define REG_DBLK_CTRL                   0x0064
#define DBLK_CTRL(expand, rotate, loop_filter)           \
( ((expand) & 0x1)<<4 |                                  \
  ((rotate) & 0x3)<<1 |                                  \
  ((loop_filter) & 0x1)<<0                               \
)

#define REG_DBLK_VTR                    0x0068
#define DBLK_FMT_H264                   0x1
#define DBLK_FMT_RV9                    0x2
#define DBLK_FMT_VC1                    0x3
#define DBLK_FMT_MPEG2                  0x4
#define DBLK_FMT_MPEG4                  0x5
#define DBLK_FMT_VP8                    0x6
#define DBLK_FRM_I                      0x0
#define DBLK_FRM_P                      0x1
#define DBLK_FRM_B                      0x2
#define DBLK_VTR(beta, alpha, vp8_spl, vp8_kf,           \
                 frm_typ, video_fmt)                     \
( ((beta) & 0xFF)<<24 |                                  \
  ((alpha) & 0xFF)<<16 |                                 \
  ((vp8_spl) & 0x1)<<9 |                                 \
  ((vp8_kf) & 0x1)<<5 |                                  \
  ((frm_typ) & 0x3)<<3 |                                 \
  ((video_fmt) & 0x7)<<0                                 \
)

#define REG_DBLK_FSTA                   0x006C
#define REG_DBLK_GSTA                   0x0070
#define REG_DBLK_GSIZE                  0x0074
#define DBLK_GSIZE(mb_height, mb_width)                  \
( ((mb_height) & 0xFFFF)<<16 |                           \
  ((mb_width) & 0xFFFF)<<0                               \
)

#define REG_DBLK_GENDA                  0x0078
#define REG_DBLK_GPOS                   0x007C
#define DBLK_GPOS(first_mby, first_mbx)                  \
( ((first_mby) & 0xFFFF)<<16 |                           \
  ((first_mbx) & 0xFFFF)<<0                              \
)

#define REG_DBLK_GPIC_STR               0x0080
#define DBLK_GPIC_STR(dst_strd_c, dst_strd_y)            \
( ((dst_strd_c) & 0xFFFF)<<16 |                          \
  ((dst_strd_y) & 0xFFFF)<<0                             \
)

#define REG_DBLK_GPIC_YA                0x0084
#define REG_DBLK_GPIC_CA                0x0088
#define REG_DBLK_GP_ENDA                0x008C
#define REG_DBLK_SLICE_ENDA             0x0090
#define REG_DBLK_BLK_CTRL               0x0094
#define REG_DBLK_BLK_FIFO               0x0098

#define REG_DBLK_MOS_CFG                0x0240
#define REG_DBLK_MOS_ADDR               0x0244
#define DBLK_MOS_BASE_ADDR              VPU_BASE + 0xC7000
#define REG_DBLK_MOS_CRC                0x0248
/*************************************************************************************
                  SDE Module
*************************************************************************************/
#define RADIX_REG_SDE_CFG0          0x0
#define RADIX_REG_SDE_CFG1          0x4
#define RADIX_REG_SDE_BS_ADDR       0x8
#define RADIX_REG_SDE_BS_LENG       0xc
#define RADIX_REG_SDE_INIT          0x10
#define RADIX_REG_SDE_BS_LENGTH     0x24
#define RADIX_REG_SDE_BS_CRC        0x34
#define RADIX_REG_SDE_MAX_BS_SIZE   0x64
#define RADIX_REG_SDE_MAX_BS_EXCEED 0x68
#define RADIX_REG_SDE_NAL3_LEN      0x7C //T33
#define RADIX_REG_SDE_BS_CF_LEN     0x80 //T33
#define RADIX_REG_SDE_STATE_BASE    0x1000//sde state start addr

//fpga dbg
#define T33_RADIX_REG_SDE_FPGA_DBG_CFG0       0xa0
#define T33_RADIX_REG_SDE_FPGA_DBG_CFG1       0xa4
#define T33_RADIX_REG_SDE_FPGA_DBG_CFG2       0xa8 //not used
#define T33_RADIX_REG_SDE_FPGA_DBG_CFG3       0xac //not uesed

#define T33_RADIX_REG_SDE_FPGA_DBG_MD_CF0     0xb0
#define T33_RADIX_REG_SDE_FPGA_DBG_MD_CF1     0xb4
#define T33_RADIX_REG_SDE_FPGA_DBG_MD_CF2     0xb8
#define T33_RADIX_REG_SDE_FPGA_DBG_MD_CF3     0xbc
#define T33_RADIX_REG_SDE_FPGA_DBG_MD_CF4     0xc0
#define T33_RADIX_REG_SDE_FPGA_DBG_MD_CF5     0xc4
#define T33_RADIX_REG_SDE_FPGA_DBG_MD_CF6     0xc8
#define T33_RADIX_REG_SDE_FPGA_DBG_MD_CF7     0xcc

#define T33_RADIX_REG_SDE_FPGA_DBG_EMC_SE0    0xd0
#define T33_RADIX_REG_SDE_FPGA_DBG_EMC_SE1    0xd4
#define T33_RADIX_REG_SDE_FPGA_DBG_EMC_SE2    0xd8
#define T33_RADIX_REG_SDE_FPGA_DBG_EMC_SE3    0xdc

#define REG_SDE_STAT                0x0000
#define REG_SDE_SL_CTRL             0x0004
#define SDE_SLICE_INIT              (0x1<<1)
#define SDE_MB_RUN                  (0x1<<0)

#define REG_SDE_SL_GEOM             0x0008
#define SDE_SL_GEOM(mb_height, mb_width,                 \
                    first_mby, first_mbx)                \
( ((mb_height) & 0xFF)<<24 |                             \
  ((mb_width) & 0xFF)<<16 |                              \
  ((first_mby) & 0xFF)<<8 |                              \
  ((first_mbx) & 0xFF)<<0                                \
)

#define REG_SDE_GL_CTRL             0x000C
#define SDE_BP(mby, mbx)                                 \
( ((mby) & 0xFF)<<24 |                                   \
  ((mbx) & 0xFF)<<16                                     \
)
#define SDE_MODE_AUTO               (0x0<<4)
#define SDE_MODE_STEP               (0x1<<4)
#define SDE_MODE_DEBUG              (0x2<<4)

#define REG_SDE_CODEC_ID            0x0010
#define SDE_FMT_H264_ENC            (0x1<<1)

#define REG_SDE_CFG0                0x0014
#define REG_SDE_CFG1                0x0018
#define REG_SDE_CFG2                0x001C
#define REG_SDE_CFG3                0x0020
#define REG_SDE_CFG4                0x0024
#define REG_SDE_CFG5                0x0028
#define REG_SDE_CFG6                0x002C
#define REG_SDE_CFG7                0x0030
#define REG_SDE_CFG8                0x0034
#define REG_SDE_CFG9                0x0038
#define REG_SDE_CFG10               0x003C
#define REG_SDE_CFG11               0x0040
#define REG_SDE_CFG12               0x0044
#define REG_SDE_CFG13               0x0048
#define REG_SDE_CFG14               0x004C
#define REG_SDE_CFG15               0x0050
#define REG_SDE_NAL3_LEN            0x005C//T33
#define REG_SDE_STATE_BASE          0x1000

#define REG_SDE_CTX_TBL             0x2000
#define REG_SDE_CQP_TBL             0x3800
/*************************************************************************************
                  BC Module
*************************************************************************************/
#define RADIX_REG_BC_CFG0           0x0
#define RADIX_REG_BC_CFG1           0x4
#define RADIX_REG_BC_CFG2           0x8
#define T33_RADIX_FPGA_CFG_ADDR_CF      0x10
#define T33_RADIX_FPGA_CFG_ADDR_SE      0x14
#define T33_RADIX_FPGA_DATA_ADDR_CF     0x18
#define T33_RADIX_FPGA_DATA_ADDR_SE1    0x1c
#define T33_RADIX_FPGA_DATA_ADDR_SE2    0x20
#define T33_RADIX_FPGA_DATA_ADDR_SE3    0x24
#define RADIX_REG_BC_STATE_BASE     0x1000//bc state start addr
/*************************************************************************************
                  STC Module
*************************************************************************************/
#define RADIX_REG_STC_CFG0          0x0
#define RADIX_REG_STC_CFG1          0x4
#define RADIX_REG_STC_SAO_MG        0x1000
#define RADIX_REG_STC_SAO_TP        0x1004
/*************************************************************************************
                  SAO Module
*************************************************************************************/
#define RADIX_REG_SAO_GLB_INFO      0x20
#define RADIX_REG_SAO_PIC_SIZE      0x28
#define RADIX_REG_SAO_SLICE_XY      0x2c
#define RADIX_REG_SAO_SLICE_INIT    0x34
/*************************************************************************************
                  DT Module
*************************************************************************************/
#define RADIX_REG_DT_INIT           0x0
#define T33_RADIX_REG_DT_CFG            0x4
#define T32_RADIX_REG_DT_CFG0           0x4
#define T32_RADIX_REG_DT_CFG1           0x8
/*************************************************************************************
  VMAU (VPU Matrix Arithmetic Unit)
*************************************************************************************/
//radix tfm
#define TFM_IS_ISLICE(a)           (((a) & 0x1)<<15)//T33
#define T33_TFM_FMT_H264               (0x1<<1)
#define T33_TFM_SLICE_INIT             (0x1<<0)
#define ACMASK_TYPE(a)             (((a) & 0x1)<<6)//T33
#define TFM_IS_ACMASK(a)           (((a) & 0x1)<<5)//T33
#define TFM_TU4_MASK(a)            (((a) & 0x7)<<10)//T33
#define TFM_TU8_MASK(a)            (((a) & 0x3)<<8)//T33
#define TFM_TU8_SCA_LIST_EN(a)     (((a) & 0x1)<<1)//T33
#define TFM8_INTRA_EN(a)           (((a) & 0x1)<<5)//T33
#define TFM8_INTER_EN(a)           (((a) & 0x1)<<3) //T33

#define TFM_DEADZONE_4IC(a)        (((a) & 0x3F)<<0)//T33
#define TFM_DEADZONE_4IY(a)        (((a) & 0x3F)<<6)//T33
#define TFM_DEADZONE_4PC(a)        (((a) & 0x3F)<<12)//T33
#define TFM_DEADZONE_4PY(a)        (((a) & 0x3F)<<18)//T33

#define TFM_DEADZONE_PCDC(a)       (((a) & 0x3F)<<0)//T33
#define TFM_DEADZONE_ICDC(a)       (((a) & 0x3F)<<6)//T33
#define TFM_DEADZONE_IYDC(a)       (((a) & 0x3F)<<12)//T33
#define TFM_DEADZONE_8IY(a)        (((a) & 0x3F)<<18)//T33
#define TFM_DEADZONE_8PY(a)        (((a) & 0x3F)<<24)//T33

// vmau tfm
#define REG_VMAU_DEADZONE           0x0078
#define VMAU_DEADZONE0_IY(a)        (((a) & 0xFF)<<0)
#define VMAU_DEADZONE1_PY(a)        (((a) & 0xFF)<<8)
#define VMAU_DEADZONE2_IC(a)        (((a) & 0xFF)<<16)
#define VMAU_DEADZONE3_PC(a)        (((a) & 0xFF)<<24)

#define REG_VMAU_DEADZONE1          0x019C

#define REG_VMAU_ACMASK             0x007C

// vmau global
#define T32_REG_VMAU_GBL_RUN            0x0040
#define T33_REG_VMAU_GBL_RUN            0x0400
#define VMAU_RUN                    0x1

#define T33_REG_VMAU_GBL_CTR            0x0404
#define T32_REG_VMAU_GBL_CTR            0x0044
#define VMAU_TFM8_SCA_CUSTOM(a)     (((a) & 0x1)<<27)
#define VMAU_NED_CTRL(a)            (((a) & 0x1)<<28)
#define VMAU_FMT_H264               (0x1<<29)

#define T33_REG_VMAU_VIDEO_TYPE         0x0408
#define T32_REG_VMAU_VIDEO_TYPE         0x0050
#define VMAU_IS_ISLICE(a)           (((a) & 0x1)<<12)
#define VMAU_MODE_ENC               (0x1<<11)
#define VMAU_YUV_GRAY               (0x0<<13)
#define VMAU_IP_MODE_MASK           ((0x0 & 0x1FFFF)<<14)

#define T33_REG_VMAU_Y_GS               0x040c
#define T32_REG_VMAU_Y_GS               0x0054
#define VMAU_FRM_WID(a)             (((a) & 0x7FFF)<<0)
#define VMAU_FRM_HEI(a)             (((a) & 0x7FFF)<<16)

// vmau tfm
#define REG_VMAU_DEADZONE           0x0078
#define VMAU_DEADZONE0_IY(a)        (((a) & 0xFF)<<0)
#define VMAU_DEADZONE1_PY(a)        (((a) & 0xFF)<<8)
#define VMAU_DEADZONE2_IC(a)        (((a) & 0xFF)<<16)
#define VMAU_DEADZONE3_PC(a)        (((a) & 0xFF)<<24)

#define REG_VMAU_DEADZONE1          0x019C

#define REG_VMAU_ACMASK             0x007C

// vmau md
#define T33_REG_VMAU_MD_CFG0            0x0410
#define T32_REG_VMAU_MD_CFG0            0x0030
#define VMAU_MD_SLICE_I(a)          (((a) & 0x1)<<0)
#define VMAU_MD_SLICE_P(a)          (((a) & 0x1)<<1)
#define VMAU_MD_IS_DECODE(a)        (((a) & 0x1)<<4)
#define VMAU_MD_I4_DIS(a)           (((a) & 0x1)<<8)
#define VMAU_MD_I16_DIS(a)          (((a) & 0x1)<<9)
#define VMAU_MD_PSKIP_DIS(a)        (((a) & 0x1)<<10)
#define VMAU_MD_P_L0_DIS(a)         (((a) & 0x1)<<11)
#define VMAU_MD_I8_DIS(a)           (((a) & 0x1)<<12)
#define VMAU_MD_PT8_DIS(a)          (((a) & 0x1)<<13)
#define VMAU_MD_DREF_EN(a)          (((a) & 0x1)<<14)
#define VMAU_MD_DCT8_EN(a)          (((a) & 0x1)<<15)
#define VMAU_MD_FRM_REDGE(a)        (((a) & 0xFF)<<16)
#define VMAU_MD_FRM_BEDGE(a)        (((a) & 0xFF)<<24)

#define T33_REG_VMAU_MD_CFG1            0x0414
#define T32_REG_VMAU_MD_CFG1            0x0034
#define VMAU_IPMY_BIAS_EN(a)        (((a) & 0x1)<<0)
#define VMAU_IPMC_BIAS_EN(a)        (((a) & 0x1)<<1)
#define VMAU_COST_BIAS_EN(a)        (((a) & 0x1)<<2)
#define VMAU_CSSE_BIAS_EN(a)        (((a) & 0x1)<<3)
#define VMAU_JMLAMBDA2_EN(a)        (((a) & 0x1)<<4)
#define VMAU_INTER_NEI_EN(a)        (((a) & 0x1)<<5)
#define VMAU_SKIP_BIAS_EN(a)        (((a) & 0x3)<<6)
#define VMAU_LMD_BIAS_EN(a)         (((a) & 0x1)<<8)
#define VMAU_INFO_EN(a)             (((a) & 0x1)<<9)
#define VMAU_DCM_EN(a)              (((a) & 0x1)<<10)
#define VMAU_MVDS_ALL(a)            (((a) & 0x1)<<12)
#define VMAU_MVDS_ABS(a)            (((a) & 0x1)<<13)
#define VMAU_MVS_ALL(a)             (((a) & 0x1)<<14)
#define VMAU_MVS_ABS(a)             (((a) & 0x1)<<15)
#define VMAU_P_L0_BIAS(a)           (((a) & 0xF)<<16)
#define VMAU_PSKIP_BIAS(a)          (((a) & 0xF)<<20)
#define VMAU_I4_BIAS(a)             (((a) & 0xF)<<24)
#define VMAU_I16_BIAS(a)            (((a) & 0xF)<<28)

#define T33_REG_VMAU_MD_CFG2            0x0418
#define T32_REG_VMAU_MD_CFG2            0x0038
#define VMAU_IPM_BIAS_0(a)          (((a) & 0xF)<<0)
#define VMAU_IPM_BIAS_1(a)          (((a) & 0xF)<<4)
#define VMAU_IPM_BIAS_2(a)          (((a) & 0xF)<<8)
#define VMAU_IPM_BIAS_QP0(a)        (((a) & 0x3F)<<12)
#define VMAU_IPM_BIAS_QP1(a)        (((a) & 0x3F)<<18)
#define VMAU_MD_FBC_EP(a)           (((a) & 0xFF)<<24)

#define T33_REG_VMAU_MD_CFG3            0x041C
#define T32_REG_VMAU_MD_CFG3            0x003C
#define VMAU_CSSE_BIAS_0(a)         (((a) & 0xF)<<0)
#define VMAU_CSSE_BIAS_1(a)         (((a) & 0xF)<<4)
#define VMAU_CSSE_BIAS_2(a)         (((a) & 0xF)<<8)
#define VMAU_CSSE_BIAS_QP0(a)       (((a) & 0x3F)<<12)
#define VMAU_CSSE_BIAS_QP1(a)       (((a) & 0x3F)<<18)
#define VMAU_LMD_BIAS(a)            (((a) & 0xF)<<24)
#define VMAU_PL0_FS_DIS(a)          (((a) & 0x1)<<28)
#define VMAU_PT8_FS_DIS(a)          (((a) & 0x1)<<29)
#define VMAU_PL0_COST_MAX(a)        (((a) & 0x1)<<30)
#define VMAU_PT8_COST_MAX(a)        (((a) & 0x1)<<31)

#define T33_REG_VMAU_MD_CFG4            0x0420
#define T32_REG_VMAU_MD_CFG4            0x00D8
#define VMAU_YSSE_THR(a)            (((a) & 0xFFFFFF)<<0)
#define VMAU_I8_BIAS(a)             (((a) & 0xF)<<24)
#define VMAU_PT8_BIAS(a)            (((a) & 0xF)<<28)

#define T33_REG_VMAU_MD_CFG5            0x0424
#define T32_REG_VMAU_MD_CFG5            0x00DC
#define VMAU_CSSE_THR(a)            (((a) & 0xFFFFFF)<<0)
#define VMAU_CQP_OFFSET(a)          (((a) & 0x1F)<<24)
#define VMAU_I4_COST_MAX(a)         (((a) & 0x1)<<29)
#define VMAU_I8_COST_MAX(a)         (((a) & 0x1)<<30)
#define VMAU_I16_COST_MAX(a)        (((a) & 0x1)<<31)

#define T33_REG_VMAU_MD_CFG6            0x0428
#define T32_REG_VMAU_MD_CFG6            0x00FC
#define VMAU_DCM_PARAM(a)           (((a) & 0xFFFFFF)<<0)
#define VMAU_SDE_PRIOR(a)           (((a) & 0xF)<<24)
#define VMAU_DB_PRIOR(a)            (((a) & 0xF)<<28)

#define T33_REG_VMAU_MD_CFG7            0x042c
#define T32_REG_VMAU_MD_CFG7            0x0114
#define VMAU_CFG_SIZE_X(a)          (((a) & 0xF)<<0)
#define VMAU_CFG_SIZE_Y(a)          (((a) & 0xF)<<4)
#define VMAU_CFG_IW_THR(a)          (((a) & 0x1FF)<<8)
#define VMAU_CFG_BASEQP(a)          (((a) & 0x3F)<<17)
#define VMAU_CFG_ALPHA(a)           (((a) & 0xFF)<<23)
#define VMAU_PS_COST_MAX(a)         (((a) & 0x1)<<31)

#define T33_REG_VMAU_MD_CFG8            0x0430
#define T32_REG_VMAU_MD_CFG8            0x0118
#define VMAU_CFG_MVR_THR1(a)        (((a) & 0x7FFF)<<0)
#define VMAU_CFG_MVR_THR2(a)        (((a) & 0x1FFFF)<<15)

#define T33_REG_VMAU_MD_CFG9            0x0434
#define T32_REG_VMAU_MD_CFG9            0x011C
#define VMAU_CFG_MVR_THR3(a)        (((a) & 0xFFFFF)<<0)
#define VMAU_CFG_BETA(a)            (((a) & 0xFF)<<20)

#define T33_REG_VMAU_MD_CFG10           0x0438
#define T32_REG_VMAU_MD_CFG10           0x0190
#define T33_REG_VMAU_MD_CFG11           0x043c
#define T32_REG_VMAU_MD_CFG11           0x0194
#define VMAU_CFG_MD_RBIAS(a)        (((a) & 0xf) << 21)
#define VMAU_CFG_MD_RBIAS_EN(a)     (((a) & 1) << 25)
#define VMAU_CFG_MD_PCDC_N0(a)      (((a) & 7) << 26)
#define VMAU_CFG_MD_IFA_VLD(a)      (((a) & 1) << 29)
#define VMAU_CFG_MD_SLV_VLD(a)      (((a) & 1) << 30)
#define VMAU_CFG_MD_SET_VLD(a)      (((a) & 1) << 31)

#define T33_REG_VMAU_MD_MAXBS0          0x0440
#define T33_REG_VMAU_MD_MAXBS1          0x0444
#define T33_REG_VMAU_CTX                0x0448
#define T33_REG_VMAU_MD_MODE            0x044c
#define T32_REG_VMAU_MD_MAXBS0          0x01E0
#define T32_REG_VMAU_MD_MAXBS1          0x01E4
#define T32_REG_VMAU_MD_MODE            0x0198
#define T32_REG_VMAU_CTX                0x002C

// vmau ipred
#define REG_VMAU_IPRED_CFG0         0x01C0
#define VMAU_IP_MD_VAL(a)           (((a) & 0x1)<<30)
#define VMAU_IP_REF_NEB_4(a)        (((a) & 0x1)<<29)
#define VMAU_IP_REF_NEB_8(a)        (((a) & 0x1)<<28)
#define VMAU_IP_REF_PRD_C(a)        (((a) & 0xF)<<24)
#define VMAU_IP_REF_PRD_4(a)        (((a) & 0xF)<<20)
#define VMAU_IP_REF_PRD_8(a)        (((a) & 0xF)<<16)
#define VMAU_IP_REF_PRD_16(a)       (((a) & 0xF)<<12)
#define VMAU_IP_REF_CUV_EN(a)       (((a) & 0x1)<<11)
#define VMAU_IP_REF_C4_EN(a)        (((a) & 0x1)<<10)
#define VMAU_IP_REF_C8_EN(a)        (((a) & 0x1)<<9)
#define VMAU_IP_REF_C16_EN(a)       (((a) & 0x1)<<8)
#define VMAU_IP_REF_LMDUV_EN(a)     (((a) & 0x1)<<7)
#define VMAU_IP_REF_LMD4_EN(a)      (((a) & 0x1)<<6)
#define VMAU_IP_REF_LMD8_EN(a)      (((a) & 0x1)<<5)
#define VMAU_IP_REF_LMD16_EN(a)     (((a) & 0x1)<<4)
#define VMAU_IP_REF_BITUV_EN(a)     (((a) & 0x1)<<3)
#define VMAU_IP_REF_BIT4_EN(a)      (((a) & 0x1)<<2)
#define VMAU_IP_REF_BIT8_EN(a)      (((a) & 0x1)<<1)
#define VMAU_IP_REF_BIT16_EN(a)     (((a) & 0x1)<<0)

#define REG_VMAU_IPRED_CFG1         0x01C4
#define VMAU_IP_REF_4_BIT0(a)       (((a) & 0xF)<<0)
#define VMAU_IP_REF_4_BIT1(a)       (((a) & 0xF)<<4)
#define VMAU_IP_REF_4_BIT2(a)       (((a) & 0xF)<<8)
#define VMAU_IP_REF_4_BIT3(a)       (((a) & 0xF)<<12)
#define VMAU_IP_REF_8_BIT0(a)       (((a) & 0xF)<<16)
#define VMAU_IP_REF_8_BIT1(a)       (((a) & 0xF)<<20)
#define VMAU_IP_REF_8_BIT2(a)       (((a) & 0xF)<<24)
#define VMAU_IP_REF_8_BIT3(a)       (((a) & 0xF)<<28)

#define REG_VMAU_IPRED_CFG2         0x01C8
#define VMAU_IP_REF_16_BIT0(a)      (((a) & 0xF)<<0)
#define VMAU_IP_REF_16_BIT1(a)      (((a) & 0xF)<<4)
#define VMAU_IP_REF_16_BIT2(a)      (((a) & 0xF)<<8)
#define VMAU_IP_REF_16_BIT3(a)      (((a) & 0xF)<<12)
#define VMAU_IP_REF_C_BIT0(a)       (((a) & 0xF)<<16)
#define VMAU_IP_REF_C_BIT1(a)       (((a) & 0xF)<<20)
#define VMAU_IP_REF_C_BIT2(a)       (((a) & 0xF)<<24)
#define VMAU_IP_REF_C_BIT3(a)       (((a) & 0xF)<<28)

#define REG_VMAU_IPRED_CFG3         0x01CC
#define VMAU_IP_REF_LMD16_IFO(a)    (((a) & 0xF)<<0)
#define VMAU_IP_REF_LMD8_IFO(a)     (((a) & 0xF)<<4)
#define VMAU_IP_REF_LMD4_IFO(a)     (((a) & 0xF)<<8)
#define VMAU_IP_REF_LMDUV_IFO(a)    (((a) & 0xF)<<12)
#define VMAU_IP_REF_NEB_8REF(a)     (((a) & 0xF)<<16)
#define VMAU_IP_REF_NEB_4REF(a)     (((a) & 0xF)<<20)

#define REG_VMAU_IPRED_CFG4         0x01D0
#define VMAU_IP_REF_C4_IFO(a)       (((a) & 0xFFFF)<<0)
#define VMAU_IP_REF_C8_IFO(a)       (((a) & 0xFFFF)<<16)

#define REG_VMAU_IPRED_CFG5         0x01D4
#define VMAU_IP_REF_C16_IFO(a)      (((a) & 0xFFFF)<<0)
#define VMAU_IP_REF_CUV_IFO(a)      (((a) & 0xFFFF)<<16)

/*************************************************************************************
  Motion interpolation programable table
*************************************************************************************/
#define HEVC_CFIR_Y_COEF0 {  1, 0,   0,  0,  0,   0, 0,  0 }
#define HEVC_CFIR_Y_COEF1 { -1, 4, -10, 58, 17,  -5, 1,  0 }
#define HEVC_CFIR_Y_COEF2 { -1, 4, -11, 40, 40, -11, 4, -1 }
#define HEVC_CFIR_Y_COEF3 {  0, 1,  -5, 17, 58, -10, 4, -1 }
#define HEVC_CFIR_Y_RND0 32
#define HEVC_CFIR_Y_SFT0 6
//#define HEVC_CFIR_Y_RND1 0
#define HEVC_CFIR_Y_SFT1 12
#define HEVC_CFIR_C_COEF0 {  1,  0,  0,  0, 0, 0, 0, 0 }
#define HEVC_CFIR_C_COEF1 { -2, 58, 10, -2, 0, 0, 0, 0 }
#define HEVC_CFIR_C_COEF2 { -4, 54, 16, -2, 0, 0, 0, 0 }
#define HEVC_CFIR_C_COEF3 { -6, 46, 28, -4, 0, 0, 0, 0 }
#define HEVC_CFIR_C_COEF4 { -4, 36, 36, -4, 0, 0, 0, 0 }
#define HEVC_CFIR_C_COEF5 { -4, 28, 46, -6, 0, 0, 0, 0 }
#define HEVC_CFIR_C_COEF6 { -2, 16, 54, -4, 0, 0, 0, 0 }
#define HEVC_CFIR_C_COEF7 { -2, 10, 58, -2, 0, 0, 0, 0 }
#define HEVC_CFIR_C_RND0 32
#define HEVC_CFIR_C_SFT0 6
//#define HEVC_CFIR_C_RND1 0
#define HEVC_CFIR_C_SFT1 12

#define SVAC_CFIR_Y_COEF0 {  1, 0,   0,   0,   0,   0, 0,  0 }
#define SVAC_CFIR_Y_COEF1 { -1, 4, -15, 111,  38, -12, 4, -1 }
#define SVAC_CFIR_Y_COEF2 { -1, 6, -19,  78,  78, -19, 6, -1 }
#define SVAC_CFIR_Y_COEF3 { -1, 4, -12,  38, 111, -15, 4, -1 }
#define SVAC_CFIR_Y_RND0 64
#define SVAC_CFIR_Y_SFT0 7
//#define SVAC_CFIR_Y_RND1 0
#define SVAC_CFIR_Y_SFT1 14
#define SVAC_CFIR_C_COEF0 {  1,  0,   0,   0,   0,   0,  0,  0 }
#define SVAC_CFIR_C_COEF1 { -1,  3,  -9, 121,  19,  -7,  2,  0 }
#define SVAC_CFIR_C_COEF2 { -1,  4, -15, 111,  38, -12,  4, -1 }
#define SVAC_CFIR_C_COEF3 { -1,  5, -18,  96,  59, -17,  5, -1 }
#define SVAC_CFIR_C_COEF4 { -1,  6, -19,  78,  78, -19,  6, -1 }
#define SVAC_CFIR_C_COEF5 { -1,  5, -17,  59,  96, -18,  5, -1 }
#define SVAC_CFIR_C_COEF6 { -1,  4, -12,  38, 111, -15,  4, -1 }
#define SVAC_CFIR_C_COEF7 {  0,  2,  -7,  19, 121,  -9,  3, -1 }
#define SVAC_CFIR_C_RND0 64
#define SVAC_CFIR_C_SFT0 7
//#define SVAC_CFIR_C_RND1 0
#define SVAC_CFIR_C_SFT1 14

#define RADIX_IS_SKIRT   0
#define RADIX_IS_MIRROR  1

#define RADIX_IS_BIAVG   0
#define RADIX_IS_WT1     1
#define RADIX_IS_WT2     2
#define RADIX_IS_FIXWT   3

#define RADIX_IS_ILUT0   0
#define RADIX_IS_ILUT1   2
#define RADIX_IS_EC      1

#define RADIX_IS_TCS     1
#define RADIX_NOT_TCS    0
#define RADIX_IS_SCS     1
#define RADIX_NOT_SCS    0
#define RADIX_IS_HLDGL   1
#define RADIX_NOT_HLDGL  0
#define RADIX_IS_AVSDGL  1
#define RADIX_NOT_AVSDGL 0

#define RADIX_INTP_HDIR  0
#define RADIX_INTP_VDIR  1

  enum radix_IntpID {
    RADIX_MPEG_HPEL = 0,
    RADIX_MPEG_QPEL,
    RADIX_H264_QPEL,
    RADIX_H264_EPEL,
    RADIX_RV8_TPEL,
    RADIX_RV9_QPEL,
    RADIX_RV9_CPEL,
    RADIX_WMV2_QPEL,
    RADIX_VC1_QPEL,
    RADIX_AVS_QPEL,
    RADIX_VP6_QPEL,
    RADIX_VP8_QPEL,
    RADIX_VP8_EPEL,
    RADIX_VP8_BIL,
    RADIX_VP8_FPEL, /*full-pixel for chroma*/
    RADIX_HEVC_QPEL,
    RADIX_HEVC_EPEL,
    RADIX_SVAC_QPEL,
    RADIX_SVAC_EPEL,
  };

enum radix_PosID {
  RADIX_H0V0 = 0,
  RADIX_H1V0,
  RADIX_H2V0,
  RADIX_H3V0,
  RADIX_H0V1,
  RADIX_H1V1,
  RADIX_H2V1,
  RADIX_H3V1,
  RADIX_H0V2,
  RADIX_H1V2,
  RADIX_H2V2,
  RADIX_H3V2,
  RADIX_H0V3,
  RADIX_H1V3,
  RADIX_H2V3,
  RADIX_H3V3,
};

enum radix_TapTYP {
  RADIX_TAP2 = 0,
  RADIX_TAP4,
  RADIX_TAP6,
  RADIX_TAP8,
};

enum radix_SPelSFT {
  RADIX_HPEL = 0,
  RADIX_QPEL,
  RADIX_EPEL,
};

typedef struct radix_IntpFMT_t{
  char tap;
  char intp_pkg[2];
  char hldgl;
  char avsdgl;
  char intp[2];
  char intp_dir[2];
  char intp_coef[2][8];
  char intp_rnd[2];
  char intp_sft[2];
  char intp_sintp[2];
  char intp_srnd[2];
  char intp_sbias[2];
}radix_IntpFMT_t;

const static char radix_AryFMT[] = {
  RADIX_IS_SKIRT, RADIX_IS_MIRROR, RADIX_IS_SKIRT, RADIX_IS_SKIRT,
  RADIX_IS_SKIRT, RADIX_IS_SKIRT, RADIX_IS_SKIRT, RADIX_IS_SKIRT,
  RADIX_IS_SKIRT, RADIX_IS_SKIRT, RADIX_IS_SKIRT, RADIX_IS_SKIRT,
  RADIX_IS_SKIRT, RADIX_IS_SKIRT, RADIX_IS_SKIRT, RADIX_IS_SKIRT, RADIX_IS_SKIRT,
};

const static char radix_SubPel[] = {
  RADIX_HPEL, RADIX_QPEL, RADIX_QPEL, RADIX_EPEL,
  RADIX_QPEL, RADIX_QPEL, RADIX_QPEL, RADIX_QPEL,
  RADIX_QPEL, RADIX_QPEL, RADIX_QPEL, RADIX_QPEL,
  RADIX_EPEL, RADIX_HPEL, RADIX_QPEL, RADIX_QPEL, RADIX_EPEL
};

const static radix_IntpFMT_t radix_IntpFMT[][16] = {
  {
    /************* MPEG_RADIX_HPEL ***************/
    {/*H0V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V1*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V1*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V1*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V1*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V3*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V3*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V3*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V3*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
  },

  {
    /************* MPEG_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 1},
      {/*intp_srnd*/1, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 1},
      {/*intp_srnd*/1, 1},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H1V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 1},
      {/*intp_srnd*/1, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H2V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H3V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 1},
      {/*intp_srnd*/1, 1},
      {/*intp_sbias*/1, 1},
    },
  },

  {
    /************* H264_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V1*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_IS_SCS}, RADIX_IS_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_IS_SCS}, RADIX_IS_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H0V3*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H1V3*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_IS_SCS}, RADIX_IS_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H2V3*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H3V3*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_IS_SCS}, RADIX_IS_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/1, 1},
    },
  },

  {
    /************* H264_RADIX_EPEL ***************/
    {/*H0V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/0},
    {/*H2V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V0*/0},
    {/*H0V1*/0},
    {/*H1V1*/0},
    {/*H2V1*/0},
    {/*H3V1*/0},
    {/*H0V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V2*/0},
    {/*H2V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V2*/0},
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* RV8_TPEL ***************/
    {/*H0V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 12, 6, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 6, 12, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/0},
    {/*H0V1*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, 12, 6, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 12, 6, -1, 0, 0, 0, 0}, {-1, 12, 6, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 0}, //{0,128}
      {/*intp_sft*/0, 8},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 6, 12, -1, 0, 0, 0, 0}, {-1, 12, 6, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 0}, //{0,128}
      {/*intp_sft*/0, 8},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/0},
    {/*H0V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, 6, 12, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 12, 6, -1, 0, 0, 0, 0}, {-1, 6, 12, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 0}, //{0,128}
      {/*intp_sft*/0, 8},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 6, 12, -1, 0, 0, 0, 0}, {-1, 6, 12, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 0}, //{0,128}
      {/*intp_sft*/0, 8},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/0},
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* RV9_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 52, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0}, {1, -5, 52, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 52, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 32},
      {/*intp_sft*/5, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 52, -5, 1, 0, 0}, {1, -5, 52, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 16},
      {/*intp_sft*/6, 5},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 52, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 16},
      {/*intp_sft*/6, 5},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 52, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0}, {1, -5, 20, 52, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 52, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 32},
      {/*intp_sft*/5, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* RV9_CPEL ***************/
    {/*H0V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0}, {3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 7},
      {/*intp_sft*/0, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 4},
      {/*intp_sft*/0, 3},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0}, {3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 7},
      {/*intp_sft*/0, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 4},
      {/*intp_sft*/0, 3},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 1},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 4},
      {/*intp_sft*/0, 3},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0}, {1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 7},
      {/*intp_sft*/0, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 4},
      {/*intp_sft*/0, 3},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 1},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* WMV2_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V1*/0},
    {/*H1V1*/0},
    {/*H2V1*/0},
    {/*H3V1*/0},
    {/*H0V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 8},
      {/*intp_sft*/4, 4},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 8},
      {/*intp_sft*/4, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 8},
      {/*intp_sft*/4, 4},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* VC1_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/31, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0}, {-4, 53, 18, -3, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 8},
      {/*intp_sft*/6, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0}, {-3, 18, 53, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/7, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-4, 53, 18, -3, 0, 0, 0, 0},},
      {/*intp_rnd*/7, 32},
      {/*intp_sft*/4, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/7, 8},
      {/*intp_sft*/4, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-3, 18, 53, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/7, 32},
      {/*intp_sft*/4, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/31, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0}, {-4, 53, 18, -3, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 8},
      {/*intp_sft*/6, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_IS_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_VDIR, RADIX_INTP_HDIR},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0}, {-3, 18, 53, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* AVS_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, -2, 96, 42, -7, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{0, -7, 42, 96, -2, -1, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, -2, 96, 42, -7, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_IS_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, -5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{0, -1, 5, 5, -1, 0, 0, 0}, {-1, -2, 96, 42, -7, 0, 0, 0},},
      {/*intp_rnd*/64, 0}, //{0,512}
      {/*intp_sft*/0, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_IS_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, -5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, -2, 96, 42, -7, 0, 0, 0}, {0, -1, 5, 5, -1, 0, 0, 0},},
      {/*intp_rnd*/64, 0}, //{0,512}
      {/*intp_sft*/0, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, 5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{0, -7, 42, 96, -2, -1, 0, 0}, {0, -1, 5, 5, -1, 0, 0, 0},},
      {/*intp_rnd*/64, 0}, //{0,512}
      {/*intp_sft*/0, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{0, -7, 42, 96, -2, -1, 0, 0}, {0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_IS_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, -5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H2V3*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{0, -1, 5, 5, -1, 0, 0, 0}, {0, -7, 42, 96, -2, -1, 0, 0},},
      {/*intp_rnd*/64, 0}, //{0,512}
      {/*intp_sft*/0, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_IS_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, -5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/1, 1},
    },
  },

  {
    /************* VP6_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP4, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0}, {-4, 109, 24, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0}, {-4, 109, 24, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0}, {-4, 109, 24, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0}, {-4, 68, 68, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0}, {-4, 68, 68, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0}, {-4, 68, 68, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0}, {-1, 24, 109, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0}, {-1, 24, 109, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      RADIX_TAP4, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0}, {-1, 24, 109, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* VP8_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP6, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0}, {0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0}, {2, -11, 108, 36, -8, 1, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0}, {2, -11, 108, 36, -8, 1, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0}, {2, -11, 108, 36, -8, 1, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0}, {3, -16, 77, 77, -16, 3, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0}, {3, -16, 77, 77, -16, 3, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0}, {3, -16, 77, 77, -16, 3, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0}, {1, -8, 36, 108, -11, 2, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0}, {1, -8, 36, 108, -11, 2, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      RADIX_TAP6, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0}, {1, -8, 36, 108, -11, 2, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* VP8_RADIX_EPEL ***************/
    {/*H0V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/0},
    {/*H2V0*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V0*/0},
    {/*H0V1*/0},
    {/*H1V1*/0},
    {/*H2V1*/0},
    {/*H3V1*/0},
    {/*H0V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V2*/0},
    {/*H2V2*/
      RADIX_TAP2, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 4},
      {/*intp_sft*/3, 3},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V2*/0},
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* VP8_BIL ***************/
    {/*H0V0*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V0*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V0*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V1*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V1*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 2},
      {/*intp_sft*/2, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V1*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/1, 2},
      {/*intp_sft*/1, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V1*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 2},
      {/*intp_sft*/2, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V2*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V2*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 1},
      {/*intp_sft*/2, 1},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V2*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/1, 1},
      {/*intp_sft*/1, 1},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V2*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 1},
      {/*intp_sft*/2, 1},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V3*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V3*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 2},
      {/*intp_sft*/2, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V3*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/1, 2},
      {/*intp_sft*/1, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V3*/
      RADIX_TAP2, {RADIX_IS_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 2},
      {/*intp_sft*/2, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
  },

  {
    /************* VP8_FPEL ***************/
    {/*H0V0*/0},
    {/*H1V0*/0},
    {/*H2V0*/0},
    {/*H3V0*/0},
    {/*H0V1*/0},
    {/*H1V1*/0},
    {/*H2V1*/0},
    {/*H3V1*/0},
    {/*H0V2*/0},
    {/*H1V2*/0},
    {/*H2V2*/0},
    {/*H3V2*/0},
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* HEVC_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/HEVC_CFIR_Y_COEF0,{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/HEVC_CFIR_Y_COEF1,{0},},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/HEVC_CFIR_Y_COEF2,{0},},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/HEVC_CFIR_Y_COEF3,{0},},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/HEVC_CFIR_Y_COEF1,{0},},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_Y_COEF1,HEVC_CFIR_Y_COEF1,},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, HEVC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_Y_COEF2,HEVC_CFIR_Y_COEF1,},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, HEVC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_Y_COEF3,HEVC_CFIR_Y_COEF1,},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, HEVC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/HEVC_CFIR_Y_COEF2,{0},},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_Y_COEF1, HEVC_CFIR_Y_COEF2,},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, HEVC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_Y_COEF2,HEVC_CFIR_Y_COEF2,},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, HEVC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_Y_COEF3,HEVC_CFIR_Y_COEF2,},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, HEVC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/HEVC_CFIR_Y_COEF3,{0},},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_Y_COEF1,HEVC_CFIR_Y_COEF3,},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, HEVC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_Y_COEF2,HEVC_CFIR_Y_COEF3,},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, HEVC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_Y_COEF3,HEVC_CFIR_Y_COEF3,},
      {/*intp_rnd*/HEVC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, HEVC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* HEVC_RADIX_EPEL ***************/
    {/*H0V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/HEVC_CFIR_C_COEF0,{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_C_COEF1,{0},},
      {/*intp_rnd*/HEVC_CFIR_C_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_C_SFT0, HEVC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_C_COEF2,{0},},
      {/*intp_rnd*/HEVC_CFIR_C_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_C_SFT0, HEVC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_C_COEF3,{0},},
      {/*intp_rnd*/HEVC_CFIR_C_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_C_SFT0, HEVC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H4V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_C_COEF4,{0},},
      {/*intp_rnd*/HEVC_CFIR_C_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_C_SFT0, HEVC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H5V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_C_COEF5,{0},},
      {/*intp_rnd*/HEVC_CFIR_C_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_C_SFT0, HEVC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H6V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_C_COEF6,{0},},
      {/*intp_rnd*/HEVC_CFIR_C_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_C_SFT0, HEVC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H7V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/HEVC_CFIR_C_COEF7,{0},},
      {/*intp_rnd*/HEVC_CFIR_C_RND0, 0},
      {/*intp_sft*/HEVC_CFIR_C_SFT0, HEVC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* SVAC_RADIX_QPEL ***************/
    {/*H0V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/SVAC_CFIR_Y_COEF0,{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/SVAC_CFIR_Y_COEF1,{0},},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/SVAC_CFIR_Y_COEF2,{0},},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, 0},
      {/*intp_coef*/SVAC_CFIR_Y_COEF3,{0},},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/SVAC_CFIR_Y_COEF1,{0},},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_Y_COEF1,SVAC_CFIR_Y_COEF1,},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, SVAC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_Y_COEF2,SVAC_CFIR_Y_COEF1,},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, SVAC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_Y_COEF3,SVAC_CFIR_Y_COEF1,},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, SVAC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/SVAC_CFIR_Y_COEF2,{0},},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_Y_COEF1, SVAC_CFIR_Y_COEF2,},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, SVAC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_Y_COEF2,SVAC_CFIR_Y_COEF2,},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, SVAC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_Y_COEF3,SVAC_CFIR_Y_COEF2,},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, SVAC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_VDIR, 0},
      {/*intp_coef*/SVAC_CFIR_Y_COEF3,{0},},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_Y_SFT0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_Y_COEF1,SVAC_CFIR_Y_COEF3,},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, SVAC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_Y_COEF2,SVAC_CFIR_Y_COEF3,},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, SVAC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 1}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_Y_COEF3,SVAC_CFIR_Y_COEF3,},
      {/*intp_rnd*/SVAC_CFIR_Y_RND0, 0},
      {/*intp_sft*/0, SVAC_CFIR_Y_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* SVAC_RADIX_EPEL ***************/
    {/*H0V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/SVAC_CFIR_C_COEF0,{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_C_COEF1,{0},},
      {/*intp_rnd*/SVAC_CFIR_C_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_C_SFT0, SVAC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_C_COEF2,{0},},
      {/*intp_rnd*/SVAC_CFIR_C_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_C_SFT0, SVAC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_C_COEF3,{0},},
      {/*intp_rnd*/SVAC_CFIR_C_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_C_SFT0, SVAC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H4V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_C_COEF4,{0},},
      {/*intp_rnd*/SVAC_CFIR_C_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_C_SFT0, SVAC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H5V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_C_COEF5,{0},},
      {/*intp_rnd*/SVAC_CFIR_C_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_C_SFT0, SVAC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H6V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_C_COEF6,{0},},
      {/*intp_rnd*/SVAC_CFIR_C_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_C_SFT0, SVAC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H7V0*/
      RADIX_TAP8, {RADIX_NOT_TCS, RADIX_NOT_SCS}, RADIX_NOT_HLDGL, RADIX_NOT_AVSDGL,
      {/*intp*/1, 0}, {RADIX_INTP_HDIR, RADIX_INTP_VDIR},
      {/*intp_coef*/SVAC_CFIR_C_COEF7,{0},},
      {/*intp_rnd*/SVAC_CFIR_C_RND0, 0},
      {/*intp_sft*/SVAC_CFIR_C_SFT0, SVAC_CFIR_C_SFT1},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

};

#define IS_SKIRT  0 //265 RADIX_IS_SKIRT
#define IS_MIRROR 1 //265 RADIX_IS_MIRROR

#define IS_BIAVG  0 //265 RADIX_IS_BIAVG
#define IS_WT1    1 //265 RADIX_IS_WT1
#define IS_WT2    2 //265 RADIX_IS_WT2
#define IS_FIXWT  3 //265 RADIX_IS_FIXWT

#define IS_ILUT0  0 //265 RADIX_IS_ILUT0
#define IS_ILUT1  2 //265 RADIX_IS_ILUT1
#define IS_EC     1 //265 RADIX_IS_EC

#define IS_TCS     1 //265 RADIX_IS_TCS
#define NOT_TCS    0 //265 RADIX_NOT_TCS
#define IS_SCS     1 //265 RADIX_IS_SCS
#define NOT_SCS    0 //265 RADIX_NOT_SCS
#define IS_HLDGL   1 //265 RADIX_IS_HLDGL
#define NOT_HLDGL  0 //265 RADIX_NOT_HLDGL
#define IS_AVSDGL  1 //265 RADIX_IS_AVSDGL
#define NOT_AVSDGL 0 //265 RADIX_NOT_AVSDGL

#define INTP_HDIR  0 //265 RADIX_INTP_HDIR
#define INTP_VDIR  1 //265 RADIX_INTP_VDIR

enum IntpID { //265 radix_IntpID
  MPEG_HPEL = 0,
  MPEG_QPEL,
  H264_QPEL,
  H264_EPEL,
  RV8_TPEL,
  RV9_QPEL,
  RV9_CPEL,
  WMV2_QPEL,
  VC1_QPEL,
  AVS_QPEL,
  VP6_QPEL,
  VP8_QPEL,
  VP8_EPEL,
  VP8_BIL,
  VP8_FPEL, /*full-pixel for chroma*/
  HEVC_QPEL,
  HEVC_EPEL,
};

enum PosID { //265 radix_PosID
  H0V0 = 0,
  H1V0,
  H2V0,
  H3V0,
  H0V1,
  H1V1,
  H2V1,
  H3V1,
  H0V2,
  H1V2,
  H2V2,
  H3V2,
  H0V3,
  H1V3,
  H2V3,
  H3V3,
};

enum TapTYP { // 265 radix_TapTYP
  TAP2 = 0,
  TAP4,
  TAP6,
  TAP8,
};

enum SPelSFT { // 265 radix_SPelSFT
  HPEL = 0,
  QPEL,
  EPEL,
};

typedef struct IntpFMT_t{ // 265 radix_IntpFMT_t
  char tap;
  char intp_pkg[2];
  char hldgl;
  char avsdgl;
  char intp[2];
  char intp_dir[2];
  char intp_coef[2][8];
  char intp_rnd[2];
  char intp_sft[2];
  char intp_sintp[2];
  char intp_srnd[2];
  char intp_sbias[2];
}IntpFMT_t;


__place_k0_data__
static IntpFMT_t IntpFMT[][16] = { // 265 radix_IntpFMT[][16]
  {
    /************* MPEG_HPEL ***************/
    {/*H0V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V1*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V1*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V1*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V1*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V3*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V3*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V3*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V3*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0} },
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
  },

  {
    /************* MPEG_QPEL ***************/
    {/*H0V0*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V1*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 1},
      {/*intp_srnd*/1, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 1},
      {/*intp_srnd*/1, 1},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V2*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V3*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1},{0},},
      {/*intp_rnd*/15, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H1V3*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 1},
      {/*intp_srnd*/1, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H2V3*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H3V3*/
      TAP8, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 3, -6, 20, 20, -6, 3, -1}, {-1, 3, -6, 20, 20, -6, 3, -1},},
      {/*intp_rnd*/15, 15},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/1, 1},
      {/*intp_srnd*/1, 1},
      {/*intp_sbias*/1, 1},
    },
  },

  {
    /************* H264_QPEL ***************/
    {/*H0V0*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V1*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP6, {IS_TCS, IS_SCS}, IS_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      TAP6, {IS_TCS, IS_SCS}, IS_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V2*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H0V3*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H1V3*/
      TAP6, {IS_TCS, IS_SCS}, IS_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H2V3*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 10},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H3V3*/
      TAP6, {IS_TCS, IS_SCS}, IS_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/1, 1},
    },
  },

  {
    /************* H264_EPEL ***************/
    {/*H0V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/0},
    {/*H2V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3 , 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V0*/0},
    {/*H0V1*/0},
    {/*H1V1*/0},
    {/*H2V1*/0},
    {/*H3V1*/0},
    {/*H0V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V2*/0},
    {/*H2V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V2*/0},
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* RV8_TPEL ***************/
    {/*H0V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 12, 6, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 6, 12, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/0},
    {/*H0V1*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 12, 6, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 12, 6, -1, 0, 0, 0, 0}, {-1, 12, 6, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 0}, //{0,128}
      {/*intp_sft*/0, 8},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 6, 12, -1, 0, 0, 0, 0}, {-1, 12, 6, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 0}, //{0,128}
      {/*intp_sft*/0, 8},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/0},
    {/*H0V2*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 6, 12, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 12, 6, -1, 0, 0, 0, 0}, {-1, 6, 12, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 0}, //{0,128}
      {/*intp_sft*/0, 8},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 6, 12, -1, 0, 0, 0, 0}, {-1, 6, 12, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 0}, //{0,128}
      {/*intp_sft*/0, 8},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/0},
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* RV9_QPEL ***************/
    {/*H0V0*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, -5, 20, 52, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0}, {1, -5, 52, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 52, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 32},
      {/*intp_sft*/5, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 52, -5, 1, 0, 0}, {1, -5, 52, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/16, 0},
      {/*intp_sft*/5, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 16},
      {/*intp_sft*/6, 5},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 16},
      {/*intp_sft*/5, 5},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 52, -5, 1, 0, 0}, {1, -5, 20, 20, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 16},
      {/*intp_sft*/6, 5},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, -5, 20, 52, -5, 1, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 52, 20, -5, 1, 0, 0}, {1, -5, 20, 52, -5, 1, 0, 0},},
      {/*intp_rnd*/32, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      TAP6, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -5, 20, 20, -5, 1, 0, 0}, {1, -5, 20, 52, -5, 1, 0, 0},},
      {/*intp_rnd*/16, 32},
      {/*intp_sft*/5, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 2},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* RV9_CPEL ***************/
    {/*H0V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0}, {3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 7},
      {/*intp_sft*/0, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 4},
      {/*intp_sft*/0, 3},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0}, {3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 7},
      {/*intp_sft*/0, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 4},
      {/*intp_sft*/0, 3},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 1},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 4},
      {/*intp_sft*/0, 3},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0}, {1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 7},
      {/*intp_sft*/0, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 4},
      {/*intp_sft*/0, 3},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 1},
      {/*intp_sft*/0, 2},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* WMV2_QPEL ***************/
    {/*H0V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/1, 0},
      {/*intp_srnd*/1, 0},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V1*/0},
    {/*H1V1*/0},
    {/*H2V1*/0},
    {/*H3V1*/0},
    {/*H0V2*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 8},
      {/*intp_sft*/4, 4},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 8},
      {/*intp_sft*/4, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/8, 8},
      {/*intp_sft*/4, 4},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* VC1_QPEL ***************/
    {/*H0V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/8, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/31, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0}, {-4, 53, 18, -3, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 8},
      {/*intp_sft*/6, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-4, 53, 18, -3, 0, 0, 0, 0}, {-3, 18, 53, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/7, 0},
      {/*intp_sft*/4, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-4, 53, 18, -3, 0, 0, 0, 0},},
      {/*intp_rnd*/7, 32},
      {/*intp_sft*/4, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/7, 8},
      {/*intp_sft*/4, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-1, 9, 9, -1, 0, 0, 0, 0}, {-3, 18, 53, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/7, 32},
      {/*intp_sft*/4, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/31, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0}, {-4, 53, 18, -3, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0}, {-1, 9, 9, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 8},
      {/*intp_sft*/6, 4},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      TAP4, {NOT_TCS, IS_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_VDIR, INTP_HDIR},
      {/*intp_coef*/{-3, 18, 53, -4, 0, 0, 0, 0}, {-3, 18, 53, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/31, 32},
      {/*intp_sft*/6, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* AVS_QPEL ***************/
    {/*H0V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, -2, 96, 42, -7, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{0, -7, 42, 96, -2, -1, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, -2, 96, 42, -7, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, IS_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, -5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{0, -1, 5, 5, -1, 0, 0, 0}, {-1, -2, 96, 42, -7, 0, 0, 0},},
      {/*intp_rnd*/64, 0}, //{0,512}
      {/*intp_sft*/0, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, IS_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, -5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/1, 0},
    },
    {/*H0V2*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, -2, 96, 42, -7, 0, 0, 0}, {0, -1, 5, 5, -1, 0, 0, 0},},
      {/*intp_rnd*/64, 0}, //{0,512}
      {/*intp_sft*/0, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, 5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{0, -7, 42, 96, -2, -1, 0, 0}, {0, -1, 5, 5, -1, 0, 0, 0},},
      {/*intp_rnd*/64, 0}, //{0,512}
      {/*intp_sft*/0, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{0, -7, 42, 96, -2, -1, 0, 0}, {0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, IS_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, -5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/0, 1},
    },
    {/*H2V3*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{0, -1, 5, 5, -1, 0, 0, 0}, {0, -7, 42, 96, -2, -1, 0, 0},},
      {/*intp_rnd*/64, 0}, //{0,512}
      {/*intp_sft*/0, 10},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, IS_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 5, 5, -1, 0, 0, 0, 0}, {-1, 5, -5, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/0, 32},
      {/*intp_sft*/0, 6},
      {/*intp_sintp*/0, 1},
      {/*intp_srnd*/0, 1},
      {/*intp_sbias*/1, 1},
    },
  },

  {
    /************* VP6_QPEL ***************/
    {/*H0V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0}, {-4, 109, 24, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0}, {-4, 109, 24, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0}, {-4, 109, 24, -1, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0}, {-4, 68, 68, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0}, {-4, 68, 68, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0}, {-4, 68, 68, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-4, 109, 24, -1, 0, 0, 0, 0}, {-1, 24, 109, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-4, 68, 68, -4, 0, 0, 0, 0}, {-1, 24, 109, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      TAP4, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 24, 109, -4, 0, 0, 0, 0}, {-1, 24, 109, -4, 0, 0, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* VP8_QPEL ***************/
    {/*H0V0*/
      TAP6, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0}, {0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0}, {2, -11, 108, 36, -8, 1, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0}, {2, -11, 108, 36, -8, 1, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0}, {2, -11, 108, 36, -8, 1, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0}, {3, -16, 77, 77, -16, 3, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0}, {3, -16, 77, 77, -16, 3, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0}, {3, -16, 77, 77, -16, 3, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0},{0},},
      {/*intp_rnd*/64, 0},
      {/*intp_sft*/7, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{2, -11, 108, 36, -8, 1, 0, 0}, {1, -8, 36, 108, -11, 2, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{3, -16, 77, 77, -16, 3, 0, 0}, {1, -8, 36, 108, -11, 2, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      TAP6, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, -8, 36, 108, -11, 2, 0, 0}, {1, -8, 36, 108, -11, 2, 0, 0},},
      {/*intp_rnd*/64, 64},
      {/*intp_sft*/7, 7},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* VP8_EPEL ***************/
    {/*H0V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/0},
    {/*H2V0*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V0*/0},
    {/*H0V1*/0},
    {/*H1V1*/0},
    {/*H2V1*/0},
    {/*H3V1*/0},
    {/*H0V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 0},
      {/*intp_sft*/3, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V2*/0},
    {/*H2V2*/
      TAP2, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/4, 4},
      {/*intp_sft*/3, 3},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V2*/0},
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* VP8_BIL ***************/
    {/*H0V0*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V0*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V0*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V1*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V1*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 2},
      {/*intp_sft*/2, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V1*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/1, 2},
      {/*intp_sft*/1, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V1*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{3, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 2},
      {/*intp_sft*/2, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V2*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/1, 0},
      {/*intp_sft*/1, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V2*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 1},
      {/*intp_sft*/2, 1},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V2*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/1, 1},
      {/*intp_sft*/1, 1},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V2*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{1, 1, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 1},
      {/*intp_sft*/2, 1},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H0V3*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/2, 0},
      {/*intp_sft*/2, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V3*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{3, 1, 0, 0, 0, 0, 0, 0},{1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 2},
      {/*intp_sft*/2, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H2V3*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 1, 0, 0, 0, 0, 0, 0},{1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/1, 2},
      {/*intp_sft*/1, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H3V3*/
      TAP2, {IS_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{1, 3, 0, 0, 0, 0, 0, 0},{1, 3, 0, 0, 0, 0, 0, 0},},
      {/*intp_rnd*/2, 2},
      {/*intp_sft*/2, 2},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
  },

  {
    /************* VP8_FPEL ***************/
    {/*H0V0*/0},
    {/*H1V0*/0},
    {/*H2V0*/0},
    {/*H3V0*/0},
    {/*H0V1*/0},
    {/*H1V1*/0},
    {/*H2V1*/0},
    {/*H3V1*/0},
    {/*H0V2*/0},
    {/*H1V2*/0},
    {/*H2V2*/0},
    {/*H3V2*/0},
    {/*H0V3*/0},
    {/*H1V3*/0},
    {/*H2V3*/0},
    {/*H3V3*/0},
  },

  {
    /************* HEVC_QPEL ***************/
    {/*H0V0*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 4, -10, 58, 17, -5, 1, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{-1, 4, -11, 40, 40, -11, 4, -1},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, 0},
      {/*intp_coef*/{0, 1, -5, 17, 58, -10, 4, -1},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V1*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 4, -10, 58, 17, -5, 1, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V1*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 4, -10, 58, 17, -5, 1, 0},{-1, 4, -10, 58, 17, -5, 1, 0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/0, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V1*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 4, -11, 40, 40, -11, 4, -1},{-1, 4, -10, 58, 17, -5, 1, 0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/0, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V1*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{0, 1, -5, 17, 58, -10, 4, -1},{-1, 4, -10, 58, 17, -5, 1, 0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/0, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{-1, 4, -11, 40, 40, -11, 4, -1},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 4, -10, 58, 17, -5, 1, 0},{-1, 4, -11, 40, 40, -11, 4, -1},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/0, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 4, -11, 40, 40, -11, 4, -1},{-1, 4, -11, 40, 40, -11, 4, -1},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/0, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{0, 1, -5, 17, 58, -10, 4, -1},{-1, 4, -11, 40, 40, -11, 4, -1},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/0, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_VDIR, 0},
      {/*intp_coef*/{0, 1, -5, 17, 58, -10, 4, -1},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 4, -10, 58, 17, -5, 1, 0},{0, 1, -5, 17, 58, -10, 4, -1},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/0, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-1, 4, -11, 40, 40, -11, 4, -1},{0, 1, -5, 17, 58, -10, 4, -1},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/0, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      TAP8, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 1}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{0, 1, -5, 17, 58, -10, 4, -1},{0, 1, -5, 17, 58, -10, 4, -1},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/0, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

  {
    /************* HEVC_EPEL ***************/
    {/*H0V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0},
      {/*intp_coef*/{1, 0, 0, 0, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0},
      {/*intp_srnd*/0},
      {/*intp_sbias*/0},
    },
    {/*H1V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-2, 58, 10, -2, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-4, 54, 16, -2, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-6, 46, 28, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H4V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-4, 36, 36, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H5V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-4, 28, 46, -6, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H6V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-2, 16, 54, -4, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H7V0*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/1, 0}, {INTP_HDIR, INTP_VDIR},
      {/*intp_coef*/{-2, 10, 58, -2, 0, 0, 0, 0},{0},},
      {/*intp_rnd*/32, 0},
      {/*intp_sft*/6, 12},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V2*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V2*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V2*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V2*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H0V3*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H1V3*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H2V3*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
    {/*H3V3*/
      TAP4, {NOT_TCS, NOT_SCS}, NOT_HLDGL, NOT_AVSDGL,
      {/*intp*/0, 0}, {0, 0},
      {/*intp_coef*/{0},{0},},
      {/*intp_rnd*/0, 0},
      {/*intp_sft*/0, 0},
      {/*intp_sintp*/0, 0},
      {/*intp_srnd*/0, 0},
      {/*intp_sbias*/0, 0},
    },
  },

};

//#define HERA_JPEG_JOIN_IN
#ifdef HERA_JPEG_JOIN_IN
uint8_t *hera_jpeg_bs_addr;
#endif

#endif /*__HERA_H__*/
