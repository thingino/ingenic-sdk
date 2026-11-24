/****************************************************************
*****************************************************************/
#ifndef __HELIX_H__
#define __HELIX_H__

/****************************************************************
  VPU register map
*****************************************************************/
#define JPEG_BASE                0x132F0000  /* jpeg register base */
#define JPEG_LEN                 0x10000     /* jpeg register len */

/****************
 * REGISTER MAP
 ****************/
#define JPEG_TRIG                0x0       /* trigger register */
#define JPEG_CTRL                0x4       /* global control register */
#define JPEG_NMCU                0x8       /* number of mcu(yuv420: 16 * 16, yuv422: 16 * 32, y: 16 * 16). for example, nmcu = (1920 * 1080) / (16 * 32) */
#define JPEG_FRM_INFO            0xc       /* jpeg width and height */
#define JPEG_LINE_STRD           0x10      /* one line stride (for example 1080P, yuv420: 1920, yuv422: 1920 * 2) */
#define JPEG_RAWY_ADDR           0x14      /* (256byte align) Y pixel start addr (buffer0 addr) */
#define JPEG_RAWC_ADDR           0x18      /* (256byte align) UV pixel start addr (yuy2 is useless)(buffer0 + UV offset) */
#define JPEG_BS_ADDR             0x1C
#define JPEG_CODE_PIX_TOTAL      0x20      /* pixel total number(width/height must 16 align) */
#define JPEG_MARKERS             0x28      /* restart markers function */
#define JPEG_LIMIT_BS_LEN_VAL    0x2c
#define JPEG_TIMEOUT_THRD        0x40
#define JPEG_BS1_ADDR            0x44
#define JPEG_BSFULL_EN           0x48
#define JPEG_BSFULL_SIZE         0x4c
#define JPEG_BSFULL_STATE        0x50
#define JPEG_DECODE_MODE         0x54
#define JPEG_VERSION_NUM         0x200     /* jpeg version */
#define JPEG_GLB_STAT            0x100     /* DEBUG: global status register */
#define JPEG_GLB_BS_LEN          0x104     /* DEBUG */
#define JPEG_INTR_STA            0x110     /* interrupt status */
#define JPEG_INTR_CLR            0x114     /* interrupt clear */
#define JPEG_BSBI_BS_LEN         0x154

#define JPEG_QT_BASE_TAB         0x300     /* 0x300~0x37c, 0x300~0x33c, y base table, 0x340~0x37c, uv base table, base qt of hardware mode */
#define JPEG_QT_TAB              0x400     /* 0x400~0x9fc, constant of 3 qts, every qt contains Y qt and UV qt */
#define JPEG_HUB_TAB             0xa00     /* 0xa00~0x106c, constant of 1 huffman tables which contains dc and ac */

/* JPEG_TRIG */
#define JPEG_START               0         //start encoder one frame(only used in manual_mode)
#define JPEG_SEQ_RESET           1         //reset all registers, only used in initial or uninitial
#define JPEG_FRM_RESET           2         //reset frame level registers after frame down(only used in manual reset mode)
#define JPEG_SLV_RESET           3         //reset slv register (frame H/W,raw addr....)
#define JPEG_RESET_CYCLE         4         //clock cycle of keeping status reset
#define JPEG_FSM_RESET           8         //reset status register, use it in necessary condition
#define RESET_END                31

/* JPEG_CTRL */
#define CTRL_HD_Y                0         //huffman DC table number for Y (default: 0)
#define CTRL_HA_Y                1         //huffman AC table number for Y (default: 0)
#define CTRL_QT_Y                2         //quant table number for Y
#define CTRL_HD_U                4         //huffman DC table number for U (default: 1)
#define CTRL_HA_U                5         //huffman AC table number for U (default: 1)
#define CTRL_QT_U                6         //quant table number for U (Y/U/V has same qt index)
#define CTRL_HD_V                8         //huffman DC table number for V (default: 1)
#define CTRL_HA_V                9         //huffman AC table number for V (default: 1)
#define CTRL_QT_V                10        //quant table number for U (Y/U/V has same qt index)
#define AWLEN                    12        //bs write out AXI bus awlen (default: 0)
#define LIMIT_BS_LEN_EN          16        //limit bs len enable (default: 0)
#define TIMEOUT_EN               17
#define CTRL_QP                  18        //only used hardware generate mode, qt index, range is 0~98
#define CTRL_BSFULL_INTR_EN      29        //bsfull intr enable

/* JPEG INTERRUPT EN*/
#define JPEG_FRAME_END_INTR_EN   25        //jpeg encode normal end interrupt
#define JPEG_FRAME_END_BSLESS_INTR_EN 26   //jpeg encode normal end interrupt (open bs limit)
#define IVDC_STOP_REQ_INTR_EN    27        //jpeg ivdc stop req interrupt
#define JPEG_TIMEOUT_INTR_EN     28        //jpeg timeout interrupt
#define GRAY_EN                  31        //only encode grey

/* JPEG_NMCU */
#define FRM_MCU_NUMBER           0         //number of mcu(yuv420: 16 * 16, yuv422: 16 * 8, y: 16 * 16). for example, nmcu = (1920 * 1088) / (16 * 8) - 1

