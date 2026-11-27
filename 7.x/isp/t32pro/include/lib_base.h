#ifndef __LIB_BASE_H__
#define __LIB_BASE_H__

/*
 * Header: lib_base.h
 * Role  : firmware 对外基础接口头文件（基础类型 + 对外共用结构体 + 核心接口）
 * Scope : 仅放稳定基础定义；避免放平台适配细节与通用工具实现声明。
 * Notes : 公共错误码/事件/通用宏优先放到 tx_isp_common.h。
 */

#define TX_ISP_FIRMWARE_VERSION "H20260803b"

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef int int32_t;
typedef short int16_t;
// typedef char int8_t;
//

#define SENSOR_NUM 4
#define ISP_SUCCESS 0

#define LOG2_GAIN_SHIFT 16
#define TX_ISP_GAIN_FIXED_POINT 16
/******************* tiziano ****************************/
typedef enum {
	TISP_VIC_IRQ = 0,
	TISP_LDC_IRQ,
	TISP_ISP_IRQ,
	TISP_IVDC_IRQ,
	TISP_MAX_IRQ,
} tisp_irq_t;

typedef enum {
	IMPISP_TOTAL_ONE = 1,
	IMPISP_TOTAL_TWO,
	IMPISP_TOTAL_THR,
	IMPISP_TOTAL_FOUR,
	IMPISP_TOTAL_BUTT,
} sensor_num_t;

typedef enum {
	IMPISP_MULTISENSOR_DIRECT_MODE,
	IMPISP_MULTISENSOR_CACHED_MODE,
	IMPISP_MULTISENSOR_MODE_BUTT,
} multi_sensor_mode_t;

typedef enum {
	IMPISP_NOT_JOINT = 0,

	IMPISP_DOUBLE_ABOVE_UNDER,
	IMPISP_DOUBLE_LEFT_RIGHT,

	IMPISP_MAIN_JOINT_BUTT,
} multi_sensor_split_joint_mode;

typedef struct {
	multi_sensor_split_joint_mode mode;
	unsigned char joint_position[12];
} multi_sensor_split_joint;

typedef struct {
	multi_sensor_mode_t mode;
	unsigned int cache_line;
} multi_input;

typedef struct {
	sensor_num_t sensor_num;
	multi_input input;
	multi_sensor_split_joint joint[6];
} multisensor_mode_t;

struct tx_isp_cmd_argdata {
	union {
		int value;
		int enable;
	};
	int vinum;
};

/************************** tuning **********************/
typedef enum {
	IMPISP_FLIP_NORMAL_MODE = 0,
	IMPISP_FLIP_ISP_H_MODE,
	IMPISP_FLIP_ISP_V_MODE,
	IMPISP_FLIP_ISP_HV_MODE,
	IMPISP_FLIP_MODE_BUTT,
} ISP_CORE_HVFLIP;

typedef enum tisp_mode_day_and_night {
	TISP_RUNING_MODE_DAY_MODE,
	TISP_RUNING_MODE_NIGHT_MODE,
	TISP_RUNING_MODE_CUSTOM_MODE,
	TISP_RUNING_MODE_BUTT,
} TISP_MODE_DN_E;

struct image_tuning_ctrls {

	/* Enable - vertically flip */
	unsigned int vflip;
	/* Enable - horizontally flip */
	unsigned int hflip;

	/* Enable Wide Dynamic Range module */
	unsigned int wdr;

	/* used and inited */
	unsigned char contrast;
	unsigned char saturation;
	unsigned char brightness;
	unsigned char sharpness;
	unsigned char hue;

	/* sensor output fps */
	unsigned int fps;
	ISP_CORE_HVFLIP hvflip;

	/* The mode of isp day and night */
	TISP_MODE_DN_E daynight;
};

/************************** core **********************/
struct tx_isp_version {
	char cpu[32];
	char version[64];
};

typedef struct tx_isp_fw_module {
	unsigned int id;
} tx_isp_fw_module_t;

