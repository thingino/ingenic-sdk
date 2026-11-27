/*
 * Header: lib_base_sensor.h
 * Role  : sensor 接口定义。
 * Scope : sensor 注册信息、sensor 属性、控制/数据总线、增益/曝光回调、video input 状态、WDR/HVFLIP/PM 参数和 sensor clock/GPIO/MCLK 声明。
 * Notes : PRJ009 sensor 驱动只包含此头文件。
 */

#ifndef __LIB_BASE_SENSOR_H__
#define __LIB_BASE_SENSOR_H__

#include "lib_base.h"
#include <linux/kernel.h>
#include <linux/list.h>
#include "tx-isp-device.h"
#include "txx-funcs.h"
#include "tx-isp-videodev.h"

#define I2C_NAME_SIZE 20
#define SPI_NAME_SIZE 32

#define SENSOR_R_BLACK_LEVEL 0
#define SENSOR_GR_BLACK_LEVEL 1
#define SENSOR_GB_BLACK_LEVEL 2
#define SENSOR_B_BLACK_LEVEL 3

typedef struct _sensor_context_t {
	int sensor_id;
	unsigned short again;
	unsigned short dgain;
	unsigned short again_x2;
	unsigned short dgain_coarse;
	unsigned short dgain_fine;
	unsigned short again_short;
	unsigned short it;
	unsigned short it_short;
	unsigned short dgain_short;
	unsigned char n_context;
	unsigned char wdr_mode;
	unsigned char column_buffer_gain_index;
	unsigned int i2c_data;
	void (*i2c_callback)(int vinum, unsigned int i2c_data);

} sensor_context_t;

typedef struct _image_resolution_t {
	unsigned short width;
	unsigned short height;
} image_resolution_t;

typedef struct tisp_sensor_param {
	image_resolution_t total;
    image_resolution_t active;
    sensor_context_t sensor_ctx;
    int32_t  max_again;
    int32_t  max_dgain;
    int32_t  min_again;
    int32_t  min_dgain;
    int32_t  xoffset;
    int32_t  yoffset;
    int32_t  anti_flicker_pos;
    uint32_t fps;
    uint32_t min_fps;
    uint32_t max_fps;
    uint32_t min_integration_time;
    uint32_t max_integration_time;
    uint32_t max_integration_time_long;
    uint32_t integration_time_limit;
    uint32_t lines_per_second;
    uint32_t min_integration_time_short;
    uint32_t max_integration_time_short;
    uint32_t max_again_short;
    uint32_t max_dgain_short;
    uint32_t min_again_short;
    uint32_t min_dgain_short;

    int32_t  again;
    uint32_t integration_time;
    uint32_t hcg_ratio;	/* 1024表示一倍 */
    uint32_t hcg_short_ratio;

    uint16_t day_light_integration_time_max;
    uint8_t  integration_time_apply_delay;
    uint8_t  analog_gain_apply_delay;
    uint8_t  digital_gain_apply_delay;
    uint8_t  analog_gain_short_apply_delay;
    uint8_t  digital_gain_short_apply_delay;
    uint8_t  integration_time_short_apply_delay;
    uint8_t  mode;
	uint8_t  aov_delay;
	uint8_t  hcg_short_enable;
	uint8_t  hcg_apply_delay;
	uint8_t  hcg_enable;
	uint8_t  hcg_short_apply_delay;

} tisp_sensor_param_t;

enum tx_sensor_control_bus_type {
	TX_SENSOR_CONTROL_INTERFACE_I2C = 1,
	TX_SENSOR_CONTROL_INTERFACE_SPI,
};

struct tx_isp_i2c_board_info {
	char type[I2C_NAME_SIZE];
	int addr;
	//      struct i2c_board_info board_info;
	int i2c_adapter_id;
};

struct tx_isp_spi_board_info {
	char modalias[SPI_NAME_SIZE];
	int bus_num;
	//      struct spi_board_info board_info;
};

