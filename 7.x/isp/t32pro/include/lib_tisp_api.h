#ifndef __LIB_TISP_API_H__
#define __LIB_TISP_API_H__

#include <stdbool.h>
#include "tx_isp_common.h"

#define TISPNUM SENSOR_NUM

typedef enum {
	TISP_RGGB = 0, /**< DISABLE mode of the current module */
	TISP_BGGR,	   /**< ENABLE mode of the current module */
	TISP_GRBG,	   /**< effect paramater, parameters have to be less than this value*/
	TISP_GBRG,
	TISP_RGIB = 8,
	TISP_BGIR,
	TISP_RIGB,
	TISP_BIGR,
	TISP_GRBI,
	TISP_GBRI,
	TISP_IRBG,
	TISP_IBRG,
	TISP_RGGI,
	TISP_BGGI,
	TISP_GRIG,
	TISP_GBIG,
	TISP_GIRG,
	TISP_GIBG,
	TISP_IGGR,
	TISP_IGGB,
} tisp_bayer_type;

typedef enum {
	TISP_OPS_TYPE_AUTO,	  /**< AUTO mode of the current module*/
	TISP_OPS_TYPE_MANUAL, /**< MANUAL mode of the current module*/
	TISP_OPS_TYPE_BUTT,	  /**< effect paramater, parameters have to be less than this value*/
} TISP_OPS_TYPE;

typedef enum {
	TISP_OPS_MODE_DISABLE, /**< DISABLE mode of the current module */
	TISP_OPS_MODE_ENABLE,  /**< ENABLE mode of the current module */
	TISP_OPS_MODE_BUTT,	   /**< effect paramater, parameters have to be less than this value*/
} TISP_OPS_MODE;

// RISCV
typedef struct lib_awb_flag {
	uint8_t vu8FlagFirstFrame;
	uint8_t vu8FlagDNRefresh;
	uint32_t vu32FlagFrameCnt;
	uint8_t vu8FlagStable;
} lib_awb_flag_t;

typedef struct lib_awb_rgbg_ct_rec {
	uint32_t au32Irg[15];
	uint32_t au32Ibg[15];
	uint32_t au32Ict[15];
} lib_awb_rgbg_ct_rec_t;

typedef struct lib_awb_api_params {
	uint32_t vu32Ev;
	uint32_t vu32Tgain;
	uint8_t vu8FlagTrig;
	uint8_t vu8FlagFreez;
	uint8_t vu8ManualMode;
	uint32_t vu32CtManual;
	uint16_t au16ManualRgBg[2];
	uint16_t au16GlobalRgBg[6];
	uint16_t au16WeightRgBg[4];
	uint32_t au16ZoneEveR[1024];
	uint32_t au16ZoneEveG[1024];
	uint32_t au16ZoneEveB[1024];
} lib_awb_api_params_t;

typedef struct __attribute__((aligned(256))) riscv_para {
	// AE 参数
	uint32_t head;
	uint32_t is_night;
	uint32_t aeResult[6];
	uint32_t luma;
	uint32_t ev;
	uint32_t num;

	// AWB 参数
	uint32_t awbMfPara[6];
	uint16_t awbCof[6];
	uint32_t awbCtManual;

	uint32_t sensor_type;
	uint32_t codec_status;
	uint32_t ae_stable;

	// mdns
	uint32_t mdnsPaddr;
	uint32_t mdnsSize;
	uint32_t mdnsRiscvInfo0;
	uint32_t mdnsRiscvInfo1;
	uint32_t mdnsRiscvInfo2;

	uint8_t vu8FlagStable;
	uint8_t riscv_enable;
	uint8_t riscv_mdns_enable;

	lib_awb_flag_t stFlag;
	lib_awb_rgbg_ct_rec_t stCtRec;
	lib_awb_api_params_t stApiPara;
} riscv_para_t;

struct isp_core_sensor_attr {
	unsigned int hts;	 /* sensor hts */
	unsigned int vts;	 /* sensor vts */
	unsigned int fps;	 /* sensor fps: */
	unsigned int width;	 /* sensor width*/
	unsigned int height; /* sensor height*/
};

typedef struct {
	ISP_CORE_HVFLIP sensor_mode;
	ISP_CORE_HVFLIP isp_mode[3];
} tisp_hv_flip_t;

typedef struct tisp_ncu_info {
	uint32_t width;
	uint32_t height;
	uint32_t sta_y_block_size;
	uint32_t sta_y_stride;
	uint32_t sta_y_buf_size;
} tisp_ncu_info_t;

/* typedef enum { */
/* 	IMP_ISP_GAMMA_CURVE_DEFAULT, */
/* 	IMP_ISP_GAMMA_CURVE_SRGB, */
/*         IMP_ISP_GAMMA_CURVE_REC709, */
/* 	IMP_ISP_GAMMA_CURVE_HDR, */
/* 	IMP_ISP_GAMMA_CURVE_USER, */
/* 	IMP_ISP_GAMMA_CURVE_BUTT, */
/* } tisp_gamma_type_t; */
/* typedef struct tisp_gamma_lut{ */
/* 	tisp_gamma_type_t type; */
/* 	unsigned short gamma[129]; */
/* } tisp_gamma_attr_t; */

typedef struct {
	unsigned char weight[225];
} tisp_ae_wgt_t;

typedef struct tisp_ae_zone_info {
	unsigned int zone[15][15];
} tisp_ae_zone_info_t;

typedef struct {
	unsigned char weight[225];
} tisp_af_weight_t;

typedef struct tisp_af_zone_info {
	uint32_t fir0;
	uint32_t fir1;
	uint32_t iir0;
	uint32_t iir1;
	uint32_t y_sum;
	uint32_t high_luma_cnt;
} tisp_af_zone_info_t;

typedef struct {
	unsigned char weight[1024];
} tisp_awb_weight_t;

typedef struct tisp_awb_zone_info {
	unsigned int zone[32][32];
} tisp_awb_zone_info_t;

typedef struct {
	tisp_awb_zone_info_t wb_r;
	tisp_awb_zone_info_t wb_g;
	tisp_awb_zone_info_t wb_b;
} tisp_wb_statis_info_t;

typedef enum tisp_wb_algo_t {
	IMPISP_AWB_ALGO_NORMAL,
	IMPISP_AWB_ALGO_GRAYWORLD,
	IMPISP_AWB_ALGO_REWEIGHT,
} TISP_AWB_ALGO;

typedef struct tisp_ae_ex_min {
	unsigned int min_it;
	unsigned int min_again;
	unsigned int min_it_short;
	unsigned int min_again_short;
} tisp_ae_ex_min_t;

typedef union tisp_module_control {
	unsigned int key;
	struct {
		unsigned int bitBypassDPC : 1;	 /* [0]  */
		unsigned int bitBypassGIB : 1;	 /* [1]  */
		unsigned int bitBypassLSC : 1;	 /* [2]  */
		unsigned int bitBypassAWBG : 1;	 /* [3]  */
		unsigned int bitBypassTMO : 1;	 /* [4]  */
		unsigned int bitBypassDMSC : 1;	 /* [5]	 */
		unsigned int bitBypassCCM : 1;	 /* [6]  */
		unsigned int bitBypassGAMMA : 1; /* [7]	 */
		unsigned int bitBypassDEFOG : 1; /* [8]	 */
		unsigned int bitBypassCSC : 1;	 /* [9]  */
		unsigned int bitBypassLCE : 1;	 /* [10] */
		unsigned int bitBypassMDNS : 1;	 /* [11] */
		unsigned int bitBypassYDNS : 1;	 /* [12] */
		unsigned int bitBypassBCSH : 1;	 /* [13] */
		unsigned int bitBypassCLM : 1;	 /* [14] */
		unsigned int bitBypassYSP : 1;	 /* [15] */
		unsigned int bitBypassSDNS : 1;	 /* [16] */
		unsigned int bitBypassCDNS : 1;	 /* [17] */
		unsigned int bitBypassCSCCR : 1; /* [18] */
		unsigned int bitBypassRDNS : 1;	 /* [19] */
		unsigned int bitBypassDECMP : 1; /* [20] */
		unsigned int bitRsv0 : 1;		 /* [21] */
		unsigned int bitBypassWDR : 1;	 /* [22] */
		unsigned int bitRsv1 : 9;		 /* [23 ~ 31]	*/
	};
} tisp_module_control_t;