typedef enum tx_isp_ops_type {
	TISP_OPS_SDK = 0,
	TISP_OPS_KERNEL,
} tx_isp_ops_type_t;

typedef enum tx_isp_mode {
	TISP_MODE_NORMAL = 0,
	TISP_MODE_ONESHOT,
} tx_isp_mode_t;

typedef enum tx_isp_module_state_info {
	TX_ISP_MODULE_INFO_UNDEFINE = 0,
	TX_ISP_MODULE_INFO_SLAKE, /* probe */
	TX_ISP_MODULE_INFO_DEINIT = TX_ISP_MODULE_INFO_SLAKE,
	TX_ISP_MODULE_INFO_ACTIVE,
	TX_ISP_MODULE_INFO_INIT = TX_ISP_MODULE_INFO_ACTIVE,
	TX_ISP_MODULE_INFO_IDLE,
	TX_ISP_MODULE_INFO_RESET,
	TX_ISP_MODULE_INFO_BUSY,
} tx_isp_module_state_info_t;

#define IOCTL_TYPE_SENSOR_OPS (0x5 << 24)

enum tx_isp_sensor_ioctl {
	/* the events of sensor are defined as follows. */
	TX_ISP_IOCTL_SENSOR_REGISTER = IOCTL_TYPE_SENSOR_OPS,
	TX_ISP_IOCTL_SENSOR_RELEASE,
	TX_ISP_IOCTL_SENSOR_ENUM_INPUT,
	TX_ISP_IOCTL_SENSOR_GET_INPUT,
	TX_ISP_IOCTL_SENSOR_SET_INPUT,
	TX_ISP_IOCTL_SENSOR_INT_TIME,
	TX_ISP_IOCTL_SENSOR_INT_TIME_SHORT,
	TX_ISP_IOCTL_SENSOR_AGAIN,
	TX_ISP_IOCTL_SENSOR_AGAIN_SHORT,
	TX_ISP_IOCTL_SENSOR_DGAIN,
	TX_ISP_IOCTL_SENSOR_FPS,
	TX_ISP_IOCTL_SENSOR_BLACK_LEVEL,
	TX_ISP_IOCTL_SENSOR_WDR,
	TX_ISP_IOCTL_SENSOR_RESIZE,
	TX_ISP_IOCTL_SENSOR_PREPARE_CHANGE,
	TX_ISP_IOCTL_SENSOR_FINISH_CHANGE,
	TX_ISP_IOCTL_SENSOR_HVFLIP,
	TX_ISP_IOCTL_SENSOR_S_REGISTER,
	TX_ISP_IOCTL_SENSOR_G_REGISTER,
	TX_ISP_IOCTL_SENSOR_WDR_STOP,
	TX_ISP_IOCTL_SENSOR_WDR_OPEN,
	TX_ISP_IOCTL_SENSOR_START,
	TX_ISP_IOCTL_SENSOR_DONE,
	TX_ISP_IOCTL_SENSOR_EXPO,
	TX_ISP_IOCTL_SENSOR_EXPO_SHORT,
	TX_ISP_IOCTL_SENSOR_HCG,
	TX_ISP_IOCTL_SENSOR_USER_DEF, /*aov define*/
	TX_ISP_IOCTL_PM_S_STREAM_CTRL, /*aov define*/
	TX_ISP_IOCTL_MULTI_MODE,
	TX_ISP_IOCTL_BYPASS_MODE,
	TX_ISP_IOCTL_SENSOR_DGAIN_SHORT,
	TX_ISP_IOCTL_FRAME_SYNC_MODE,
	TX_ISP_IOCTL_SENSOR_STREAM,

};

typedef struct tx_isp_fw_device {

	tx_isp_ops_type_t ops_type;
	multisensor_mode_t multi_mode;
	tx_isp_module_state_info_t moduleState;

	/* pad */

	void *isp_dev; /* 存上层的device结构体*/

} tx_isp_fw_device_t;