#define TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS       1
#define TX_ISP_SENSOR_FULL_RES_MAX_FPS          2

typedef enum {
	ISP_CLK_960P_MODE = 60000000,
	ISP_CLK_1080P_MODE = 90000000,
	ISP_CLK_3M_MODE = 100000000,
} isp_clk_mode;

typedef enum {
	TX_SENSOR_MAX_FPS_120 = 120,
	TX_SENSOR_MAX_FPS_110 = 110,
	TX_SENSOR_MAX_FPS_100 = 100,
	TX_SENSOR_MAX_FPS_90 = 90,
	TX_SENSOR_MAX_FPS_80 = 80,
	TX_SENSOR_MAX_FPS_70 = 70,
	TX_SENSOR_MAX_FPS_60 = 60,
	TX_SENSOR_MAX_FPS_50 = 50,
	TX_SENSOR_MAX_FPS_45 = 45,
	TX_SENSOR_MAX_FPS_40 = 30,
	TX_SENSOR_MAX_FPS_30 = 30,
	TX_SENSOR_MAX_FPS_25 = 25,
	TX_SENSOR_MAX_FPS_20 = 20,
	TX_SENSOR_MAX_FPS_15 = 15,
	TX_SENSOR_MAX_FPS_12 = 12,
	TX_SENSOR_MAX_FPS_10 = 10,
	TX_SENSOR_MAX_FPS_5 = 5,
} sensor_max_fps_mode;

typedef enum {
	TX_SENSOR_RES_30 = 30,
	TX_SENSOR_RES_100 = 100,
	TX_SENSOR_RES_130 = 130,
	TX_SENSOR_RES_200 = 200,
	TX_SENSOR_RES_300 = 300,
	TX_SENSOR_RES_400 = 400,
	TX_SENSOR_RES_500 = 500,
	TX_SENSOR_RES_800 = 800,
} sensor_resolution_type;

typedef enum {
	TISP_SENSOR_VI_MIPI_CSI0 = 0,
	TISP_SENSOR_VI_MIPI_CSI1 = 1,
	TISP_SENSOR_VI_DVP = 2,
	TISP_SENSOR_VI_BUTT = 3,
} vin_type_t;

typedef enum {
	TISP_SENSOR_MCLK0 = 0,
	TISP_SENSOR_MCLK1 = 1,
	TISP_SENSOR_MCLK2 = 2,
	TISP_SENSOR_MCLK_BUTT = 3,
} vin_mclk_t;

struct tx_isp_sensor_register_info {
	char name[32];
	enum tx_sensor_control_bus_type cbus_type;
	union {
		struct tx_isp_i2c_board_info i2c;
		struct tx_isp_spi_board_info spi;
	};
	int rst_gpio;
	int pwdn_gpio;
	int power_gpio;
	int switch_gpio;
	int switch_gpio_state;
	unsigned short sensor_id;
	vin_type_t video_interface;
	vin_mclk_t mclk;
	int default_boot;
	struct {
		unsigned int num;
		unsigned int den;
	} fps;
};

struct tx_isp_sensor_item {
	unsigned int value;
	unsigned int min;
	unsigned int max;
	unsigned short apply_delay;
};

enum tx_sensor_data_bus_type {
	TX_SENSOR_DATA_INTERFACE_MIPI = 1,
	TX_SENSOR_DATA_INTERFACE_DVP,
	TX_SENSOR_DATA_INTERFACE_BT601,
	TX_SENSOR_DATA_INTERFACE_BT656,
	TX_SENSOR_DATA_INTERFACE_BT1120,
};