typedef struct tisp_autozoom_control {
	int32_t zoom_chx_en[3];
	int32_t zoom_left[3];
	int32_t zoom_top[3];
	int32_t zoom_width[3];
	int32_t zoom_height[3];
} tisp_autozoom_attr_t;

typedef struct tisp_fcrop_control {
	bool fcrop_enable;
	unsigned int fcrop_top;
	unsigned int fcrop_left;
	unsigned int fcrop_width;
	unsigned int fcrop_height;
} tisp_fcrop_attr_t;

/* typedef enum { */
/* 	ISP_CSC_CG_BT601_FULL, */
/* 	ISP_CSC_CG_BT601_LIMITED, */
/* 	ISP_CSC_CG_BT709_FULL, */
/* 	ISP_CSC_CG_BT709_LIMITED, */
/*         ISP_CSC_CG_BT2020_FULL, */
/*         ISP_CSC_CG_BT2020_LIMITED, */
/* 	ISP_CSC_CG_USER, */
/* 	ISP_CSC_CG_BUTT, */
/* } tisp_csc_cg_t; */

/* typedef struct { */
/* 	unsigned int  csc_matrix[9]; */
/* 	unsigned char csc_offset_y; */
/* 	unsigned char csc_offset_uv; */
/* 	unsigned char csc_clip_y_min; */
/* 	unsigned char csc_clip_y_max; */
/* 	unsigned char csc_clip_uv_min; */
/* 	unsigned char csc_clip_uv_max; */
/* } tisp_csc_matrix_t; */

/* typedef struct { */
/* 	tisp_csc_cg_t csc_version; */
/* 	tisp_csc_matrix_t rgb2yuv; */
/* 	tisp_csc_matrix_t yuv2rgb; */
/* } tisp_csc_attr_t; */

/**
 * ccm-manual : 0 : attr-para not work
 *              1 : attr-para work
 * sat-en     : 0 : fix sat , sat-value = default 1.0
 *              1 : tool-para sat-adjust work
 * ccm        : 3x3 matrix 65536 = 1.0
 */
/* typedef struct tisp_ccm_attr{ */
/* 	char ccm_manual; */
/* 	char sat_en; */
/* 	int32_t ccm[9]; */
/* } tisp_ccm_attr_t; */

typedef enum {
	ISP_ANTIFLICKER_DISABLE_MODE,
	ISP_ANTIFLICKER_NORMAL_MODE,
	ISP_ANTIFLICKER_AUTO_MODE, // can reach min it
	ISP_ANTIFLICKER_BUTT,
} tisp_antiflicker_mode_t;

typedef struct {
	tisp_antiflicker_mode_t mode;
	uint8_t freq;
} tisp_anfiflicker_attr_t;

typedef enum {
	IMPISP_MASK_TYPE_RGB = 0,
	IMPISP_MASK_TYPE_YUV = 1,
} mask_value_type_t;

typedef struct {
	struct {
		unsigned char r_value;
		unsigned char g_value;
		unsigned char b_value;
	} argb;
	struct {
		unsigned char y_value;
		unsigned char u_value;
		unsigned char v_value;
	} ayuv;
} mask_value_t;

typedef struct {
	uint8_t chx;
	uint8_t pinum;
	uint8_t mask_en;
	uint16_t mask_pos_top;
	uint16_t mask_pos_left;
	uint16_t mask_width;
	uint16_t mask_height;
	mask_value_type_t color_type;
	mask_value_t mask_value;
} tisp_mask_block_attr_t;

/**
 * OSD picture atribution
 */
typedef struct {
	uint8_t pinum;
	uint8_t osd_enable;	 /**< osd enable */
	uint16_t osd_left;	 /**< osd area x value */
	uint16_t osd_top;	 /**< osd area y value */
	uint16_t osd_width;	 /**< osd area width */
	uint16_t osd_height; /**< osd area height */
	char *osd_image;	 /**< osd picture */
	uint16_t osd_stride; /**< osd stride */
} tisp_osd_block_attr_t;

/**
 * OSD picture type
 */
typedef enum {
	IMP_ISP_PIC_ARGB_8888, /**< ARGB8888 */
	IMP_ISP_PIC_ARGB_1555, /**< ARGB1555 */
	IMP_ISP_PIC_2_bit,	   /**< 2bit */
} tisp_pic_type_t;

/**
 * OSD type
 */
typedef enum {
	IMP_ISP_ARGB_TYPE_ARGB = 0,
	IMP_ISP_ARGB_TYPE_ARBG,
	IMP_ISP_ARGB_TYPE_AGRB,
	IMP_ISP_ARGB_TYPE_AGBR,
	IMP_ISP_ARGB_TYPE_ABRG,
	IMP_ISP_ARGB_TYPE_ABGR,

	IMP_ISP_ARGB_TYPE_RGBA = 8,
	IMP_ISP_ARGB_TYPE_RBGA,
	IMP_ISP_ARGB_TYPE_GRBA,
	IMP_ISP_ARGB_TYPE_GBRA,
	IMP_ISP_ARGB_TYPE_BRGA,
	IMP_ISP_ARGB_TYPE_BGRA,
} tisp_argb_type_t;

/**
 * OSD 2bit RGB value
 */
typedef struct {
	uint8_t osd_value_r[4];
	uint8_t osd_value_g[4];
	uint8_t osd_value_b[4];
	uint8_t osd_value_alpha[4];
} tisp_osd_2bit_value;

/**
 * OSD channel attribution
 */
typedef struct {
	tisp_pic_type_t osd_type;			   /**< OSD picture type */
	tisp_argb_type_t osd_argb_type;		   /**< OSD argb type */
	TISP_OPS_MODE osd_pixel_alpha_disable; /**< OSD pixel alpha disable function enable */
	tisp_osd_2bit_value osd_rgb_value;
	uint8_t osd_overlap;
	uint8_t osd_group;
} tisp_osd_attr_t;

/**
 * Draw window attribution
 */
typedef struct {
	uint8_t wind_enable;	 /**< draw window enable */
	uint16_t wind_x;		 /**< window start pixel in horizental */
	uint16_t wind_y;		 /**< window start pixel in vertical */
	uint16_t wind_w;		 /**< window width */
	uint16_t wind_h;		 /**< window height */
	mask_value_t wind_color; /**< window color */
	uint8_t wind_line_w;	 /**< window line width */
	uint8_t wind_alpha;		 /**< window alpha */
	uint8_t wind_msk_en;
	/* uint8_t  rang_msk_size; */
} tisp_draw_wind_attr_t;

/**
 * Draw range attribution
 */