struct tx_isp_fw_mem {
	unsigned int vaddr;
	unsigned int paddr;
	unsigned int size;
};

typedef struct tx_isp_fw_workqueue {
	void *data;
} tx_isp_fw_workqueue_t;

typedef struct {
	void *data;
} tx_isp_fw_spinlock_t;

typedef struct tx_isp_fw_mutex {
	void *data;
} tx_isp_fw_mutex_t;

typedef struct tx_isp_fw_completion {
	void *data;
} tx_isp_fw_completion_t;

typedef struct tx_isp_fw_semaphore {
	void *data;
} tx_isp_fw_semaphore_t;

typedef struct tx_isp_fw_wait_queue_head {
	void *data;
} tx_isp_fw_wait_queue_head_t;

typedef struct tx_isp_fw_timespec {
	long ts_sec;
	long ts_nsec;
} tx_isp_fw_timespec_t;

void *tx_isp_fw_device_probe(void *ispDev);
void tx_isp_fw_device_remove(tx_isp_fw_device_t *coreDev);

int lib_tx_isp_driver_version(unsigned long arg);

int lib_tisp_defaule_bin_info(void *arg);

#ifdef __KERNEL__
#include <linux/interrupt.h>
typedef irqreturn_t tx_isp_irqreturn_t;
#else
typedef int tx_isp_irqreturn_t;
#endif

tx_isp_irqreturn_t tx_isp_irq_handle(uint32_t this_irq, void *dev);
tx_isp_irqreturn_t tx_isp_irq_thread_handle(uint32_t this_irq, void *dev);
void system_reg_write(unsigned int reg, unsigned int value);
unsigned int system_reg_read(unsigned int reg);
void system_reg_csi_write(unsigned int reg, unsigned int value);
unsigned int system_reg_csi_read(unsigned int reg);
void system_reg_csi_phy_write(unsigned int reg, unsigned int value);
unsigned int system_reg_csi_phy_read(unsigned int reg);
void system_reg_ivdc_write(unsigned int reg, unsigned int value);
unsigned int system_reg_ivdc_read(unsigned int reg);

int lib_tx_isp_sensor_enum_input(int ops_type, unsigned long arg);
int lib_tx_isp_sensor_set_input(int ops_type, unsigned long arg);
int lib_tx_isp_set_tiziano_par(int ops_type, unsigned long arg);
typedef struct tx_isp_riscv_info {
	int32_t vinum;
	unsigned long riscv_info_addr;
} tx_isp_riscv_info_t;

int lib_tx_isp_set_riscv_info(int ops_type, unsigned long arg);
typedef struct tx_isp_zeratul_debug_info {
	int32_t vinum;
	uint8_t zeratul_debug;
	uint32_t debug_frame_count;
} tx_isp_zeratul_debug_info_t;

int lib_tx_isp_set_zeratul_debug_attr(int ops_type, unsigned long arg);
void lib_tx_isp_fw_device_sync_opstype(tx_isp_ops_type_t ops);
void lib_tx_isp_fw_device_get_opstype(tx_isp_ops_type_t *ops);
void lib_tx_isp_fs_sync_opstype(tx_isp_ops_type_t ops);

/************************** device **********************/
struct tx_isp_subdev;
struct tx_isp_chip_ident;
struct tx_isp_dbg_register;
struct tx_isp_dbg_register_list;
struct tisp_format;

struct tx_isp_dbg_register {
	char name[32];
	int sensor_id;
	int type;
	unsigned int size;
	unsigned long long reg;
	unsigned long long val;
};

struct tx_isp_dbg_register_list {
	char name[32];
	int sensor_id;
	int type;
	unsigned int size;
	unsigned long long reg;
	unsigned long long val;
};

struct tx_isp_chip_ident {
	char name[32];
	char *revision;
	unsigned int ident;
};