typedef struct tx_isp_sensor_ctrl {
	/* isp_gain mean that the value is output of ISP-FW,it is not a gain multiplier unit.
	 *  gain_mutiplier = (2^(isp_gain/(2^LOG_GAIN_SHIFT))).
	 *  the fuction will convert gain_mutiplier to sensor_Xgain.
	 *  the return value is isp_gain of sensor_Xgain's inverse conversion.
	 */
	unsigned int (*alloc_again)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again);
	unsigned int (*alloc_again_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again);
	unsigned int (*alloc_dgain)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain);
	unsigned int (*alloc_dgain_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain);
	unsigned int (*alloc_integration_time)(unsigned int it, unsigned char shift, unsigned int *sensor_it);
	unsigned int (*alloc_integration_time_short)(unsigned int it_short, unsigned char shift, unsigned int *sensor_it_short);

	unsigned int (*alloc_hcg)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg);
	unsigned int (*alloc_hcg_short)(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg);
} TX_ISP_SENSOR_CTRL;

enum mipi_sensor_data_type_value {
	YUV422_8BIT = 0x1e,
	RAW8 = 0x2a,
	RAW10 = 0x2b,
	RAW12 = 0x2c,
};

enum tx_sensor_frm_mode {
	TX_SENSOR_DEFAULT_FRAME_MODE = 0,
	TX_SENSOR_WDR_2_FRAME_MODE,
	TX_SENSOR_WDR_3_FRAME_MODE,
	TX_SENSOR_WDR_4_FRAME_MODE,
};

enum tx_sensor_mode {
	TX_SENSOR_DEFAULT_MODE = 0,
	TX_SENSOR_NOT_VC_MODE,
	TX_SENSOR_VC_MODE,
};

enum tx_sensor_csi_fmt {
	TX_SENSOR_RAW8 = 0,
	TX_SENSOR_RAW10,
	TX_SENSOR_RAW12,
	TX_SENSOR_YUV422 = 7,
};

struct vic_mipi_sensor_ctl {
	unsigned int hcrop_diff_en;
	unsigned int mipi_vcomp_en;
	unsigned int mipi_hcomp_en;
	unsigned short mipi_crop_start0x;
	unsigned short mipi_crop_start0y;
	unsigned short mipi_crop_start1x;
	unsigned short mipi_crop_start1y;
	unsigned short mipi_crop_start2x;
	unsigned short mipi_crop_start2y;
	unsigned short mipi_crop_start3x;
	unsigned short mipi_crop_start3y;
	unsigned int line_sync_mode;
	unsigned int work_start_flag;
	unsigned int data_type_en;
	enum mipi_sensor_data_type_value data_type_value; // 0x1E 2A 2B 2C
	unsigned int del_start;
	unsigned int sensor_fid_mode;
	enum tx_sensor_frm_mode sensor_frame_mode;
	enum tx_sensor_mode sensor_mode;
	enum tx_sensor_csi_fmt sensor_csi_fmt;
};

typedef enum {
	SENSOR_MIPI_OTHER_MODE,
	SENSOR_MIPI_SONY_MODE,
} sensor_mipi_mode;

struct vic_mipi_vcrop_del {
	unsigned int del0;
	unsigned int del1;
	unsigned int del2;
	unsigned int del3;
};

struct tx_isp_mipi_bus {
	sensor_mipi_mode mode;
	unsigned int clk;
	unsigned int clk_nocontinue;
	unsigned int data;
	unsigned char index;
	unsigned char lans;
	unsigned int def2wdr;
	unsigned int image_twidth;
	unsigned int image_theight;
	unsigned int front_invlid_pix;
	unsigned int after_invlid_pix;
	unsigned int settle_time_apative_en;
	struct vic_mipi_sensor_ctl mipi_sc;
	struct vic_mipi_vcrop_del mipi_vcropdel;
};

typedef enum {
	DVP_PA_LOW_10BIT,
	DVP_PA_HIGH_10BIT,
	DVP_PA_12BIT,
	DVP_PA_LOW_8BIT,
	DVP_PA_HIGH_8BIT,
} sensor_dvp_gpio_mode;

typedef enum {
	SENSOR_DVP_HREF_MODE,
	SENSOR_DVP_HSYNC_MODE,
	SENSOR_DVP_SONY_MODE,
} sensor_dvp_timing_mode;

