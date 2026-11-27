/*
 * Header: tx-isp-device.h
 * Role  : TX-ISP driver 设备层定义。
 * Scope : subdev 名称、模块数量、descriptor、寄存器访问宏、clock、resource、link、irq/proc/misc-device 状态和 driver 私有设备结构。
 * Notes : 仅供 tx-isp driver 设备层和内部子模块使用。
 */

#ifndef __TX_ISP_DEVICE_H__
#define __TX_ISP_DEVICE_H__

#include <asm/io.h>
#include <asm/irq.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#ifdef CONFIG_KERNEL_5_15
#include <linux/proc_fs.h>
#endif

#include "lib_base.h"

#define TX_ISP_DRIVER_VERSION "H20260723a"

#define MULTICAMERA
#define SENSORNUM SENSOR_NUM
#define CSINUM 2
#define VINNUM 4
#define VICNUM 2
#define ISPCORENUM 4
#define TIZIANONUM 2

#define DUMMY_CLOCK_RATE 0x0000ffff

#define TX_ISP_VIN_NAME "isp-w00"
#define TX_ISP_CSI_NAME "isp-w01"
#define TX_ISP_VIC_NAME "isp-w02"
#define TX_ISP_CORE_NAME "isp-m0"
#define TX_ISP_IVDC_NAME "isp-ivdc"
#define TX_ISP_LDC_NAME "isp-ldc"
#define TX_ISP_MSCA_NAME "isp-msca"

#define TX_ISP_DEV_NAME "isp-device"
#define TX_ISP_IRQ_NAME "isp-irq"
#define TX_ISP_IRQ_ID "isp-irq-id"

struct tx_isp_pad_descriptor {
	unsigned char type;
	unsigned char links_type;
};

struct tx_isp_device_clk {
	const char *name;
	unsigned long rate;
};

struct tx_isp_descriptor {
	unsigned char type;
	unsigned char subtype;
	unsigned char parentid;
	unsigned char unitid;
};

struct tx_isp_subdev_descriptor {
	unsigned char type;
	unsigned char subtype;
	unsigned char parentid;
	unsigned char unitid;
	unsigned char clks_num;
	struct tx_isp_device_clk *clks;
	unsigned char pads_num;
	struct tx_isp_pad_descriptor *pads;
};

struct tx_isp_widget_descriptor {
	unsigned char type;
	unsigned char subtype;
	unsigned char parentid;
	unsigned char unitid;
	unsigned char clks_num;
	struct tx_isp_device_clk *clks;
};

#define tx_isp_readl(base, reg) __raw_readl((base) + (reg))
#define tx_isp_writel(base, reg, value) __raw_writel((value), ((base) + (reg)))
#define tx_isp_readw(base, reg) __raw_readw((base) + (reg))
#define tx_isp_writew(base, reg, value) __raw_writew((value), ((base) + (reg)))
#define tx_isp_readb(base, reg) __raw_readb((base) + (reg))
#define tx_isp_writeb(base, reg, value) __raw_writeb((value), ((base) + (reg)))

#ifdef CONFIG_KERNEL_3_10
#define CDR_ISP_NAME  "div_isp"
#define CDR_ISPA_NAME "div_ispa"
#define CDR_LDC_NAME  "div_ldc"
#define CDR_VPU_NAME  "div_vpu"
#else
#define CDR_ISP_NAME  "mux_isp"
#define CDR_ISPA_NAME "mux_ispa"
#define CDR_LDC_NAME  "mux_ldc"
#define CDR_VPU_NAME  "mux_vpu"
#endif

#ifdef CONFIG_SOC_PRJ008
#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
#define SEN_TCLK "ext"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_TCLK "ext1"
#endif
#elif defined(CONFIG_SOC_PRJ007) || defined(CONFIG_SOC_PRJ009)
#define SEN_TCLK "vpll"
#endif
#define SEN_TCLK1 "apll"

#if defined(CONFIG_KERNEL_4_4_94) || defined(CONFIG_KERNEL_5_15)
#define SEN_MCLK "mux_cim"
#endif
#ifdef CONFIG_KERNEL_3_10
#define SEN_MCLK "div_cim"
#endif

#define SEN_BCLK "div_cim"

struct tx_isp_frame_sync;

enum tx_isp_subdev_id {
	TX_ISP_CORE_SUBDEV_ID,
	TX_ISP_MAX_SUBDEV_ID,
};

typedef enum tisp_running_state {
	TISP_ONESHOT_STATE_DISABLE = 0,
	TISP_ONESHOT_STATE_ENABLE,
	TISP_ONESHOT_STATE_MAX,
} tisp_oneshot_state_t;