struct tx_isp_module_ops {
	int (*init)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*deinit)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*stream_on)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*stream_off)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*reset)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*restart)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*prepare)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*suspend)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*resume_early)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*resume)(unsigned char vinum, tx_isp_mode_t mode, void *data);
	int (*complete)(unsigned char vinum, tx_isp_mode_t mode, void *data);
};

typedef struct tx_isp_module {
	char name[32];
	tx_isp_ops_type_t ops_type;
	tx_isp_mode_t mode;
	tx_isp_module_state_info_t moduleState;
	struct tx_isp_module_ops priv_ops;
	void (*sync_opstype)(tx_isp_ops_type_t ops);
	void (*debug)(unsigned char vinum, void *data);
	int (*notify)(unsigned char vinum, tx_isp_mode_t mode, unsigned int notification, void *data);
} tx_isp_module_t;

#define tx_isp_set_module_mode(module, mod) \
	if (module) {                           \
		(module)->mode = (mod);             \
	}
#define tx_isp_get_module_mode(module) ((module) ? (module)->mode : TISP_MODE_NORMAL)

#define tx_isp_set_module_state(module, state) \
	if (module) {                              \
		(module)->moduleState = (state);       \
	}
#define tx_isp_get_module_state(module) ((module) ? (module)->moduleState : TX_ISP_MODULE_INFO_UNDEFINE)

typedef struct tx_isp_subdev_pad {
	struct tx_isp_subdev *sd; /* Subdev this pad belongs to */
	unsigned char index; /* Pad index in the entity pads array */
	unsigned char type; /* Pad type (TX_ISP_PADTYPE_*) */
	unsigned char links_type; /* Pad link type (TX_ISP_PADLINK_*) */
	unsigned char state; /* Pad state (TX_ISP_PADSTATE_*) */
	struct tx_isp_subdev_pad *remote;
	int (*event)(struct tx_isp_subdev_pad *, unsigned int event, void *data);

	void *priv;
} tx_isp_subdev_pad_t;

typedef struct tx_isp_subdev {
	tx_isp_module_t module;
	struct tx_isp_chip_ident chip;

	/* expanded members */
	unsigned short num_outpads; /* Number of sink pads */
	unsigned short num_inpads; /* Number of source pads */

	struct tx_isp_subdev_pad *outpads; /* OutPads array (num_pads elements) */
	struct tx_isp_subdev_pad *inpads; /* InPads array (num_pads elements) */

	int (*ioctl)(struct tx_isp_subdev *sd, unsigned int ioctl, void *arg);

	void *isp_fw_dev;
	void *dev_priv;
	void *host_priv;

} tx_isp_subdev_t;

#define tx_isp_set_sd_mode(sd, mod) \
	if (sd) {                       \
		(sd)->module.mode = (mod);  \
	}
#define tx_isp_get_sd_mode(sd) ((sd) ? (sd)->module.mode : TISP_MODE_NORMAL)

#define tx_isp_set_sd_state(sd, state)      \
	if (sd) {                               \
		(sd)->module.moduleState = (state); \
	}
#define tx_isp_get_sd_state(sd) ((sd) ? (sd)->module.moduleState : TX_ISP_MODULE_INFO_UNDEFINE)
#define tx_isp_add_sd_state(sd, state)       \
	if (sd) {                                \
		(sd)->module.moduleState += (state); \
	}
#define tx_isp_sub_sd_state(sd, state)       \
	if (sd) {                                \
		(sd)->module.moduleState -= (state); \
	}

static inline void tx_isp_set_subdevdata(struct tx_isp_subdev *sd, void *data)
{
	sd->dev_priv = data;
}

static inline void *tx_isp_get_subdevdata(struct tx_isp_subdev *sd)
{
	return sd->dev_priv;
}

static inline void tx_isp_set_subdev_hostdata(struct tx_isp_subdev *sd, void *data)
{
	sd->host_priv = data;
}

static inline void *tx_isp_get_subdev_hostdata(struct tx_isp_subdev *sd)
{
	return sd->host_priv;
}