typedef struct {
	unsigned short vblanking;
	unsigned short hblanking;
} sensor_dvp_blanking;

typedef struct {
	unsigned char hsync_polar;
	unsigned char vsync_polar;
	unsigned char pclk_polar; /**< reserved */
} sensor_dvp_polar;

struct tx_isp_dvp_bus {
	sensor_dvp_gpio_mode gpio;
	sensor_dvp_timing_mode mode;
	sensor_dvp_blanking blanking;
	sensor_dvp_polar polar;
	unsigned int dvp_hcomp_en;
};

typedef enum {
	SENSOR_BT_8BIT,
	SENSOR_BT_16BIT,
} sensor_bt_mode;

struct tx_isp_bt1120_bus {
	sensor_bt_mode gpio;
};

struct tx_isp_bt656_bus {
	sensor_bt_mode gpio;
};

struct tx_isp_bt601_bus {
	sensor_bt_mode gpio;
};

struct tx_isp_sensor_hcg {
	unsigned int base_gain;
	unsigned short apply_delay;
};

enum tx_sensor_data_type {
	TX_SENSOR_DATA_TYPE_LINEAR = 0,
	TX_SENSOR_DATA_TYPE_WDR_FS,
	TX_SENSOR_DATA_TYPE_WDR_DOL,	/**< Clear HDR / DOL HDR */
	TX_SENSOR_DATA_TYPE_WDR_NATIVE, /**< NATIVE / DAG */
};

enum tx_sensor_expo_triggers { TX_SENSOR_EXPO_FRAME_DONE, TX_SENSOR_EXPO_FRAME_START };

enum tx_isp_sensor_fsync_mode {
	TX_ISP_SENSOR_FSYNC_MODE_CLOSE = 0,
	TX_ISP_SENSOR_FSYNC_MODE_MS_SINGLE,
	TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME,
	TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE,
	TX_ISP_SENSOR_FSYNC_MODE_SS_SINGLE_PWM,
	TX_ISP_SENSOR_FSYNC_MODE_SS_SINGLE_PWM_MISPLACE,
	TX_ISP_SENSOR_FSYNC_MODE_SS_DUAL_PWM,
	TX_ISP_SENSOR_FSYNC_MODE_SS_DUAL_PWM_MISPLACE,
};

struct tx_isp_sensor_fsync_pwm {
	unsigned int mgpio;	 /**< master gpio */
	unsigned int sgpio;	 /**< slave gpio */
	unsigned int freq;	 /**< frequency (unit:Hz) */
	unsigned int dratio; /**< duty ratio (range:[0, 100]) */
	unsigned int polar;	 /**< polarity (range:[0, 1])*/
	unsigned int offset; /**< Phase offset in dual pwm mode */
	void *priv;
};

struct tx_isp_sensor_fsync_attr {
	enum tx_isp_sensor_fsync_mode mode;

	int call_times;		 /**< The number of function loop calls. */
	unsigned int sdelay; /**< Switch delay time. (unit:ns) */

	struct tx_isp_sensor_fsync_pwm pwm;

	void *priv;
};

struct tx_isp_sensor_dag_attr {
	int enable;
	unsigned short curve_break[12];
};

struct tx_isp_sensor_attribute {
	const char *name;
	unsigned int chip_id;
	enum tx_sensor_control_bus_type cbus_type;
	unsigned int cbus_mask;
	unsigned int cbus_device;
	enum tx_sensor_data_bus_type dbus_type;
	union {
		struct tx_isp_mipi_bus mipi;
		struct tx_isp_dvp_bus dvp;
		struct tx_isp_bt1120_bus bt1120;
		struct tx_isp_bt656_bus bt656bus;
		struct tx_isp_bt601_bus bt601bus;
		char string[64];
	};
	enum tx_sensor_data_type data_type;