typedef struct {
	uint8_t rang_enable;	 /**< draw range enable */
	uint16_t rang_x;		 /**< range start pixel in horizental */
	uint16_t rang_y;		 /**< range start pixel in vertical */
	uint16_t rang_w;		 /**< range width */
	uint16_t rang_h;		 /**< range height */
	mask_value_t rang_color; /**< range color */
	uint8_t rang_line_w;	 /**< range line width */
	uint8_t rang_alpha;		 /**< range alpha(3bit) */
	uint16_t rang_extend;	 /**< range extend */
	uint8_t rang_msk_en;
	/* uint8_t  rang_msk_size; */
} tisp_draw_range_attr_t;

/**
 * Draw line attribution
 */
typedef struct {
	uint8_t line_enable;	 /**< draw line enable */
	uint16_t line_startx;	 /**< line start pixel in horizental */
	uint16_t line_starty;	 /**< line start pixel in vertical */
	uint16_t line_endx;		 /**< line width */
	uint16_t line_endy;		 /**< line height */
	mask_value_t line_color; /**< line color */
	uint8_t line_width;		 /**< line line width */
	uint8_t line_alpha;		 /**< line alpha */
} tisp_draw_line_attr_t;

/**
 * Block line attribution
 */
typedef struct {
	uint8_t enable;
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
	mask_value_t color;
	uint8_t lineWidth;
	uint8_t alpha;
	uint8_t mskEn;
} tisp_draw_msk_attr_t;
/**
 * Draw type
 */
typedef enum {
	TISP_DRAW_WIND,	 /**< Draw window */
	TISP_DRAW_RANGE, /**< Draw range */
	TISP_DRAW_LINE,	 /**< Draw line */
	TISP_DRAW_BLOCK, /**< Draw line */
} tisp_draw_type_t;

/**
 * Draw unit Attribution
 */
typedef struct {
	uint8_t pinum;
	uint8_t msk_size;
	tisp_draw_type_t type; /**< draw type */
	mask_value_type_t color_type;
	union {
		tisp_draw_wind_attr_t wind;	 /**< draw window attr */
		tisp_draw_range_attr_t rang; /**< draw range attr */
		tisp_draw_line_attr_t line;	 /**< draw line attr */
		tisp_draw_msk_attr_t msk;	 /**< draw line attr */
	} draw_cfg;						 /**< draw attr */
} tisp_draw_block_attr_t;

/**
 * awb mode
 */
typedef enum {
	ISP_CORE_WB_MODE_AUTO = 0,		   /**< auto mode */
	ISP_CORE_WB_MODE_MANUAL,		   /**< manual mode */
	ISP_CORE_WB_MODE_DAY_LIGHT,		   /**< day light mode */
	ISP_CORE_WB_MODE_CLOUDY,		   /**< cloudy mode */
	ISP_CORE_WB_MODE_INCANDESCENT,	   /**< incandescent mode */
	ISP_CORE_WB_MODE_FLOURESCENT,	   /**< flourescent mode */
	ISP_CORE_WB_MODE_TWILIGHT,		   /**< twilight mode */
	ISP_CORE_WB_MODE_SHADE,			   /**< shade mode */
	ISP_CORE_WB_MODE_WARM_FLOURESCENT, /**< warm flourescent mode */
	ISP_CORE_WB_MODE_COLORTEND,		   /**< Color Trend Mode */
} TISPAWBMode;

/**
 * awb gain
 */
typedef struct {
	uint32_t rgain; /**< awb r-gain */
	uint32_t bgain; /**< awb b-gain */
} tisp_awb_gain_t;

/**
 * awb custom mode attribution
 */
typedef struct {
	TISP_OPS_MODE customEn; /**< awb custom enable */
	tisp_awb_gain_t gainH;	/**< awb gain on high ct */
	tisp_awb_gain_t gainM;	/**< awb gain on medium ct */
	tisp_awb_gain_t gainL;	/**< awb gain on low ct */
	uint32_t ct_node[4];	/**< awb custom mode nodes */
} tisp_awb_custom_attr_t;

/**
 * awb attribution
 */
typedef struct {
	TISPAWBMode mode;                     /**< awb mode */
	tisp_awb_gain_t gain_val;			/**< awb gain on manual mode */
	TISP_OPS_MODE frz;
	tisp_awb_custom_attr_t custom;         /**< awb custom attribution */
} tisp_awb_api_attr_t;

typedef struct isp_core_awb_global_statis {
	tisp_awb_gain_t statis_weight_gain; /**< awb statistics */
	tisp_awb_gain_t statis_gol_gain;	/**< awb global statistics */
} tisp_awb_global_statics_t;

/************ AE ioctl start ********************/
/**
 * AE scence mode
 */
typedef enum {
	TISP_AE_SCENCE_AUTO,		  /**< auto mode */
	TISP_AE_SCENCE_DISABLE,		  /**< diable mode */
	TISP_AE_SCENCE_ROI_ENABLE,	  /**< enable mode */
	TISP_AE_SCENCE_GLOBAL_ENABLE, /**< enable mode */
	TISP_AE_SCENCE_BUTT,		  /**< effect paramater, parameters have to be less than this value */
} TISPAEScenceMode;

typedef struct {
    uint32_t ae_hist[256];
    uint32_t ae_hist_5bin[5];
    uint8_t ae_hist_nodes[4];
    uint8_t ae_hist_hv[2];
} tisp_ae_sta_t;

/**
 * AE scence mode attr
 */
typedef struct {
	TISPAEScenceMode AeHLCEn;	  /**< AE high light depress enable */
	unsigned char AeHLCStrength;  /**< AE high light depress strength (0 ~ 10) */
	TISPAEScenceMode AeBLCEn;	  /**< AE back light compensation */
	unsigned char AeBLCStrength;  /**< AE back light compensation strength (0 ~ 10) */
	TISP_OPS_MODE AeTargetCompEn; /**< AE luma target compensation enable */
	uint32_t AeTargetComp;		  /**< AE luma target compensation strength（0 ~ 255）*/
	TISP_OPS_MODE AeStartEn;	  /**< AE start point enable */
	uint32_t AeStartEv;			  /**< AE start ev value */

	uint32_t luma;		  /**< AE luma value */
	uint32_t luma_scence; /**< AE luma value */
} tisp_ae_scence_attr_t;

/**
 * 获取AE特定历史信息
 */
typedef struct {
	uint32_t    start_adc;      /* AE 启动时ADC参数 */
	uint32_t    start_luma;     /* AE 启动起始luma值 */
	uint32_t    ev_frames;      /* 当前调用时刻前 frames 帧的EV信息，最大可获取连续64帧 */
	uint64_t    ev_value[64];   /* 连续帧EV的记录 */
} tisp_ae_history_info;

typedef enum {
	ISP_CORE_EXPR_UNIT_LINE, /**< The unit is integration line */
	ISP_CORE_EXPR_UNIT_US,	 /**< The unit is millisecond */
} TISPAEIntegrationTimeUnit;

/**
 * AE exposure info
 */