typedef enum {
	TISP_LINEAR_MODE = 0,
	TISP_WDR_MODE,
} tisp_wdr_mode_t;

typedef enum {
	TISP_DN_DAY = 0, /* 白天 */
	TISP_DN_NIGHT, /* 黑夜 */
} tisp_running_mode_t;

typedef struct tisp_switch_bin {
	unsigned int paddr;
	unsigned int size;
	tisp_wdr_mode_t wdr_mode;
	tisp_running_mode_t dn_mode;
	unsigned char updateCsccr;
	unsigned char vinum;
} tisp_switch_bin_t;

typedef enum {
	TISP_BIN_MODE_DEFAULT = 0,
	TISP_BIN_MODE_SELPATH,
	TISP_BIN_MODE_ADDR,
} tisp_bin_mode_t;

#define TISP_MAX_CACHE_BIN 6
typedef struct {
	tisp_bin_mode_t binMode;
	uint32_t vaddr[TISP_MAX_CACHE_BIN];
	uint32_t paddr[TISP_MAX_CACHE_BIN];
	uint32_t binSize[TISP_MAX_CACHE_BIN];
	uint8_t binNum;
	uint8_t binId;
	uint8_t binStatic[TISP_MAX_CACHE_BIN];
	uint8_t binPath[TISP_MAX_CACHE_BIN][128];
} tisp_bin_attr_t;

typedef struct tisp_bin_info {
	tisp_bin_attr_t binAttr;
	tisp_wdr_mode_t wdr_mode;
	tisp_running_mode_t dn_mode;
	uint8_t zeratulOpen;
	uint8_t optMode;
	uint8_t vinum;
	char version[8];
	char soc[8];
} tisp_bin_info_t;

/**
 * lib_tisp_set_tiziano_par - Set Tiziano ISP parameters for bin switching
 * @ops_type: Operation type (1 = internal/kernel call, 0 = user space call)
 * @arg: Pointer to tisp_switch_bin_t structure containing:
 *       - paddr: Physical address of the parameter buffer
 *       - size: Size of the parameter buffer
 *       - wdr_mode: WDR mode (TISP_LINEAR_MODE or TISP_WDR_MODE)
 *       - dn_mode: Day/night mode (TISP_DN_DAY or TISP_DN_NIGHT)
 *       - updateCsccr: Flag to update CSCCR parameters
 *       - vinum: Video input channel number
 *
 * Return: 0 on success, negative error code on failure
 *         -EFAULT if copy from user space failed
 */
int lib_tisp_set_tiziano_par(int ops_type, unsigned long arg);

struct isp_buf_info {
	unsigned int vinum;
	unsigned int paddr;
	unsigned int size;
};

enum oneshot_control_value {
	TISP_ONESHOT_DISABLE = 0,
	TISP_ONESHOT_ENABLE,
	TISP_ONESHOT_TICK,
	TISP_ONESHOT_TICK_NOBLOCK,
	TISP_ONESHOT_CONTROL_END,
};

int lib_tisp_get_isp_rmem_info(int ops_type, unsigned long arg);
int lib_tx_isp_set_multi_mode(int ops_type, unsigned long arg);

int lib_tx_isp_sensor_register_sensor(int ops_type, unsigned long arg);
int lib_tx_isp_sensor_release_sensor(int ops_type, unsigned long arg);
int lib_tisp_set_isp_rmem_info(int ops_type, unsigned long arg);
int lib_tx_isp_sensor_get_input(struct tx_isp_module *module, unsigned long arg);
int lib_tx_isp_video_s_stream(struct tx_isp_module *module, unsigned long arg);
int lib_tx_isp_pm_prepare(unsigned char vinum, tx_isp_mode_t mode, void *data);
int lib_tx_isp_pm_suspend(unsigned char vinum, tx_isp_mode_t mode, void *data);
int lib_tx_isp_pm_resume_early(unsigned char vinum, tx_isp_mode_t mode, void *data);
int lib_tx_isp_pm_resume(unsigned char vinum, tx_isp_mode_t mode, void *data);
int lib_tx_isp_pm_complete(unsigned char vinum, tx_isp_mode_t mode, void *data);
int lib_tx_isp_pm_stream_on(void *data);
int lib_tx_isp_wait_pm_all_done(int ops_type, unsigned long arg);
int lib_tx_isp_oneshot_control(enum oneshot_control_value ops);
int lib_tx_isp_set_frame_drop(int ops_type, unsigned long arg);
int lib_tx_isp_get_frame_drop(int ops_type, unsigned long arg);
int lib_tx_isp_set_frame_drop_once(int ops_type, unsigned long arg);
int lib_tx_isp_get_frame_drop_once(int ops_type, unsigned long arg);