	struct tx_isp_sensor_item again;
	struct tx_isp_sensor_item again_short;
	struct tx_isp_sensor_item dgain;
	struct tx_isp_sensor_item dgain_short;
	struct tx_isp_sensor_item integration_time;
	struct tx_isp_sensor_item integration_time_short;

	/* Read-only */
	unsigned int expo;
	unsigned int expo_short;

	enum tx_sensor_expo_triggers expo_fs;

	struct tx_isp_sensor_hcg hcg;
	struct tx_isp_sensor_hcg hcg_short;

	unsigned short total_width;
	unsigned short total_height;

	/* Read-only for Sensor drivers (format: us_den | (us_num << 16)) */
	unsigned int one_line_expr_in_us;

	TX_ISP_SENSOR_CTRL sensor_ctrl;

	unsigned int wdr_cache; // the format is .16

	struct tx_isp_sensor_fsync_attr fsync_attr;

	struct tx_isp_sensor_dag_attr dag_attr;

	unsigned int priv_size;
	void *priv; /* point to struct tx_isp_sensor_board_info */
};

struct tisp_mbus_framefmt {
	unsigned int width;
	unsigned int height;
	unsigned int code;
	unsigned int field;
	unsigned int colorspace;

	unsigned short ycbcr_enc;
	unsigned short quantization;
	unsigned short xfer_func;
	unsigned short reserved[11];
};

struct tx_isp_sensor_subdev_id {
	int grp;
	int vic;
};

struct tx_isp_video_in {
	struct tisp_mbus_framefmt mbus;
	unsigned int mbus_change;
	struct tx_isp_sensor_attribute *attr;
	struct tx_isp_sensor_register_info *info;
	unsigned int vi_max_width;	// the max width of sensor output setting
	unsigned int vi_max_height; // the max height of sensor output setting
	struct tx_isp_sensor_item fps;
	struct tx_isp_sensor_subdev_id sd_id;
	int hvflip_mode;
	unsigned int wdr;
	int state;
};

enum tx_isp_hvflip_mode {
	TX_ISP_SENSOR_HVFLIP_NOMAL,
	TX_ISP_SENSOR_HVFLIP_HFLIP,
	TX_ISP_SENSOR_HVFLIP_VFLIP,
	TX_ISP_SENSOR_HVFLIP_HVFLIP,
};

struct tx_isp_hvflip_par {
	enum tx_isp_hvflip_mode hvflip;
	int drop_frame;
	int reset;
};

typedef struct tx_isp_sensor {
	struct tx_isp_subdev sd;
	int index;
	unsigned int type;
	int fsync_flag;
	struct list_head list;
	struct tx_isp_sensor_register_info info;
	struct tx_isp_sensor_attribute attr;
	struct tx_isp_video_in video;
	struct device *dev;
	struct clk *mclk;
	void *priv;
} tx_isp_sensor_t;

#define sd_to_sensor_device(_ep) container_of(_ep, struct tx_isp_sensor, sd)

struct tx_isp_sensor_win_setting {
	int width;
	int height;
	int fps;
	enum input_mbus_fmt mbus_code;
	enum tisp_colorspace colorspace;
	void *regs;
};

struct tx_isp_wdr_par {
	int boot;
	int wdr_en;
	int exp_ratio;
};

typedef struct tx_isp_sensor_user_data {
	int dir;
	int cmd;
	int value;
} IMPISPUserCtl;

typedef enum {
	TX_SENSOR_PM_PREPARE = 0,
	TX_SENSOR_PM_SUSPEND,
	TX_SENSOR_PM_RESUME,
	TX_SENSOR_PM_COMPLETE,
	TX_SENSOR_PM_STREAMON,
	TX_SENSOR_PM_STREAMOFF,
} sensor_s_stream_type;

struct tx_isp_internal_arg {
	int vinum;
	int value;
};

struct tx_isp_sensor_fast_attr {
	char name[16];
	int i2c_addr;
	int width;
	int height;
	int wdr;
	int (*init_sensor)(void);
	void (*exit_sensor)(void);
};