typedef struct {
	TISPAEIntegrationTimeUnit AeIntegrationTimeUnit;     /**< AE integration time unit */
	TISP_OPS_TYPE AeMode;				 /**< AE freezen enable */
	TISP_OPS_TYPE AeIntegrationTimeMode;		 /**< AE integration time manual */
	TISP_OPS_TYPE AeAGainManualMode;		 /**< AE sensor analog gain manual */
	TISP_OPS_TYPE AeDGainManualMode;		 /**< AE sensor digital gain manual */
	TISP_OPS_TYPE AeIspDGainManualMode;		 /**< AE ISP digital gain manual */
	uint32_t AeIntegrationTime;                            /**< AE integration time value */
	uint32_t AeAGain;                                      /**< AE sensor analog gain value */
	uint32_t AeDGain;                                      /**< AE sensor digital gain value */
	uint32_t AeIspDGain;                                   /**< AE ISP digital gain value */

	TISP_OPS_TYPE AeMinIntegrationTimeMode;		 /**< Reserve */
	TISP_OPS_TYPE AeMinAGainMode;			 /**< AE min analog gain enable */
	TISP_OPS_TYPE AeMinDgainMode;			 /**< Reserve */
	TISP_OPS_TYPE AeMinIspDGainMode;		 /**< Reserve */
	TISP_OPS_TYPE AeMaxIntegrationTimeMode;		 /**< AE max integration time enable */
	TISP_OPS_TYPE AeMaxAGainMode;			 /**< AE max sensor analog gain enable */
	TISP_OPS_TYPE AeMaxDgainMode;			 /**< AE max sensor digital gain enable */
	TISP_OPS_TYPE AeMaxIspDGainMode;		 /**< AE max isp digital gain enable */
	uint32_t AeMinIntegrationTime;                         /**< AE min integration time */
	uint32_t AeMinAGain;                                   /**< AE min sensor analog gain */
	uint32_t AeMinDgain;                                   /**< AE min sensor digital gain */
	uint32_t AeMinIspDGain;                                /**< AE min isp digital gain */
	uint32_t AeMaxIntegrationTime;                         /**< AE max integration time */
	uint32_t AeMaxAGain;                                   /**< AE max sensor analog gain */
	uint32_t AeMaxDgain;                                   /**< AE max sensor digital gain */
	uint32_t AeMaxIspDGain;                                /**< AE max isp digital gain */

	/* WDR模式下短帧的AE 手动模式属性*/
	TISP_OPS_TYPE AeShortMode;			  /**< AE freezen enable */
	TISP_OPS_TYPE AeShortIntegrationTimeMode;	  /**< AE integration time manual */
	TISP_OPS_TYPE AeShortAGainManualMode;		  /**< AE sensor analog gain manual */
	TISP_OPS_TYPE AeShortDGainManualMode;		  /**< AE sensor digital gain manual */
	TISP_OPS_TYPE AeShortIspDGainManualMode;	  /**< AE ISP digital gain manual */
	uint32_t AeShortIntegrationTime;                        /**< AE integration time value */
	uint32_t AeShortAGain;                                  /**< AE sensor analog gain value */
	uint32_t AeShortDGain;                                  /**< AE sensor digital gain value */
	uint32_t AeShortIspDGain;                               /**< AE ISP digital gain value */

	TISP_OPS_TYPE AeShortMinIntegrationTimeMode;	  /**< Reserve */
	TISP_OPS_TYPE AeShortMinAGainMode;		  /**< AE min analog gain enable */
	TISP_OPS_TYPE AeShortMinDgainMode;		  /**< Reserve */
	TISP_OPS_TYPE AeShortMinIspDGainMode;		  /**< Reserve */
	TISP_OPS_TYPE AeShortMaxIntegrationTimeMode;	  /**< AE max integration time enable */
	TISP_OPS_TYPE AeShortMaxAGainMode;		  /**< AE max sensor analog gain enable */
	TISP_OPS_TYPE AeShortMaxDgainMode;		  /**< AE max sensor digital gain enable */
	TISP_OPS_TYPE AeShortMaxIspDGainMode;		  /**< AE max isp digital gain enable */
	uint32_t AeShortMinIntegrationTime;                     /**< AE min integration time */
	uint32_t AeShortMinAGain;                               /**< AE min sensor analog gain */
	uint32_t AeShortMinDgain;                               /**< AE min sensor digital gain */
	uint32_t AeShortMinIspDGain;                            /**< AE min isp digital gain */
	uint32_t AeShortMaxIntegrationTime;                     /**< AE max integration time */
	uint32_t AeShortMaxAGain;                               /**< AE max sensor analog gain */
	uint32_t AeShortMaxDgain;                               /**< AE max sensor digital gain */
	uint32_t AeShortMaxIspDGain;                            /**< AE max isp digital gain */
} sdk_api_exprinfo_attr;

/**
 * AE exposure info
 */
typedef struct {
	TISPAEIntegrationTimeUnit AeIntegrationTimeUnit; /**< AE integration time unit */
	TISP_OPS_TYPE AeMode;							 /**< AE freezen enable */
	TISP_OPS_TYPE AeIntegrationTimeMode;			 /**< AE integration time manual */
	TISP_OPS_TYPE AeAGainManualMode;				 /**< AE sensor analog gain manual */
	TISP_OPS_TYPE AeDGainManualMode;				 /**< AE sensor digital gain manual */
	TISP_OPS_TYPE AeIspDGainManualMode;				 /**< AE ISP digital gain manual */
	uint32_t AeIntegrationTime;						 /**< AE integration time value */
	uint32_t AeAGain;								 /**< AE sensor analog gain value */
	uint32_t AeDGain;								 /**< AE sensor digital gain value */
	uint32_t AeIspDGain;							 /**< AE ISP digital gain value */

	TISP_OPS_TYPE AeMinIntegrationTimeMode; /**< Reserve */
	TISP_OPS_TYPE AeMinAGainMode;			/**< AE min analog gain enable */
	TISP_OPS_TYPE AeMinDgainMode;			/**< Reserve */
	TISP_OPS_TYPE AeMinIspDGainMode;		/**< Reserve */
	TISP_OPS_TYPE AeMaxIntegrationTimeMode; /**< AE max integration time enable */
	TISP_OPS_TYPE AeMaxAGainMode;			/**< AE max sensor analog gain enable */
	TISP_OPS_TYPE AeMaxDgainMode;			/**< AE max sensor digital gain enable */
	TISP_OPS_TYPE AeMaxIspDGainMode;		/**< AE max isp digital gain enable */
	uint32_t AeMinIntegrationTime;			/**< AE min integration time */
	uint32_t AeMinAGain;					/**< AE min sensor analog gain */
	uint32_t AeMinDgain;					/**< AE min sensor digital gain */
	uint32_t AeMinIspDGain;					/**< AE min isp digital gain */
	uint32_t AeMaxIntegrationTime;			/**< AE max integration time */
	uint32_t AeMaxAGain;					/**< AE max sensor analog gain */
	uint32_t AeMaxDgain;					/**< AE max sensor digital gain */
	uint32_t AeMaxIspDGain;					/**< AE max isp digital gain */

	/* WDR模式下短帧的AE 手动模式属性*/
	TISP_OPS_TYPE AeShortMode;				  /**< AE freezen enable */
	TISP_OPS_TYPE AeShortIntegrationTimeMode; /**< AE integration time manual */
	TISP_OPS_TYPE AeShortAGainManualMode;	  /**< AE sensor analog gain manual */
	TISP_OPS_TYPE AeShortDGainManualMode;	  /**< AE sensor digital gain manual */
	TISP_OPS_TYPE AeShortIspDGainManualMode;  /**< AE ISP digital gain manual */
	uint32_t AeShortIntegrationTime;		  /**< AE integration time value */
	uint32_t AeShortAGain;					  /**< AE sensor analog gain value */
	uint32_t AeShortDGain;					  /**< AE sensor digital gain value */
	uint32_t AeShortIspDGain;				  /**< AE ISP digital gain value */

	TISP_OPS_TYPE AeShortMinIntegrationTimeMode; /**< Reserve */
	TISP_OPS_TYPE AeShortMinAGainMode;			 /**< AE min analog gain enable */
	TISP_OPS_TYPE AeShortMinDgainMode;			 /**< Reserve */
	TISP_OPS_TYPE AeShortMinIspDGainMode;		 /**< Reserve */
	TISP_OPS_TYPE AeShortMaxIntegrationTimeMode; /**< AE max integration time enable */
	TISP_OPS_TYPE AeShortMaxAGainMode;			 /**< AE max sensor analog gain enable */
	TISP_OPS_TYPE AeShortMaxDgainMode;			 /**< AE max sensor digital gain enable */
	TISP_OPS_TYPE AeShortMaxIspDGainMode;		 /**< AE max isp digital gain enable */
	uint32_t AeShortMinIntegrationTime;			 /**< AE min integration time */
	uint32_t AeShortMinAGain;					 /**< AE min sensor analog gain */
	uint32_t AeShortMinDgain;					 /**< AE min sensor digital gain */
	uint32_t AeShortMinIspDGain;				 /**< AE min isp digital gain */
	uint32_t AeShortMaxIntegrationTime;			 /**< AE max integration time */
	uint32_t AeShortMaxAGain;					 /**< AE max sensor analog gain */
	uint32_t AeShortMaxDgain;					 /**< AE max sensor digital gain */
	uint32_t AeShortMaxIspDGain;				 /**< AE max isp digital gain */

	uint32_t TotalGainDb;	   /**< AE total gain, unit is dB */
	uint32_t TotalGainDbShort; /**< AE total gain, unit is dB */
	uint32_t ExposureValue;	   /**< AE exposure value(integration time x again x dgain) */
	uint32_t EVLog2;		   /**< AE exposure value cal by log */
} tisp_ae_exprinfo_t;