#define TX_ISP_ENTITY_ENUM_MAX_DEPTH 16
struct tx_isp_module;
struct tx_isp_irq_device;
struct tx_isp_subdev;
struct tx_isp_subdev_pad;
struct tx_isp_subdev_link;
struct tisp_format;

#if 0
struct tx_isp_module {
	struct tx_isp_descriptor desc;
	struct device *dev;
	const char *name;
	struct miscdevice miscdev;
	/*struct list_head list;*/

	/* the interface */
	struct file_operations *ops;

	/* the interface */
#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
	struct file_operations *debug_ops;
#endif
#ifdef CONFIG_KERNEL_5_15
	struct proc_ops *debug_ops;
#endif

	int kernel;
	tisp_oneshot_state_t oneshot_state; /* 运行状态：oneshot or normal */
	int oneshot_wait_tickdone;
	struct completion tick_comp;

	/* the list header of sub-modules */
	struct tx_isp_module *submods[TX_ISP_ENTITY_ENUM_MAX_DEPTH];
	/* the module's parent */
	void *parent;
	int (*notify)(struct tx_isp_module *module, unsigned int notification, void *data);
};
#endif

/* Description of the connection between modules */
struct link_pad_description {
	char *name; // the module name
	unsigned char type; // the pad type
	unsigned char index; // the index in array of some pad type
};

struct tx_isp_link_config {
	struct link_pad_description src;
	struct link_pad_description dst;
	unsigned int flag;
};

struct tx_isp_link_configs {
	struct tx_isp_link_config *config;
	unsigned int length;
};

/* The description of module entity */
struct tx_isp_subdev_link {
	struct tx_isp_subdev_pad *source; /* Source pad */
	struct tx_isp_subdev_pad *sink; /* Sink pad  */
	struct tx_isp_subdev_link *reverse; /* Link in the reverse direction */
	unsigned int flag; /* Link flag (TX_ISP_LINKTYPE_*) */
	unsigned int state; /* Link state (TX_ISP_MODULE_*) */
};

#if 0
struct tx_isp_subdev_pad {
	struct tx_isp_subdev *sd;		/* Subdev this pad belongs to */
	unsigned char index;			/* Pad index in the entity pads array */
	unsigned char type;				/* Pad type (TX_ISP_PADTYPE_*) */
	unsigned char links_type;		/* Pad link type (TX_ISP_PADLINK_*) */
	unsigned char state;			/* Pad state (TX_ISP_PADSTATE_*) */
	struct tx_isp_subdev_link link; /* The active link */
	int (*event)(struct tx_isp_subdev_pad *, unsigned int event, void *data);
	void *priv;
};
#endif

#if 0
struct tx_isp_chip_ident {
	char name[32];
	char *revision;
	unsigned int ident;
};
#endif

struct tx_isp_initarg;
#if 0
struct tx_isp_subdev_core_ops {
	int (*g_chip_ident)(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip);
	int (*init)(struct tx_isp_subdev *sd, struct tx_isp_initarg *init); // clk's, power's and init ops.
	int (*reset)(struct tx_isp_subdev *sd, struct tx_isp_initarg *init);
	int (*fsync)(struct tx_isp_subdev *sd, struct tx_isp_frame_sync *value);
	int (*g_register)(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg);
	int (*g_register_list)(struct tx_isp_subdev *sd, struct tx_isp_dbg_register_list *reg);
	int (*g_register_all)(struct tx_isp_subdev *sd, struct tx_isp_dbg_register_list *reg);
	int (*s_register)(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg);
	int (*ioctl)(struct tx_isp_subdev *sd, unsigned int cmd, void *arg);
	irqreturn_t (*interrupt_service_routine)(struct tx_isp_subdev *sd, u32 status, bool *handled);
	irqreturn_t (*interrupt_service_thread)(struct tx_isp_subdev *sd, void *data);
};

struct tx_isp_subdev_video_ops {
	int (*s_stream)(struct tx_isp_subdev *sd, struct tx_isp_initarg *init);
	int (*link_stream)(struct tx_isp_subdev *sd, struct tx_isp_initarg *init);
	int (*link_setup)(const struct tx_isp_subdev_pad *local, const struct tx_isp_subdev_pad *remote, u32 flags);
};