int lib_tx_isp_code_debug_info(char *buf, unsigned int buf_size);
int lib_tx_isp_vin_debug_info(char *buf, unsigned int buf_size);
int lib_tx_isp_csi_debug_info(char *buf, unsigned int buf_size);
int lib_tx_isp_vic_debug_info(char *buf, unsigned int buf_size);
int lib_tx_isp_ldc_debug_info(char *buf, unsigned int buf_size);
int lib_tx_isp_msca_debug_info(char *buf, unsigned int buf_size);
int lib_tx_isp_tuning_debug_info(char *buf, unsigned int buf_size);

/* ldc */
enum ldc_vi_num {
	LDC_VI_MAIN = 0,
	LDC_VI_SEC = 1,
	LDC_VI_THR = 2,
	LDC_VI_FOUR = 3,
	LDC_VI_BUTT,
};

enum ldc_func_mode {
	LDC_ISP_I2D_MODE = 1,
	LDC_ISP_HLDC_MODE = 2,
	LDC_ISP_LDC_MODE = 4,
	LDC_ISP_LDC_FUNC_BUTT,
};

enum ldc_lut_blocksize {
	LDC_LUT_BLOCKSIZE_8x8 = 8,
	LDC_LUT_BLOCKSIZE_16x16 = 16,
	LDC_LUT_BLOCKSIZE_32x32 = 32,
	LDC_LUT_BLOCKSIZE_64x64 = 64,
	LDC_LUT_BLOCKSIZE_BUTT,
};

enum ldc_isp_i2d_angle {
	LDC_ISP_I2D_0 = 0,
	LDC_ISP_I2D_90,
	LDC_ISP_I2D_180,
	LDC_ISP_I2D_270,
	LDC_ISP_I2D_MIRROR,
	LDC_ISP_I2D_FLIP,
	LDC_ISP_I2D_BUTT,
};

enum ldc_isp_ops_mode {
	LDC_OPS_MODE_DISABLE = 0,
	LDC_OPS_MODE_ENABLE,
	LDC_OPS_MODE_BUTT,
};

struct ldc_online_init {
	enum ldc_vi_num vinum;
	enum ldc_func_mode func;
	enum ldc_lut_blocksize size;
	enum ldc_isp_ops_mode state;
	enum ldc_isp_i2d_angle angle;
	uint16_t width;
	uint16_t height;
	uint16_t lut_mode;
	uint16_t lut_single_flag;
	uint32_t lut_addr;
	uint32_t lut_offset;
	uint32_t lut_size;
	uint32_t lut_buf0;
	uint32_t lut_buf1;
};

struct ldc_online_lut_attr {
	enum ldc_vi_num vinum;
	uint32_t lut_addr;
	uint32_t lut_offset;
};

enum tx_isp_ldc_mode {
	LDC_ONLINE_MODE = 0,
	LDC_OFFLINE_MODE,
	LDC_MODE_BUTT,
};

enum tx_isp_ldc_online_state {
	LDC_STATE_CLOSE = 0,
	LDC_STATE_OPEN,
};

struct tx_isp_hldc_online_state {
	enum ldc_vi_num vinum;
	enum tx_isp_ldc_online_state state;
};