/**
 * AE statistics info
 */
typedef struct {
	unsigned short ae_hist_5bin[5]; /**< AE hist bin value [0 ~ 65535] */
	uint32_t ae_hist_256bin[256];	/**< AE hist bin value, is the true value of pixels num each bin */
	tisp_ae_zone_info_t ae_statis;	/**< AE statistics info */
} __attribute__((packed, aligned(1))) tisp_ae_statis_info_t;

/**
 * API: AE weight attribute
 */
typedef struct {
	TISP_OPS_MODE weight_enable;  /**< global weight set enable */
	tisp_ae_wgt_t ae_weight;             /**< global weight value (0 ~ 8)*/
} sdk_api_ae_weight_attr;

/**
 * AE weight attribute
 */
typedef struct {
	TISP_OPS_MODE roi_enable;	 /**< roi weight set enable */
	TISP_OPS_MODE weight_enable; /**< global weight set enable */
	tisp_ae_wgt_t ae_roi;		 /**< roi weight value (0 ~ 8) */
	tisp_ae_wgt_t ae_weight;	 /**< global weight value (0 ~ 8)*/
} tisp_ae_weight_t;

/**
 * statistics color domain
 */
typedef enum {
	IMP_ISP_HIST_ON_RAW, /**< Raw Domain */
	IMP_ISP_HIST_ON_YUV, /**< YUV Domain */
} TISPHistDomain;

/**
 * statistics hist area struct
 */
typedef struct {
	unsigned int start_h; /**< start pixel in the horizontal */
	unsigned int start_v; /**< start pixel in the vertical */
	unsigned char node_h; /**< the statistics node num in the horizontal */
	unsigned char node_v; /**< the statistics node num in the vertical */
} tisp_statis_location_t;

/**
 * AE statistics attribution
 */
typedef struct {
	TISP_OPS_MODE ae_statis_en;
	tisp_statis_location_t local; /**< AE statistics location */
	TISPHistDomain hist_domain;	  /**< AE Hist statistics color domain */
	unsigned char histThresh[4];  /**< AE Hist Thresh */
} tisp_ae_statis_attr_t;

typedef struct {
	TISP_OPS_MODE enable;
	unsigned int left;
	unsigned int top;
	unsigned int right;
	unsigned int bottom;
	unsigned int target;
} tisp_face_t;

typedef struct {
	TISPAEIntegrationTimeUnit AeIntegrationTimeUnit; /**< AE曝光时间单位 */
	uint32_t AeIntegrationTime;						 /**< AE手动模式下的曝光值 */
	uint32_t AeAGain;								 /**< AE Sensor 模拟增益值，单位是倍数 x 1024 */
	uint32_t AeDGain;								 /**< AE Sensor数字增益值，单位是倍数 x 1024 */
	uint32_t AeIspDGain;							 /**< AE ISP 数字增益值，单位倍数 x 1024*/

	uint32_t AeMinIntegrationTime; /**< AE最小曝光时间 */
	uint32_t AeMinAGain;		   /**< AE最小sensor模拟增益 */
	uint32_t AeMinDgain;		   /**< AE最小sensor数字增益 */
	uint32_t AeMinIspDGain;		   /**< AE最小ISP数字增益 */
	uint32_t AeMaxIntegrationTime; /**< AE最大曝光时间 */
	uint32_t AeMaxAGain;		   /**< AE最大sensor模拟增益 */
	uint32_t AeMaxDgain;		   /**< AE最大sensor数字增益 */
	uint32_t AeMaxIspDGain;		   /**< AE最大ISP数字增益 */

	/* WDR模式下短帧的AE 手动模式属性*/
	uint32_t AeShortIntegrationTime; /**< AE手动模式下的曝光值 */
	uint32_t AeShortAGain;			 /**< AE Sensor 模拟增益值，单位是倍数 x 1024 */
	uint32_t AeShortDGain;			 /**< AE Sensor数字增益值，单位是倍数 x 1024 */
	uint32_t AeShortIspDGain;		 /**< AE ISP 数字增益值，单位倍数 x 1024*/

	uint32_t AeShortMinIntegrationTime; /**< AE最小曝光时间 */
	uint32_t AeShortMinAGain;			/**< AE最小sensor模拟增益 */
	uint32_t AeShortMinDgain;			/**< AE最小sensor数字增益 */
	uint32_t AeShortMinIspDGain;		/**< AE最小ISP数字增益 */
	uint32_t AeShortMaxIntegrationTime; /**< AE最大曝光时间 */
	uint32_t AeShortMaxAGain;			/**< AE最大sensor模拟增益 */
	uint32_t AeShortMaxDgain;			/**< AE最大sensor数字增益 */
	uint32_t AeShortMaxIspDGain;		/**< AE最大ISP数字增益 */
	uint32_t fps;						/**< sensor 帧率 */
	tisp_ae_statis_attr_t AeStatis;
} tisp_ae_algo_init_t;

typedef struct {
	tisp_ae_statis_info_t ae_info;
	TISPAEIntegrationTimeUnit AeIntegrationTimeUnit; /**< AE曝光时间单位 */
	uint32_t AeIntegrationTime;						 /**< AE手动模式下的曝光值 */
	uint32_t AeAGain;								 /**< AE Sensor 模拟增益值，单位是倍数 x 1024 */
	uint32_t AeDGain;								 /**< AE Sensor数字增益值，单位是倍数 x 1024 */
	uint32_t AeIspDGain;							 /**< AE ISP 数字增益值，单位倍数 x 1024*/
	uint32_t AeShortIntegrationTime;				 /**< AE手动模式下的曝光值 */
	uint32_t AeShortAGain;							 /**< AE Sensor 模拟增益值，单位是倍数 x 1024 */
	uint32_t AeShortDGain;							 /**< AE Sensor数字增益值，单位是倍数 x 1024 */
	uint32_t AeShortIspDGain;						 /**< AE ISP 数字增益值，单位倍数 x 1024*/

	uint32_t Wdr_mode;
	struct isp_core_sensor_attr sensor_attr;
} __attribute__((packed, aligned(1))) tisp_ae_algo_info_t;