struct tx_isp_subdev_sensor_ops {
	int (*release_all_sensor)(struct tx_isp_subdev *sd);
	int (*sync_sensor_attr)(struct tx_isp_subdev *sd, void *arg);
	int (*ioctl)(struct tx_isp_subdev *sd, unsigned int cmd, void *arg);
	int (*pm)(struct tx_isp_subdev *sd, unsigned int cmd, void *arg);
};

struct tx_isp_subdev_pad_ops {
	int (*g_fmt)(struct tx_isp_subdev *sd, struct tisp_format *f);
	int (*s_fmt)(struct tx_isp_subdev *sd, struct tisp_format *f);
	int (*streamon)(struct tx_isp_subdev *sd, void *data);
	int (*streamoff)(struct tx_isp_subdev *sd, void *data);
};
#endif

struct tx_isp_irq_device {
	spinlock_t slock;
	int index;
	int irq;
	int irq_enabled;
	int irq_refCont; /*表示调用次数*/
};

enum tx_isp_module_state {
	TX_ISP_MODULE_UNDEFINE = 0,
	TX_ISP_MODULE_SLAKE,
	TX_ISP_MODULE_ACTIVATE,
	TX_ISP_MODULE_INIT,
	TX_ISP_MODULE_IDLE,
	TX_ISP_MODULE_RESET,
	TX_ISP_MODULE_BUSY,
};

typedef enum tisp_mode {
	TISP_NORMAL_MODE = 0,
	TISP_AOV_MODE,
} tisp_mode_t;

/*
 * Internal ops. Never call this from drivers, only the tx isp device can call
 * these ops.
 *
 * activate_module: called when this subdev is enabled. When called the module
 * could be operated;
 *
 * slake_module: called when this subdev is disabled. When called the
 *      module couldn't be operated.
 *
 */
struct tx_isp_subdev_internal_ops {
	int (*activate_module)(struct tx_isp_subdev *sd);
	int (*slake_module)(struct tx_isp_subdev *sd);
	int (*prepare_module)(struct tx_isp_subdev *sd);
	int (*suspend_module)(struct tx_isp_subdev *sd);
	int (*resume_early_module)(struct tx_isp_subdev *sd);
	int (*resume_module)(struct tx_isp_subdev *sd);
	void (*complete_module)(struct tx_isp_subdev *sd);
};

#define tx_isp_call_module_notify(ent, args...) (!(ent) ? -ENOENT : (((ent)->notify) ? (ent)->notify(((ent)->parent), ##args) : -ENOIOCTLCMD))

#define tx_isp_call_subdev_notify(ent, args...) (!(ent) ? -ENOENT : (((ent)->module.notify) ? ((ent)->module.notify(&((ent)->module), ##args)) : -ENOIOCTLCMD))

#define tx_isp_call_subdev_event(ent, args...) (!(ent) ? -ENOENT : (((ent)->event) ? (ent)->event((ent), ##args) : -ENOIOCTLCMD))

/* Compatibility wrapper: new subdev dispatch is unified via sd->ioctl. */
#define tx_isp_subdev_call(sd, o, f, args...) (!(sd) ? -ENODEV : (((sd)->ioctl) ? (sd)->ioctl((sd), ##args) : -ENOIOCTLCMD))

#if 0
struct tx_isp_subdev {
	struct tx_isp_module module;
	struct tx_isp_irq_device irqdev;
	struct tx_isp_chip_ident chip;

	/* basic members */
	struct resource *res[3];
	void __iomem *base[3];
	struct clk **clks;
	unsigned int clk_num;
	struct tx_isp_subdev_ops *ops;

	/* expanded members */
	unsigned short num_outpads; /* Number of sink pads */
	unsigned short num_inpads;	/* Number of source pads */

	struct tx_isp_subdev_pad *outpads; /* OutPads array (num_pads elements) */
	struct tx_isp_subdev_pad *inpads;  /* InPads array (num_pads elements) */

	void *dev_priv;
	void *host_priv;
};
#endif

struct tx_isp_mem {
	unsigned int vaddr;
	unsigned int paddr;
	unsigned int size;
};

#define TX_ISP_MEMORY_MAX_NUM 16

static struct tx_isp_device_clk tx_isp_clk_info[] = {
	{"div_isp", 600000000},
	{"div_ispa", 600000000},
	{"div_ldc", 520000000},
	{"div_vpu", 600000000},
	{"gate_isp", DUMMY_CLOCK_RATE},
	{"gate_csi", DUMMY_CLOCK_RATE},
	{"gate_ivdc", DUMMY_CLOCK_RATE},
	{"gate_ldc", DUMMY_CLOCK_RATE},
	{"gate_vpu", DUMMY_CLOCK_RATE},
};