/* JPEG_FRM_INFO */
#define FRM_WIDTH                0         //origin width
#define FRM_HEIGHT               16        //origin height

/* JPEG_LINE_STRD */
#define LINE_STRD                0         //yuv422(width * 2), yuv420/y(width), width must 16-align

/* JPEG MARKERS */
#define MARKERS_ENABLE           0         //enable restart marker function
#define MARKERS_NUMBER           16        //markers MCU number reduce 1

/* JPEG LIMIT BS LEN VAL */
#define LIMIT_BS_LEN_VAL         0

/* JPEG_GLB_STAT */
#define BS_LEN                   0         //size of one encoded frame, the unit is byte
#define BUS_FREE                 29        //indicate axi bus free, use with TRIG_BUS_STOP
#define FRM_END                  31        //current frame encoder end

/* JPEG_GLB_FSM */
#define CTRL_FSM                 0         //TODO:
#define RESET_FSM                4         //TODO:
#define SLV_FSM0                 8         //TODO:
#define SLV_FSM1                 12        //TODO:

/* JPEG INTERRUPT */
#define JPEG_FRAME_END_INTR   0        //jpeg encode normal end interrupt
#define JPEG_FRAME_END_BSLESS_INTR 1   //jpeg encode normal end interrupt (open bs limit)
#define IVDC_STOP_REQ_INTR    2        //jpeg ivdc stop req interrupt
#define JPEG_TIMEOUT_INTR     3        //jpeg timeout interrupt
#define JPEG_BSFULL_INTR_STAT 4        //jpeg bsfull interrupt