enum tx_isp_offline_ldc_ioctl {
	LDC_OFFLINE_CMD_CREATE_JOB = 0x00,
	LDC_OFFLINE_CMD_DESTROY_JOB,
	LDC_OFFLINE_CMD_SUBMIT_TASK,
	LDC_OFFLINE_CMD_SUBMIT_TASK_EXT,
};

int lib_tx_isp_ldc_offline_open(void);
int lib_tx_isp_ldc_offline_release(void);
long lib_tx_isp_ldc_offline_create_job(unsigned long arg);
long lib_tx_isp_ldc_offline_destroy_job(unsigned long arg);
long lib_tx_isp_ldc_offline_submit_task(unsigned long arg);
long lib_tx_isp_ldc_offline_submit_task_ext(unsigned long arg);
int lib_tx_isp_ldc_online_set_mode(int ops_type, unsigned long arg);
int lib_tx_isp_ldc_online_probe(int ops_type, unsigned long arg);
int lib_tx_isp_ldc_online_init(int ops_type, unsigned long arg);
int lib_tx_isp_ldc_online_get_init_state(void);
int lib_tx_isp_ldc_online_get_param(int ops_type, unsigned long arg);
int lib_tx_isp_ldc_online_set_hldc_bypass(int ops_type, unsigned long arg);
int lib_tx_isp_ldc_online_refresh_lut(int ops_type, unsigned long arg);
uint32_t ldc_interrupt_service_routine(void);

int tx_isp_write_sensor_register(struct tx_isp_dbg_register *sensorReg);
int tx_isp_read_sensor_register(struct tx_isp_dbg_register *sensorReg);
//////////////////////////////////////////////
/* fs */
int lib_tx_isp_frame_channel_open(void *data);
int lib_tx_isp_frame_channel_release(void *data);
int lib_tx_isp_frame_channel_vidioc_get_fmt(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_vidioc_set_fmt(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_vb2_querybuf(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_vb2_reqbufs(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_vb2_qbuf(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_vb2_dqbuf(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_vb2_streamon(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_vb2_streamoff(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_listen_buffer(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_set_channel_banks(int ops, void *data, unsigned long arg);
int lib_tx_isp_frame_channel_debug_info(char *buf, unsigned int buf_size);
int lib_tx_isp_set_raw_share_line(int ops, void *data, unsigned long arg);
/* ivdc */
int lib_tx_isp_ivdc_debug_info(char *buf, unsigned int buf_size);
int lib_tx_isp_ivdc_set_direct_mode(unsigned int mode);
int lib_tx_isp_ivdc_set_mem_line(unsigned int line);
int lib_tx_isp_ivdc_set_mem_paddr(unsigned int paddr);

/*************************** tuning *********************/
#define TX_ISP_TISP_IOCTL_SCRATCH_SIZE ((20 * 1024) + 64)

enum tx_isp_priv_ioctl_direction {
	TX_ISP_PRIVATE_IOCTL_SET,
	TX_ISP_PRIVATE_IOCTL_GET,
};
struct tisp_control {
	unsigned int id;
	int value;
};
struct isp_image_tuning_default_ctrl {
	int vinum;
	enum tx_isp_priv_ioctl_direction dir;
	struct tisp_control control;
};

long lib_tx_isp_core_tunning_default_ioctl_ex(unsigned int cmd, unsigned long arg, void *tisp_par_buf, unsigned int tisp_par_buf_size);
long lib_tx_isp_core_tunning_default_ioctl(unsigned int cmd, unsigned long arg);
long lib_tx_isp_core_ae_start_ev_ioctl(int vinum, enum tx_isp_priv_ioctl_direction dir, unsigned long attr_arg);
int lib_tx_isp_core_tuning_filter_draw(const unsigned long arg);
int tx_isp_fw_device_open(tx_isp_ops_type_t ops, tx_isp_mode_t mode, void *data);
int tx_isp_fw_device_release(tx_isp_ops_type_t ops, tx_isp_mode_t mode, void *data);

#endif