typedef struct {
	uint32_t change;
	TISPAEIntegrationTimeUnit AeIntegrationTimeUnit; /**< AE曝光时间单位 */
	uint32_t AeIntegrationTime;						 /**< AE手动模式下的曝光值 */
	uint32_t AeAGain;								 /**< AE Sensor 模拟增益值，单位是倍数 x 1024 */
	uint32_t AeDGain;								 /**< AE Sensor数字增益值，单位是倍数 x 1024 */

	uint32_t AeIspDGain;			 /**< AE ISP 数字增益值，单位倍数 x 1024*/
	uint32_t AeShortIntegrationTime; /**< AE手动模式下的曝光值 */
	uint32_t AeShortAGain;			 /**< AE Sensor 模拟增益值，单位是倍数 x 1024 */
	uint32_t AeShortDGain;			 /**< AE Sensor数字增益值，单位是倍数 x 1024 */
	uint32_t AeShortIspDGain;		 /**< AE ISP 数字增益值，单位倍数 x 1024*/

	uint32_t luma;		  /**< AE Luma值 */
	uint32_t luma_scence; /**< AE 场景Luma值 */
} tisp_ae_algo_attr_t;

typedef struct {
	uint32_t version;
	int vinum;
	tisp_ae_algo_init_t AeInitAttr;
} tisp_ae_algo_init_self_t;

typedef struct {
	uint32_t version;
	int vinum;
	tisp_ae_algo_info_t Aeinfo;
} __attribute__((packed, aligned(1))) tisp_ae_algo_info_self_t;

typedef struct {
	uint32_t version;
	int vinum;
	tisp_ae_algo_attr_t AeAttr;
} tisp_ae_algo_attr_self_t;

typedef enum {
	IMPISP_AE_NOTIFY_FPS_CHANGE,
} tisp_ae_algo_notify_t;

typedef struct {
	void *priv_data;
	int (*open)(void *priv_data, tisp_ae_algo_init_t AeInitAttr);
	void (*close)(void *priv_data);
	void (*handle)(void *priv_data, const tisp_ae_algo_info_t *AeInfo, tisp_ae_algo_attr_t *AeAttr);
	int (*notify)(void *priv_data, tisp_ae_algo_notify_t notify, void *data);
} tisp_ae_algo_func_t;

typedef struct {
	uint32_t stable;
	uint32_t target;
	uint32_t ae_mean;
} tisp_ae_stable_attr_t;

typedef struct {
	TISP_OPS_MODE en;
	uint32_t list[5 * 16];
} tisp_ae_explist_t;

typedef struct {
	uint32_t global_weight;
	uint32_t roi_weight;
	uint32_t smooth;
} tisp_face_ae_weiget;

typedef struct {
	uint32_t global_luma;
	uint32_t roi_luma;
} tisp_face_ae_luma;

typedef struct {
	TISP_OPS_MODE enable;
	uint64_t ev_li[15];
} tisp_ev_list;

typedef struct {
	TISP_OPS_MODE enable;
	uint64_t at_li[15];
} tisp_at_list;

typedef struct {
	int32_t flag;
} tisp_ae_flicker_flag;

typedef struct {
	uint32_t TotalGainDb;
	uint32_t TotalGainDbShort;
	uint64_t ExposureValue;
	uint32_t EVLog2;
	uint32_t luma;
	uint32_t luma_scence;
	int stable;
	uint32_t target;
	uint32_t ae_mean;
	uint32_t bv;
}sdk_api_ae_only_read_attr;

/**
 * ae target attribution
 */
typedef struct {
	TISP_OPS_MODE AeTargetCompEn;  /**< AE luma target compensation enable */
	uint32_t AeTargetComp;              /**< AE luma target compensation strength（0 ~ 255）*/
	tisp_at_list at;
	tisp_ev_list ev;
} sdk_api_ae_target_attr;

/**
 * Highlight Compensation or backlight compensation mode
 */
typedef enum {
	ISP_CORE_TUNING_HBLC_DISABLE,
	ISP_CORE_TUNING_HLC_ENABLE,
	ISP_CORE_TUNING_BLC_ENABLE,
} ISP_CORE_TUNING_HBLC_Mode;

/**
 * Highlight Compensation or backlight compensation attribution
 */
typedef struct {
	ISP_CORE_TUNING_HBLC_Mode mode;
	uint8_t strength;
} sdk_api_hlc_or_blc_attr;


/************ AE ioctl end ********************/

/**
 * AF statistics info
 */
typedef struct {
	tisp_af_zone_info_t Af_Stat[225];
} tisp_af_statis_info_t;

/**
 * AWB statistics value attribution
 */
typedef enum {
	IMP_ISP_AWB_ORIGIN,	 /**< Original value */
	IMP_ISP_AWB_LIMITED, /**< Limited statistics value */
} TISPAWBStatisMode;

/**
 * AWB statistics attribution
 */
typedef struct {
	TISP_OPS_MODE awb_statis_en;
	tisp_statis_location_t local; /**< AWB Statistic area */
	TISPAWBStatisMode mode;		  /**< AWB Statistic mode */
} tisp_awb_statis_attr_t;

/**
 * AF statistics attribution
 */
typedef struct {
	TISP_OPS_MODE af_statis_en;
	tisp_statis_location_t local;	/**< AF statistics area */
	unsigned char af_metrics_shift; /**< Metrics scaling factor 0x0 is default*/
	unsigned short af_delta;		/**< AF statistics low pass fliter weight [0 ~ 64]*/
	unsigned short af_theta;		/**< AF statistics high pass fliter weight [0 ~ 64]*/
	unsigned short af_hilight_th;	/**< AF high light threshold [0 ~ 255]*/
	unsigned short af_alpha_alt;	/**< AF statistics H and V direction weight [0 ~ 64]*/
	unsigned short af_belta_alt;	/**< AF statistics H and V direction weight [0 ~ 64]*/
} tisp_af_statis_attr_t;

/**
 * Statistics info attribution
 */
typedef struct {
	tisp_ae_statis_attr_t ae;	/**< AE statistics info attr */
	tisp_awb_statis_attr_t awb; /**< AWB statistics info attr */
	tisp_af_statis_attr_t af;	/**< AF statistics info attr */
} tisp_statis_config_t;

typedef enum {
	IMP_ISP_MODULE_SINTER = 0, /**< 2D降噪下标 */
	IMP_ISP_MODULE_TEMPER,	   /**< 3D降噪下标 */
	IMP_ISP_MODULE_DRC,		   /**< 数字宽动态下标 */
	IMP_ISP_MODULE_DPC,		   /**< 动态去坏点下标 */
	IMP_ISP_MODULE_DEFOG,	   /**< 去雾模块的强度下标 */
	IMP_ISP_MODULE_BUTT,	   /**< 用于判断参数有效性的值，必须大于此值 */
} IMPISPModuleRatioArrayList;

typedef struct {
	TISP_OPS_MODE en; /**< module ratio enable */
	uint8_t ratio;	  /**< module ratio value. The default value is 128, more than 128 (increase strength), and less than 128 (decrease strength) */
} tisp_ratio_unit_t;

/**
 * ISP module ratio Attrbution
 */
typedef struct {
	tisp_ratio_unit_t ratio_attr[16]; /**< module ratio attr */
} tisp_module_ratio_t;

typedef struct {
	uint32_t af_metrics;	 /**< AF Main metrics */
	uint32_t af_metrics_alt; /**< AF second metrics */
	uint8_t af_frame_num;	 /**< AF frame num */
} tisp_af_metric_info_t;

typedef enum {
	TISP_WDR_OUTPUT_MODE_FUS_12BIT_FRAME,
	TISP_WDR_OUTPUT_MODE_LONG_FRAME,
	TISP_WDR_OUTPUT_MODE_SHORT_FRAME,
	TISP_WDR_OUTPUT_MODE_FUS_16BIT_FRAME,
	TISP_WDR_OUTPUT_MODE_BUTT,
} tisp_wdr_output_mode_t;