typedef struct tx_isp_device {
	struct tx_isp_module module;
	struct device *dev;
	struct clk *clks[ARRAY_SIZE(tx_isp_clk_info)];
	struct miscdevice miscdev;
	struct miscdevice ivdc_miscdev;
	struct miscdevice ldc_miscdev;
	struct miscdevice isp_m0_miscdev;
	struct miscdevice isp_tuning_miscdev;
	char *version;
	struct mutex mlock;
	struct mutex tuning_mlock;
	spinlock_t slock;
	int refcnt;
	int tuning_opened_times;
	void *tuning_par_buf;
	unsigned int tuning_par_buf_size;
	struct tx_isp_mem mem[TX_ISP_MEMORY_MAX_NUM];
	unsigned char mem_used[TX_ISP_MEMORY_MAX_NUM];
	/* debug parameters */
	struct proc_dir_entry *proc;
	struct proc_dir_entry *proc_w00;
	struct proc_dir_entry *proc_w01;
	struct proc_dir_entry *proc_w02;
	struct proc_dir_entry *proc_m0;
	struct proc_dir_entry *proc_tuning;
	struct proc_dir_entry *proc_msca;
	struct proc_dir_entry *proc_ldc;
	struct proc_dir_entry *proc_ivdc;
	struct proc_dir_entry *proc_fs;
	char *proc_buf;
	int irq_num;
	struct tx_isp_irq_device irqdev[4]; /* enable 和disable */
	/* iomem */
	struct resource *res[5];
	void __iomem *base[5];
	void *fwDev;
	tx_isp_fw_completion_t pm_completion;
	tisp_mode_t pm_mode;
} tx_isp_device_t;

struct tx_isp_snap_raw {
	int vinum;
	int total_cnt;
	int raw_type;
	int wdr_rawmode;
	int size;
	uint32_t paddr;
};

struct tx_isp_specified_raw {
	int vinum;
	int frame_index;
	int frame_num;
	int raw_type;
	int wdr_mode;
	int size;
	uint32_t paddr;
};

int tx_isp_save_specified_raw(struct tx_isp_specified_raw *raw);

struct tx_isp_msca_switch {
	int vinum;
	int en;
};

#define miscdev_to_ispdev(mdev) (container_of((mdev), struct tx_isp_device, miscdev))
#define miscdev_to_module(mdev) (&miscdev_to_ispdev((mdev))->module)
#define module_to_subdev(mod) (container_of(mod, struct tx_isp_subdev, module))
#define irqdev_to_subdev(dev) (container_of(dev, struct tx_isp_subdev, irqdev))
#define module_to_ispdev(mod) (container_of(mod, struct tx_isp_device, module))

#define tx_isp_sd_readl(sd, reg) tx_isp_readl(((sd)->base[0]), reg)
#define tx_isp_sd_writel(sd, reg, value) tx_isp_writel(((sd)->base[0]), reg, value)

int private_reset_tx_isp_module(enum tx_isp_subdev_id id);
int tx_isp_reg_set(struct tx_isp_subdev *sd, unsigned int reg, int start, int end, int val);

int tx_isp_send_event_to_remote(struct tx_isp_subdev_pad *pad, unsigned int cmd, void *data);

static inline void tx_isp_set_module_nodeops(struct tx_isp_module *module, struct file_operations *ops)
{
	// module->ops = ops;
}

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
static inline void tx_isp_set_module_debugops(struct tx_isp_module *module, struct file_operations *ops)
{
	// module->debug_ops = ops;
}
#endif

#ifdef CONFIG_KERNEL_5_15
static inline void tx_isp_set_module_debugops(struct tx_isp_module *module, struct proc_ops *ops)
{
	// module->debug_ops = ops;
}
#endif

static inline void tx_isp_set_subdev_nodeops(struct tx_isp_subdev *sd, struct file_operations *ops)
{
	tx_isp_set_module_nodeops(&sd->module, ops);
}

#if defined(CONFIG_KERNEL_3_10) || defined(CONFIG_KERNEL_4_4_94)
static inline void tx_isp_set_subdev_debugops(struct tx_isp_subdev *sd, struct file_operations *ops)
{
	tx_isp_set_module_debugops(&sd->module, ops);
}
#endif
#ifdef CONFIG_KERNEL_5_15
static inline void tx_isp_set_subdev_debugops(struct tx_isp_subdev *sd, struct proc_ops *ops)
{
	tx_isp_set_module_debugops(&sd->module, ops);
}
#endif

#endif /*__TX_ISP_DEVICE_H__*/