typedef struct _sensor_control_t
{
    int32_t (*alloc_analog_gain)(int32_t gain, sensor_context_t *p_ctx);
    int32_t (*alloc_analog_gain_short)(int32_t gain, sensor_context_t *p_ctx);
    int32_t (*alloc_digital_gain)(int32_t gain, sensor_context_t *p_ctx);
    int32_t (*alloc_digital_gain_short)(int32_t gain, sensor_context_t *p_ctx);
    uint32_t (*alloc_integration_time)(uint32_t int_time, sensor_context_t *p_ctx);
    uint32_t (*fps_control)(uint8_t fps, tisp_sensor_param_t *param);
    uint32_t (*alloc_integration_time_short)(uint32_t int_time, sensor_context_t *p_ctx);
    uint32_t (*get_lines_per_second)(tisp_sensor_param_t *param);

    uint16_t (*get_normal_fps)(tisp_sensor_param_t *param);
    uint16_t (*read_black_pedestal)(int i, uint32_t gain);
    uint16_t (*get_id)(sensor_context_t *p_ctx);

    void (*set_integration_time)(uint16_t int_time, sensor_context_t *p_ctx);
    void (*set_integration_time_short)(uint16_t int_time, sensor_context_t *p_ctx);
    void (*start_changes)(sensor_context_t *p_ctx);
    void (*end_changes)(sensor_context_t *p_ctx);
    void (*set_analog_gain)(uint32_t again_reg_val, sensor_context_t *p_ctx);
    void (*set_analog_gain_short)(uint32_t again_reg_val, sensor_context_t *p_ctx);
    void (*set_digital_gain)(uint32_t dgain_reg_val, sensor_context_t *p_ctx);
    void (*set_digital_gain_short)(uint32_t dgain_reg_val, sensor_context_t *p_ctx);
    void (*set_mode)( uint8_t mode, tisp_sensor_param_t *param);
    void (*set_wdr_mode)(uint8_t wdr, tisp_sensor_param_t *param);
    void (*disable_isp)(sensor_context_t *p_ctx);
    void (*set_fps)(uint32_t fps, sensor_context_t *p_ctx);
    void (*hw_reset_disable)(void);
    void (*hw_reset_enable)(void);
    int32_t (*get_register)(uint32_t *reg, uint32_t *value, sensor_context_t *p_ctx);
    int32_t (*set_register)(uint32_t *reg, uint32_t *value, sensor_context_t *p_ctx);
} sensor_control_t;

typedef struct tx_sensor_attr_t {
    tisp_sensor_param_t sensor_param;
    sensor_control_t sensor_ctrl;
} sensor_attr_t;

enum tx_isp_i2c_index {
	TX_ISP_I2C_SET_EXPO,
	TX_ISP_I2C_SET_EXPO_SHORT,
	TX_ISP_I2C_SET_AGAIN,
	TX_ISP_I2C_SET_AGAIN_SHORT,
	TX_ISP_I2C_SET_DGAIN,
	TX_ISP_I2C_SET_INTEGRATION,
	TX_ISP_I2C_SET_INTEGRATION_SHORT,
	TX_ISP_I2C_SET_HVFLIP,
	TX_ISP_I2C_SET_DGAIN_SHORT,
	TX_ISP_I2C_SET_FPS,
	TX_ISP_I2C_SET_STREAMON,
	TX_ISP_I2C_SET_STREAMOFF,
	TX_ISP_I2C_SET_BUTTON,
};

struct tx_isp_i2c_msg {
	unsigned int flag;
	unsigned int value;
	unsigned int i2c_data;
	void (*i2c_callback)(int vinum, unsigned int i2c_data);
};

int set_sensor_gpio_function(int func_set);
int set_sensor_mclk_function(int mclk_set);

#endif /* __LIB_BASE_SENSOR_H__ */