typedef struct {
	tisp_awb_statis_attr_t AwbStatis;
} tisp_awb_algo_init_t;

typedef struct {
	uint32_t cur_r_gain;
	uint32_t cur_b_gain;
	uint32_t r_gain_statis;
	uint32_t b_gain_statis;
	uint32_t r_gain_wei_statis;
	uint32_t b_gain_wei_statis;
	tisp_wb_statis_info_t awb_statis;
} __attribute__((packed, aligned(1))) tisp_awb_algo_info_t;

typedef struct {
	uint32_t change;
	uint32_t r_gain;
	uint32_t b_gain;
	uint32_t ct;
} tisp_awb_algo_attr_t;

typedef enum {
	IMPISP_AWB_NOTIFY_MODE_CHANGE,
} tisp_awb_algo_notify_t;

typedef struct {
	uint32_t version;
	int vinum;
	tisp_awb_algo_init_t AwbInitAttr;
} tisp_awb_algo_init_self_t;

typedef struct {
	uint32_t version;
	int vinum;
	tisp_awb_algo_info_t Awbinfo;
} __attribute__((packed, aligned(1))) tisp_awb_algo_info_self_t;

typedef struct {
	uint32_t version;
	int vinum;
	tisp_awb_algo_attr_t AwbAttr;
} tisp_awb_algo_attr_self_t;

typedef struct {
	void *priv_data;
	int (*open)(void *priv_data, tisp_awb_algo_init_self_t *AwbInitAttr);
	void (*close)(void *priv_data);
	void (*handle)(void *priv_data, const tisp_awb_algo_info_self_t *AwbInfo, tisp_awb_algo_attr_self_t *AwbAttr);
	int (*notify)(void *priv_data, tisp_awb_algo_notify_t notify, void *data);
} tisp_awb_algo_func_t;

typedef struct {
	char version[8];
	char rbpath[64];
	uint32_t dstatus;
	uint32_t nstatus;
} tisp_show_bin;

typedef struct {
	unsigned char saturation[TISPNUM];
	unsigned char contrast[TISPNUM];
	unsigned char sharpness[TISPNUM];
	unsigned char brightness[TISPNUM];
	unsigned char hue[TISPNUM];
	tisp_ae_exprinfo_t ae_info[TISPNUM];
	tisp_ae_scence_attr_t ae_scence[TISPNUM];
	tisp_module_ratio_t module_ratio[TISPNUM];
	tisp_statis_config_t stattis_config[TISPNUM];
	tisp_awb_api_attr_t awb_attr[TISPNUM];
	tisp_ae_weight_t ae_weight[TISPNUM];
	tisp_awb_custom_attr_t awb_custom[TISPNUM];
	tisp_anfiflicker_attr_t flicker_attr[TISPNUM];
	tisp_show_bin swbin[TISPNUM];
} tisp_core_tuning_attr;

typedef struct {
	TISP_OPS_MODE enable;
	uint8_t binId;
	char bname[128];
} tisp_bin_t;

typedef struct {
	TISP_OPS_MODE enable;
	uint8_t lsize;
	uint32_t fmark;
} tisp_frame_drop_t;

typedef struct {
	uint8_t vinum;
	tisp_frame_drop_t fdrop[3];
} tisp_frame_drop_attr_t;

typedef struct {
	uint8_t vinum;
	uint8_t snapraw_mode;
	uint8_t snapnum;
	uint8_t wdrRawMode;
} tisp_raw_attr_t;

typedef struct {
	uint8_t vinum;
	uint8_t channel;
	uint32_t savepaddr;
	uint8_t snapnum;
	uint32_t imagesize;
} tisp_nv12_attr_t;

typedef enum {
	IMPISP_SCALER_FITTING_CRUVE,
	IMPISP_SCALER_FIXED_WEIGHT,
	IMPISP_SCALER_BUTT,
} TISP_SCALER_MODE_E;

typedef struct tisp_scaler_opt_control {
	unsigned char chx;
	TISP_SCALER_MODE_E mode;
	unsigned char level;
} tisp_scaler_opt_attr_t;

typedef struct tisp_msca_crop_attr {
	uint8_t chx;
	uint8_t crop_enable;
	uint16_t crop_x;
	uint16_t crop_y;
	uint16_t crop_w;
	uint16_t crop_h;
} tisp_msca_crop_attr_t;

/* peter add start */
/* ---------- BLC ---------- */
typedef struct {
	uint16_t api_work;
	uint16_t black_level_r;
	uint16_t black_level_gr;
	uint16_t black_level_gb;
	uint16_t black_level_b;
	uint16_t black_level_ir;
} tisp_api_blc_attr_t;

/* ---------- CSC ---------- */
typedef enum {
	ISP_CSC_CG_BT601_FULL,
	ISP_CSC_CG_BT601_LIMITED,
	ISP_CSC_CG_BT709_FULL,
	ISP_CSC_CG_BT709_LIMITED,
	ISP_CSC_CG_BT2020_FULL,
	ISP_CSC_CG_BT2020_LIMITED,
	ISP_CSC_CG_USER,
	ISP_CSC_CG_BUTT,
} tisp_csc_cg_t;

typedef struct {
	int32_t csc_matrix[9];
	uint8_t csc_offset_y;
	uint8_t csc_offset_uv;
	uint8_t csc_clip_y_min;
	uint8_t csc_clip_y_max;
	uint8_t csc_clip_uv_min;
	uint8_t csc_clip_uv_max;
} tisp_csc_matrix_t;

typedef struct {
	tisp_csc_cg_t csc_version;
	tisp_csc_matrix_t rgb2yuv;
	tisp_csc_matrix_t yuv2rgb;
} tisp_csc_attr_t;

/* ---------- Gamma ---------- */
typedef enum {
	ISP_GAMMA_TYPE_RGB = 0,
	ISP_GAMMA_TYPE_IR = 1,
} tisp_gamma_rgbir_type_t;

typedef enum {
	IMP_ISP_GAMMA_CURVE_DEFAULT,
	IMP_ISP_GAMMA_CURVE_SRGB,
	IMP_ISP_GAMMA_CURVE_REC709,
	IMP_ISP_GAMMA_CURVE_HDR,
	IMP_ISP_GAMMA_CURVE_USER,
	IMP_ISP_GAMMA_CURVE_BUTT,
} tisp_gamma_type_t;

typedef struct tisp_gamma_lut {
	tisp_gamma_type_t type;
	unsigned short gamma[129];
} tisp_gamma_api_attr_t;

/* ---------- CCM ---------- */
typedef struct tisp_ccm_attr {
	int8_t ccm_manual;
	int8_t sat_en;
	int32_t ccm[9];
} tisp_ccm_attr_t;

/* ---------- CSCCR ---------- */
typedef enum {
	IMPISP_TUNING_CSCCR_DEFAULT_MODE,
	IMPISP_TUNING_CSCCR_STRETCH_MODE,
	IMPISP_TUNING_CSCCR_COMPRESS_MODE,
	IMPISP_TUNING_CSCCR_CUSTOMER_MODE,
} IMPISPCsccrMode;

typedef struct {
	uint8_t input_y_min;
	uint8_t input_y_max;
	uint8_t output_y_min;
	uint8_t output_y_max;
	uint8_t input_uv_min;
	uint8_t input_uv_max;
	uint8_t output_uv_min;
	uint8_t output_uv_max;
} IMPISPCsccrValue;

typedef struct {
	IMPISPCsccrMode csccr_mode;
	IMPISPCsccrValue parameters;
} tisp_csccr_mode_t;