#define HUFFENC_LEN             (412)
uint32_t huffenc_t32v[HUFFENC_LEN] = {
    0x000100, 0x010101, 0x050204, 0x01030b, 0x01041a, 0x010678, 0x0107f8, 0x0109f6,
    0x010f82, 0x000f83, 0x00030c, 0x00041b, 0x000679, 0x0008f6, 0x000af6, 0x000f84,
    0x000f85, 0x010f86, 0x020f87, 0x030f88, 0x04041c, 0x0507f9, 0x0609f7, 0x070bf4,
    0x080f89, 0x090f8a, 0x0a0f8b, 0x0b0f8c, 0x000f8d, 0x020f8e, 0x01053a, 0x0308f7,
    0x030bf5, 0x020f8f, 0x040f90, 0x030f91, 0x050f92, 0x050f93, 0x040f94, 0x040f95,
    0x00053b, 0x0009f8, 0x010f96, 0x7d0f97, 0x010f98, 0x020f99, 0x030f9a, 0x000f9b,
    0x040f9c, 0x110f9d, 0x05067a, 0x120af7, 0x210f9e, 0x310f9f, 0x410fa0, 0x060fa1,
    0x130fa2, 0x510fa3, 0x610fa4, 0x070fa5, 0x22067b, 0x710bf6, 0x140fa6, 0x320fa7,
    0x810fa8, 0x910fa9, 0xa10faa, 0x080fab, 0x230fac, 0x420fad, 0xb107fa, 0xc10bf7,
    0x150fae, 0x520faf, 0xd10fb0, 0xf00fb1, 0x240fb2, 0x330fb3, 0x620fb4, 0x720fb5,
    0x8208f8, 0x090ec0, 0x0a0fb6, 0x160fb7, 0x170fb8, 0x180fb9, 0x190fba, 0x1a0fbb,
    0x250fbc, 0x260fbd, 0x2708f9, 0x280fbe, 0x290fbf, 0x2a0fc0, 0x340fc1, 0x350fc2,
    0x360fc3, 0x370fc4, 0x380fc5, 0x390fc6, 0x3a08fa, 0x430fc7, 0x440fc8, 0x450fc9,
    0x460fca, 0x470fcb, 0x480fcc, 0x490fcd, 0x4a0fce, 0x530fcf, 0x5409f9, 0x550fd0,
    0x560fd1, 0x570fd2, 0x580fd3, 0x590fd4, 0x5a0fd5, 0x630fd6, 0x640fd7, 0x650fd8,
    0x6609fa, 0x670fd9, 0x680fda, 0x690fdb, 0x6a0fdc, 0x730fdd, 0x740fde, 0x750fdf,
    0x760fe0, 0x770fe1, 0x780af8, 0x790fe2, 0x7a0fe3, 0x830fe4, 0x840fe5, 0x850fe6,
    0x860fe7, 0x870fe8, 0x880fe9, 0x890fea, 0x8a0feb, 0x920fec, 0x930fed, 0x940fee,
    0x950fef, 0x960ff0, 0x970ff1, 0x980ff2, 0x990ff3, 0x9a0ff4, 0xa20ff5, 0xa30ff6,
    0xa40ff7, 0xa50ff8, 0xa60ff9, 0xa70ffa, 0xa80ffb, 0xa90ffc, 0xaa0ffd, 0xb20ffe,
    0xb3030a, 0xb40af9, 0xb50fff, 0xb60fff, 0xb70fff, 0xb80fff, 0xb90fff, 0xba0fff,
    0xc20fd0, 0xc30fd1, 0xc40fd2, 0xc50fd3, 0xc60fd4, 0xc70fd5, 0xc80fd6, 0xc90fd7,
    0xca0101, 0xd20204, 0xd3030a, 0xd40418, 0xd50419, 0xd60538, 0xd70678, 0xd808f4,
    0xd909f6, 0xda0bf4, 0xe1030b, 0xe20539, 0xe307f6, 0xe408f5, 0xe50af6, 0xe60bf5,
    0xe70f88, 0xe80f89, 0xe90f8a, 0xea0f8b, 0xf1041a, 0xf207f7, 0xf309f7, 0xf40bf6,
    0xf50ec2, 0xf60f8c, 0xf70f8d, 0xf80f8e, 0xf90f8f, 0xfa0f90, 0x00041b, 0x0307f8,
    0x0109f8, 0x010bf7, 0x010f91, 0x010f92, 0x010f93, 0x010f94, 0x010f95, 0x010f96,
    0x01053a, 0x0008f6, 0x000f97, 0x000f98, 0x000f99, 0x000f9a, 0x000f9b, 0x010f9c,
    0x020f9d, 0x030f9e, 0x04053b, 0x0509f9, 0x060f9f, 0x070fa0, 0x080fa1, 0x090fa2,
    0x0a0fa3, 0x0b0fa4, 0x000fa5, 0x020fa6, 0x010679, 0x020af7, 0x040fa7, 0x040fa8,
    0x030fa9, 0x040faa, 0x070fab, 0x050fac, 0x040fad, 0x040fae, 0x00067a, 0x010af8,
    0x020faf, 0x770fb0, 0x000fb1, 0x010fb2, 0x020fb3, 0x030fb4, 0x110fb5, 0x040fb6,
    0x0507f9, 0x210fb7, 0x310fb8, 0x060fb9, 0x120fba, 0x410fbb, 0x510fbc, 0x070fbd,
    0x610fbe, 0x710fbf, 0x1308f7, 0x220fc0, 0x320fc1, 0x810fc2, 0x080fc3, 0x140fc4,
    0x420fc5, 0x910fc6, 0xa10fc7, 0xb10fc8, 0xc108f8, 0x090fc9, 0x230fca, 0x330fcb,
    0x520fcc, 0xf00fcd, 0x150fce, 0x620fcf, 0x720fd0, 0xd10fd1, 0x0a08f9, 0x160fd2,
    0x240fd3, 0x340fd4, 0xe10fd5, 0x250fd6, 0xf10fd7, 0x170fd8, 0x180fd9, 0x190fda,
    0x1a08fa, 0x260fdb, 0x270fdc, 0x280fdd, 0x290fde, 0x2a0fdf, 0x350fe0, 0x360fe1,
    0x370fe2, 0x380fe3, 0x390af9, 0x3a0fe4, 0x430fe5, 0x440fe6, 0x450fe7, 0x460fe8,
    0x470fe9, 0x480fea, 0x490feb, 0x4a0fec, 0x530de0, 0x540fed, 0x550fee, 0x560fef,
    0x570ff0, 0x580ff1, 0x590ff2, 0x5a0ff3, 0x630ff4, 0x640ff5, 0x650ec3, 0x660ff6,
    0x670ff7, 0x680ff8, 0x690ff9, 0x6a0ffa, 0x730ffb, 0x740ffc, 0x750ffd, 0x760ffe,
    0x770100, 0x7809fa, 0x790fff, 0x7a0fff, 0x820fff, 0x830fff, 0x840fff, 0x850fff,
    0x860fd0, 0x870fd1, 0x880fd2, 0x890fd3, 0x8a0fd4, 0x920fd5, 0x930fd6, 0x940fd7,
    0x950100, 0x960202, 0x970203, 0x980204, 0x990205, 0x9a0206, 0xa2030e, 0xa3041e,
    0xa4053e, 0xa5067e, 0xa607fe, 0xa708fe, 0xa80fff, 0xa90fff, 0xaa0fff, 0xb20fff,
    0xb30100, 0xb40101, 0xb50102, 0xb60206, 0xb7030e, 0xb8041e, 0xb9053e, 0xba067e,
    0xc207fe, 0xc308fe, 0xc409fe, 0xc50afe, 0xc60fff, 0xc70fff, 0xc80fff, 0xc90fff,
    0xca0000, 0xd20000, 0xd30000, 0xd40000, 0xd50000, 0xd60000, 0xd70000, 0xd80000,
    0xd90000, 0xda0000, 0xe20000, 0xe30000, 0xe40000, 0xe50000, 0xe60000, 0xe70000,
    0xe80000, 0xe90000, 0xea0000, 0xf20000, 0xf30000, 0xf40000, 0xf50000, 0xf60000,
    0xf70000, 0xf80000, 0xf90000, 0xfa0000
};

#define CPM_HELIX_SR(ID)		(31)
#define CPM_HELIX_STP(ID)		(27)
#define CPM_HELIX_ACK(ID)		(26)

#endif /*__HELIX_H__*/