/* ---------- HLDC ---------- */
typedef struct {
	int16_t strength;
	int16_t width;
	int16_t height;
	int16_t center_w;
	int16_t center_h;
} tisp_hldc_attr_t;

/* ---------- GSM ---------- */
/* typedef struct { */
/*         uint32_t hist[256]; */
/*         uint32_t hist_5bin[5]; */
/*         uint8_t  hist_nodes[4]; */
/*         uint8_t  hist_hv[2]; */
/* } tisp_gsm_attr_t; */

/* peter add end */

typedef struct {
	uint8_t chx : 2;
	uint8_t y_mirr_en : 1;
	uint8_t c_mirr_en : 1;
	uint8_t y_flip_en : 1;
	uint8_t c_flip_en : 1;
	uint8_t y_pass_en : 1; /**< ivdc y mirr select */
	uint8_t c_pass_en : 1; /**< ivdc uv mirr select */
} tisp_mirr_flip_t;

/**** select [7:0]*****/
/* 32：BLC(L) */
/* 33: BLC(S) */
/* 34: LSC(L) */
/* 35: LSC(S) */
/* 36: AWBG0(L) */
/* 37: AWBG0(S) */
/* 38: DPC(L) */
/* 39: DPC(S） */
/* 40: WDR */
/* 41: TMO */
/* 42: GIB */
/* 43: AWBG1  */
/* 44: ADR */
/* LINERA:DPC->TMO->GIB->LSC->AWB1->ADR->DMSC */
/* LINERA:GIB->LSC->AWB0->DPC->TMO->ADR->DMSC */
/* WDR:BLC->LSC->AWB0->DPC->WDR->TMO->ADR->DMSC */
typedef enum { TISP_RAW_MODULE_BLC = 32, TISP_RAW_MODULE_LSC = 34, TISP_RAW_MODULE_AWBG0 = 36, TISP_RAW_MODULE_DPC = 38, TISP_RAW_MODULE_WDR = 40, TISP_RAW_MODULE_TMP = 41, TISP_RAW_MODULE_GIB = 42, TISP_RAW_MODULE_AWB1 = 43, TISP_RAW_MODULE_ADR = 44, TISP_RAW_MODULE_BUTT } tisp_raw_module_type;

/**** data type [17:16]*****/
/* 0: RAW8 */
/* 1: RAW16 */
/* 2: RAW32 */
typedef enum { TISP_RAW_DTYPE_RAW8, TISP_RAW_DTYPE_RAW16, TISP_RAW_DTYPE_BUTT } tisp_raw_data_type;

typedef enum { TISP_RAW_OPS_DISABLE, TISP_RAW_OPS_ENABLE, TISP_RAW_MODE_BUTT } tisp_raw_ops_mode;

typedef enum { TISP_RAW_IO_OPS_DISABLE, TISP_RAW_IO_OPS_ENABLE, TISP_RAW_IO_OPS_BUTT } tisp_raw_io_ops_mode;

typedef struct {
	uint32_t paddr;
	tisp_raw_io_ops_mode mode;
	tisp_raw_data_type dtype;
	tisp_raw_module_type mtype;
} tisp_raw_io_attr_t;

typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t wdr_en;

	tisp_raw_io_attr_t iattr;
	tisp_raw_io_attr_t oattr;
} tisp_raw_io_t;

typedef struct {
	tisp_raw_ops_mode smode;
	tisp_raw_io_t io;
} tisp_raw_row_attr_t;

typedef struct {
	tisp_raw_ops_mode smode;

	tisp_raw_io_attr_t iattr;
	tisp_raw_io_attr_t oattr;
} tisp_raw_row_control_t;

typedef struct {
	tisp_raw_ops_mode mode;

	tisp_raw_io_t io;
	uint32_t paddr[4];
} tisp_raw_rw_attr_t;

typedef struct {
	tisp_raw_ops_mode smode;

	tisp_raw_data_type dtype;
	tisp_raw_module_type mtype;

	uint32_t paddr[4];
} tisp_raw_rw_control_t;

typedef struct {
	unsigned short offset_r;
	unsigned short offset_g;
	unsigned short offset_b;
} tisp_bcsh_offset_rgb_attr_t;

typedef struct {
	uint8_t tmo_roinum;
	uint16_t tmo_xcent[6];
	uint16_t tmo_ycent[6];
	uint16_t tmo_distep[6];
	uint16_t tmo_uplumthr[6];
	uint8_t tmo_upk[6];
} tisp_tmo_faceae;

typedef struct {
	uint8_t transfer_flag;
	uint8_t id;
	uint16_t value[201];
} tisp_tmo_curve;

typedef struct {
	TISP_OPS_MODE enable;
	uint8_t flag;
} tisp_aov_stream_t;

typedef struct {
	uint16_t ct_trend_offset_thres[2];
	uint32_t ct_trend_offset_grgb[6];
} tisp_awb_ct_trend_offset;

/**
 * 多摄时，摄像头效果同步描述结构体。
 * 注意：全局模式暂时不支持
 */
typedef struct {
	bool    sync_effect_enable; /* 效果同步是否使能；如果不使能摄像头图像效果各自调整，如果使能
                                摄像头图像效果根据下面描述进行联动调整 */
	bool    master_mode;        /* 效果同步主从模式是否使能，主从模式表示所有从摄效果参考主摄；
	                           如果不使能,表示采用全局模式，所有参与同步的摄像头效果根据所有图像整体统计值信息进行统一调整 */

	uint32_t master_vinum;         /* 主从模式下，主摄的编号; 全局模式下，该参数无效 */
	uint32_t sensors_map[TISPNUM][TISPNUM];  /* 表示摄像头相对位置，采用左上对齐使用，无效的位置填写0xff */
} tisp_sync_effect;

int tisp_enable_tuning(void);
void tisp_disable_tuning(void);
tisp_core_tuning_attr *tisp_get_tuning(void);

/* ******************* TISP TUNNING TOOlS VERIFY ********************* */
typedef enum {
	TISP_API_TOP_INFO = 0,
	TISP_API_DN_MODE,
	TISP_API_MSCA_MASK,
	TISP_API_MSCA_LINE,
	TISP_API_MSCA_OSD,
	TISP_API_MSCA_MIRR,
	TISP_API_MSCA_FLIP,
	TISP_API_MSCA_FCROP,
	TISP_API_MSCA_CROP,
	TISP_API_MSCA_AUTOZOOM,
	TISP_API_BRIGHTNESS,
	TISP_API_CONTRAST,
	TISP_API_SATURATION,
	TISP_API_SHARPNESS,
	TISP_API_BCSHHUE,
	TISP_API_MDNSRATIO,
	TISP_API_SDNSRATIO,
	TISP_API_BACK0,
	TISP_API_BACK1,
	TISP_API_MAX_NUM,
} tisp_api_name;

typedef struct tisp_api_ctrl_cfg {
	tisp_api_name api_name;
	uint32_t api_ctrl;
	uint32_t api_backup;
	bool api_status[TISP_API_MAX_NUM];
} tisp_api_ctrl_cfg_t;

/* lib api */
int lib_tx_isp_get_fps(int vinum, uint32_t *fps);
int lib_tisp_api_get_dn_status(int vinum);
int lib_tisp_csc_get_full_range(int vinum);
int32_t tisp_msca_api_set_frame_drop(int vinum, int chx, tisp_frame_drop_t *tfd);
int32_t tisp_msca_api_get_frame_drop(int vinum, int chx, tisp_frame_drop_t *tfd);

#endif /* __TIZIANO_CORE_CTRL_H__ */
