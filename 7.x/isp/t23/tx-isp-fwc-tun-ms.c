// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-tun-ms.c - the tuning ioctls of the multi-sensor firmware
 * (CONFIG_MULTI_SENSOR) that read the task's address limit through $gp:
 *
 *   isp_core_tunning_unlocked_ioctl  the /dev/isp-m0 ioctl (the only caller of
 *                                    apical_isp_core_ops_g_ctrl)
 *   apical_isp_core_ops_g_ctrl       its get-control dispatcher
 *   tisp_code_tuning_ioctl           the ioctl of the code tuning node
 *
 * The same three as tx-isp-fwc-tun.c, but this build keeps tuning state per
 * sensor: the controls take a sensor index (0..2), the code tuning buffer
 * carries one at +0x1a. Firmware-owned objects keep the firmware's offsets
 * (FW()); the user copies are copy_{from,to}_user(). See tx-isp-fwc.c.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "tx-isp-abi.h"
#include "tx-isp-twin.h"

/* ---- firmware symbols ------------------------------------------------------ */

extern __printf(2, 3) int isp_printf(int level, const char *fmt, ...);
extern int private_copy_to_user(void __user *to, const void *from, unsigned long n);
extern void *private_kmalloc(size_t size, unsigned int gfp);
extern void private_kfree(void *p);
extern int isp_frame_done_wait(long timeout, u64 *cnt);

/* getters: (sensor, out) */
extern int tisp_g_ae_it_max(int s, void *out);
extern int tisp_g_wb_attr(int s, void *out);
extern int tisp_g_rgb_coefft(int s, void *out);
extern int tisp_g_awb_start(int s, void *out);
extern int tisp_g_awb_cluster(int s, void *out);
extern int tisp_g_awb_ct_trend(int s, void *out);
extern int tisp_g_wb_ct(int s, void *out);
extern int tisp_g_ev_attr(int s, void *out);
extern int tisp_g_Hilightdepress(int s, void *out);
extern int tisp_g_ae_hist(int s, void *out);
extern int tisp_g_ae_min(int s, void *out);
extern int tisp_g_ae_at_list(int s, void *out);
extern int tisp_g_BacklightComp(int s, void *out);
extern int tisp_g_af_metric(int s, void *out);
extern int tisp_g_dpc_strength(int s, void *out);
extern int tisp_g_drc_strength(int s, void *out);
extern int tisp_g_ccm_attr(int s, void *out);
extern int tisp_g_module_control(int s, void *out);
extern int tisp_g_fcrop_control(int s, void *out);
extern int tisp_g_osd_attr(int s, void *out);
extern int tisp_g_osd_block_attr(int s, void *out);
extern int tisp_g_draw_block_attr(int s, void *out);
extern int tisp_g_mscaler_mask_block_attr(int s, void *out);
extern int tisp_get_defog_strength(int s, void *out);
extern int tisp_get_ae_state(int s, void *out);
extern int tisp_get_ae_comp(int s, void *out);
extern int tisp_get_ae_luma(int s, void *out);
extern int tisp_cust_mode_g_ctrl(int s);
/* these two are not per sensor */
extern int tisp_g_ncuinfo(void *out);
extern int tisp_get_blc_attr(void *out);

/* local to the firmware: (sensor, the control's payload) */
#define FW_ISRA(fn, n)	extern int fn(int s, void *arg) asm(#fn ".isra." #n)
FW_ISRA(apical_isp_mask_g_attr, 75);
FW_ISRA(apical_isp_expr_g_ctrl, 79);
FW_ISRA(apical_isp_max_again_g_ctrl, 80);
FW_ISRA(apical_isp_max_dgain_g_ctrl, 81);
FW_ISRA(apical_isp_ev_g_attr, 82);
FW_ISRA(apical_isp_gamma_g_attr, 83);
FW_ISRA(apical_isp_ae_g_roi, 84);
FW_ISRA(apical_isp_ae_zone_g_ctrl, 90);
FW_ISRA(apical_isp_af_zone_g_ctrl, 91);
FW_ISRA(apical_isp_ae_zone_weight_g_attr, 95);
FW_ISRA(apical_isp_ae_hist_origin_g_attr, 98);
FW_ISRA(apical_isp_awb_zone_statis_g_attr, 100);
FW_ISRA(apical_isp_af_hist_g_attr, 101);
FW_ISRA(apical_isp_af_weight_g_attr, 102);
FW_ISRA(tiziano_isp_ae_manual_attr_g_ctrl, 108);
FW_ISRA(tiziano_isp_csc_g_attr, 113);

extern int apical_isp_core_ops_s_ctrl(void *t, int s, void *ctrl);

/* the 4.4 build's GFP_KERNEL, which private_kmalloc translates */
#define FW_GFP_KERNEL		0x24000c0

/*
 * The firmware returns -ENOIOCTLCMD (-515) from ops it does not implement;
 * callers treat it as success.
 */
#define FW_ENOIOCTLCMD		(-515)

/* the firmware's per-sensor arrays have room for three */
#define TUN_SENSORS		3

/* ---- apical_isp_core_ops_g_ctrl ---------------------------------------------
 *
 * t is the tuning state (isp-m0 core subdev +0x320), s the sensor; ctrl is
 * {id, value or user pointer}. 0 is success, -1 unknown id; the getters'
 * own errors are returned as they are.
 */
#define GC_VAL(t, off)		FW(t, off, u32)
#define GC_SVAL(t, off, s)	FW(t, (off) + 4 * (s), u32)
#define GC_ISP(t)		FW(FW(t, 0, u8 *), 0xd8, u8 *)
#define GC_FDW_BYTES		0x18

static int gc_fail(const char *fmt, int level, const char *fn, int line, int ret)
{
	isp_printf(level, fmt, fn, line);
	return ret;
}

#define GC_FAIL_SET(fn, line, ret) \
	gc_fail("%s:%d set control failed!!!\n", 1, fn, line, ret)
#define GC_FAIL_GET(fn, line, ret) \
	gc_fail("[ %s:%d ] get control failed!!!\n", 1, fn, line, ret)
#define GC_FAIL_COPY(fn, line, ret) \
	gc_fail("[ %s:%d ] copy error!!!\n", 2, fn, line, ret)
#define GC_FAIL_AWB(fn, line, ret) \
	gc_fail("%s:%d get awb_cluster attr failed!!!\n", 1, fn, line, ret)

/* the control's value to the user's buffer, the firmware's way: errors ignored */
static void gc_out(const u32 *ctrl, const void *buf, unsigned long n)
{
	private_copy_to_user((void __user *)ctrl[1], buf, n);
}

static int gc_frame_done(u32 *ctrl)
{
	void __user *uptr = (void __user *)ctrl[1];
	u32 w[GC_FDW_BYTES / 4];
	u64 cnt = 0;
	int ret;

	/* a faulting read leaves w zeroed, as the firmware's access check did */
	if (copy_from_user(w, uptr, sizeof(w)))
		memset(w, 0, sizeof(w));
	ret = isp_frame_done_wait(w[0], &cnt);
	w[2] = (u32)cnt;
	w[3] = cnt >> 32;
	/* the result is the wait's, whether or not the user's buffer took it */
	if (copy_to_user(uptr, w, sizeof(w)))
		return ret;
	return ret;
}

static int gc_ae_hist(int s, u32 *ctrl)
{
	u8 *b = private_kmalloc(0x41c, FW_GFP_KERNEL);
	u32 out[4];

	if (!b) {
		isp_printf(1, "Failed to private_kmalloc ae_hist\n");
		return -1;
	}
	tisp_g_ae_hist(s, b);
	out[0] = FW(b, 0x414, u32);
	out[1] = (u32)(u16)FW(b, 0x404, u32) << 16 | (u16)FW(b, 0x400, u32);
	out[2] = (u32)(u16)FW(b, 0x40c, u32) << 16 | (u16)FW(b, 0x408, u32);
	out[3] = (u32)b[0x419] << 24 | (u32)b[0x418] << 16 | (u16)FW(b, 0x410, u32);
	gc_out(ctrl, out, 0x10);
	private_kfree(b);
	return 0;
}

/* getter that fills out[0] as the control's value */
#define GC_VALUE(ctrl, out, call, name, line)				\
	do {								\
		int r_ = (call);					\
									\
		if (r_)							\
			return GC_FAIL_SET(name, line, r_);		\
		(ctrl)[1] = (out)[0];					\
		return 0;						\
	} while (0)

/* getter whose output is copied out whole */
#define GC_COPY(ctrl, out, call, n)					\
	do {								\
		(call);							\
		gc_out(ctrl, out, n);					\
		return 0;						\
	} while (0)

/* getter whose low byte is the control's value */
#define GC_BYTE(ctrl, out, call)					\
	do {								\
		(call);							\
		(ctrl)[1] = (out)[0] & 0xff;				\
		return 0;						\
	} while (0)

/* getter that copies its output to the user itself, with its own messages */
#define GC_BLOCK(ctrl, out, call, n, name, gline, cline)		\
	do {								\
		int r_ = (call);					\
									\
		if (r_)							\
			return GC_FAIL_GET(name, gline, r_);		\
		r_ = private_copy_to_user((void __user *)(ctrl)[1], out, n); \
		return r_ ? GC_FAIL_COPY(name, cline, r_) : 0;		\
	} while (0)

#define GC_ISRA(s, ctrl, fn)	return fn(s, (ctrl) + 1)

static int fwc_apical_isp_core_ops_g_ctrl(void *t, int s, u32 *ctrl)
{
	u32 out[0x14] = { };
	u8 *isp;
	int ret;

	switch (ctrl[0]) {
	/* ids the firmware answers with success only */
	case 0x8000000: case 0x8000001: case 0x8000003: case 0x8000006:
	case 0x8000007: case 0x8000020: case 0x8000021: case 0x8000022:
	case 0x800002c: case 0x8000040: case 0x8000041: case 0x8000082:
	case 0x8000083: case 0x80000a0: case 0x80000a1: case 0x80000c0:
	case 0x80000c1: case 0x80000c2: case 0x8000120: case 0x8000160:
	case 0x8000161: case 0x8000169:
		return 0;

	/* the stock V4L2 ones: these four are shared by the sensors */
	case 0x980900: ctrl[1] = GC_VAL(t, 0x40ac); return 0;
	case 0x980901: ctrl[1] = GC_VAL(t, 0x40a4); return 0;
	case 0x980902: ctrl[1] = GC_VAL(t, 0x40a8); return 0;
	case 0x98091b: ctrl[1] = GC_VAL(t, 0x3f84); return 0;
	case 0x98091f: ctrl[1] = GC_VAL(t, 0x40b4); return 0;
	case 0x9a091a: ctrl[1] = GC_VAL(t, 0x40b0); return 0;
	case 0x980914: ctrl[1] = GC_SVAL(t, 0xebc, s); return 0;
	case 0x980915: ctrl[1] = GC_SVAL(t, 0xeb0, s); return 0;
	case 0x980918: ctrl[1] = GC_SVAL(t, 0xf0c, s); return 0;
	case 0x8000167: ctrl[1] = GC_VAL(t, 0xed4); return 0;
	case 0x80000e1: ctrl[1] = GC_SVAL(t, 0x40bc, s) & 1; return 0;
	case 0x8000186: ctrl[1] = FW(t, 0x40cb + s, u8); return 0;
	case 0x8000187: ctrl[1] = FW(t, 0x40c8 + s, u8); return 0;
	case 0x80000e0: ctrl[1] = FW(GC_ISP(t), 0x134 + 0x5c * s, u32); return 0;
	case 0x80000e4:
		isp = GC_ISP(t) + 4 * s;
		ctrl[1] = (FW(isp, 0x24c, int) != 0) | (FW(isp, 0x234, int) ? 2 : 0);
		return 0;

	case 0x8000005:
		tisp_g_wb_attr(s, out);
		ctrl[1] = out[5] << 16 | (u16)out[6];
		return 0;
	case 0x8000009:
		tisp_g_wb_attr(s, out);
		ctrl[1] = out[3] << 16 | (u16)out[4];
		return 0;
	case 0x8000008: GC_COPY(ctrl, out, tisp_g_rgb_coefft(s, out), 6);
	case 0x800000a: GC_COPY(ctrl, out, tisp_g_awb_start(s, out), 8);
	case 0x800000b: GC_ISRA(s, ctrl, apical_isp_awb_zone_statis_g_attr);
	case 0x800000d: GC_COPY(ctrl, out, tisp_g_wb_ct(s, out), 4);
	case 0x800000e:
		ret = tisp_g_awb_cluster(s, out);
		gc_out(ctrl, out, 0x28);
		return ret ? GC_FAIL_AWB("tiziano_isp_awb_cluster_g_attr", 0x385, ret) : 0;
	case 0x800000f:
		ret = tisp_g_awb_ct_trend(s, out);
		gc_out(ctrl, out, 0x18);
		return ret ? GC_FAIL_AWB("tiziano_isp_awb_ct_trend_g_attr", 0x39f, ret) : 0;

	case 0x8000023: GC_BYTE(ctrl, out, tisp_get_ae_comp(s, out));
	case 0x8000024: GC_ISRA(s, ctrl, apical_isp_ae_g_roi);
	case 0x8000025: GC_ISRA(s, ctrl, apical_isp_expr_g_ctrl);
	case 0x8000026: GC_ISRA(s, ctrl, apical_isp_ev_g_attr);
	case 0x8000027:
		ret = tisp_g_ev_attr(s, out);
		if (ret)
			return gc_fail("%s:%d:Get Control failed!!!\n", 1,
				       "apical_isp_g_totalgain", 0x18, ret);
		ctrl[1] = out[10];
		return 0;
	case 0x8000028: GC_ISRA(s, ctrl, apical_isp_max_again_g_ctrl);
	case 0x8000029: GC_ISRA(s, ctrl, apical_isp_max_dgain_g_ctrl);
	case 0x800002a:
		GC_VALUE(ctrl, out, tisp_g_Hilightdepress(s, out),
			 "apical_isp_hi_light_depress_g_ctrl", 0x63c);
	case 0x800002b: GC_ISRA(s, ctrl, apical_isp_gamma_g_attr);
	case 0x800002d: GC_ISRA(s, ctrl, apical_isp_ae_zone_weight_g_attr);
	case 0x800002e: return gc_ae_hist(s, ctrl);
	case 0x800002f:
		ret = tisp_g_ae_min(s, out);
		if (ret)
			return ret;
		gc_out(ctrl, out, 0x10);
		return 0;
	case 0x8000030: GC_ISRA(s, ctrl, apical_isp_ae_zone_g_ctrl);
	case 0x8000031: GC_ISRA(s, ctrl, apical_isp_ae_hist_origin_g_attr);
	case 0x8000032:
		GC_VALUE(ctrl, out, tisp_g_ae_it_max(s, out),
			 "apical_isp_ae_it_max_g_ctrl", 0x71e);
	case 0x8000033: GC_BYTE(ctrl, out, tisp_get_ae_luma(s, out));
	case 0x8000035: GC_ISRA(s, ctrl, tiziano_isp_ae_manual_attr_g_ctrl);
	case 0x8000036: GC_COPY(ctrl, out, tisp_get_ae_state(s, out), 0xc);
	case 0x8000037:
		GC_VALUE(ctrl, out, tisp_g_BacklightComp(s, out),
			 "tiziano_isp_backlight_comp_g_ctrl", 0x658);
	case 0x8000038:
		ret = tisp_g_ae_at_list(s, out);
		gc_out(ctrl, out, 0x28);
		if (!ret)
			return 0;
		return gc_fail("%s:%d get ae target list failed!!!\n", 1,
			       "tiziano_isp_ae_target_list_g_ctrl", 0x47f, ret);
	case 0x8000039:
		ret = tisp_get_defog_strength(s, out);
		gc_out(ctrl, out, 1);
		return ret;
	case 0x8000042: GC_ISRA(s, ctrl, apical_isp_af_hist_g_attr);
	case 0x8000043:
		/* a failed read is reported and answered with success */
		if (tisp_g_af_metric(s, out))
			return GC_FAIL_SET("apical_isp_af_metric_g_ctrl", 0x668, 0);
		gc_out(ctrl, out, 4);
		return 0;
	case 0x8000044: GC_ISRA(s, ctrl, apical_isp_af_weight_g_attr);
	case 0x8000045:
		isp = GC_ISP(t);
		out[0] = FW(FW(isp, 0x124, u8 *), 0xb4, u16);
		out[1] = FW(FW(isp, 0x124, u8 *), 0xb6, u16);
		out[2] = FW(isp, 0x134, u32);
		out[3] = FW(isp, 0x12c, u32);
		out[4] = FW(isp, 0x130, u32);
		gc_out(ctrl, out, 0x14);
		return 0;
	case 0x8000046: GC_ISRA(s, ctrl, apical_isp_af_zone_g_ctrl);
	case 0x8000062:
		GC_VALUE(ctrl, out, tisp_g_dpc_strength(s, out),
			 "apical_isp_dpc_strength_g_ctrl", 0x73a);
	case 0x8000084: GC_COPY(ctrl, out, tisp_g_ncuinfo(out), 0x14);
	case 0x80000a2:
		GC_VALUE(ctrl, out, tisp_g_drc_strength(s, out),
			 "apical_isp_drc_strength_g_ctrl", 0x756);
	case 0x80000a5: GC_COPY(ctrl, out, tisp_get_blc_attr(out), 0xc);
	case 0x80000a6: GC_ISRA(s, ctrl, tiziano_isp_csc_g_attr);
	case 0x80000e2: GC_COPY(ctrl, out, tisp_g_module_control(s, out), 4);
	case 0x80000e3: GC_COPY(ctrl, out, tisp_g_fcrop_control(s, out), 0x14);
	case 0x80000e5: GC_ISRA(s, ctrl, apical_isp_mask_g_attr);
	case 0x80000e7:
		ret = tisp_cust_mode_g_ctrl(s);
		if (ret >= 0) {
			ctrl[1] = ret;
			return 0;
		}
		isp_printf(2, "make sure xxx-cust-t31.bin exist\n");
		return -1;
	case 0x8000100: GC_COPY(ctrl, out, tisp_g_ccm_attr(s, out), 0x28);
	case 0x8000162:
		return gc_frame_done(ctrl);
	case 0x8000180:
		GC_BLOCK(ctrl, out, tisp_g_draw_block_attr(s, out), 0x20,
			 "tx_isp_draw_block_g_attr", 0x928, 0x92e);
	case 0x8000181:
		GC_BLOCK(ctrl, out, tisp_g_osd_attr(s, out), 0xc,
			 "tx_isp_osd_g_attr", 0x8e3, 0x8e9);
	case 0x8000182:
		GC_BLOCK(ctrl, out, tisp_g_osd_block_attr(s, out), 0x14,
			 "tx_isp_osd_block_g_attr", 0x906, 0x90c);
	case 0x8000183:
		tisp_g_mscaler_mask_block_attr(s, out);
		ret = private_copy_to_user((void __user *)ctrl[1], out, 0x14);
		return ret ? GC_FAIL_COPY("tx_isp_mask_block_g_attr", 0x8c7, ret) : 0;
	}
	return -1;
}

/* ---- isp_core_tunning_unlocked_ioctl ----------------------------------------- */

#define TUN_FILE_CORE		0xcc
#define CORE_TUNING		0x320
#define TUN_STATE		0x40ec
#define TUN_STATE_OPEN		3

/* get and set for sensors 0, 1 and 2: {id, value} */
#define TUNING_G_CTRL		0xc008561b
#define TUNING_S_CTRL		0xc008561c
#define TUNING_CMDS		6

long fwc_isp_core_tunning_unlocked_ioctl(void *twin, unsigned int cmd, unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	u8 *t = FW(FW(FW(twin, FILE44_PRIVATE_DATA, u8 *), TUN_FILE_CORE, u8 *), CORE_TUNING, u8 *);
	u32 buf[4] = { };
	unsigned int n = cmd - TUNING_G_CTRL;
	u32 *ctrl;
	int s, ret;
	bool get;

	if (FW(t, TUN_STATE, int) != TUN_STATE_OPEN)
		return -1;

	if (n < TUNING_CMDS) {
		if (copy_from_user(buf, uarg, 8))
			return -EFAULT;
		ctrl = buf;
		s = n / 2;
		get = !(n & 1);
	} else {
		/* {0 = set / else get, id, value, sensor} */
		if (copy_from_user(buf, uarg, 16))
			return -EFAULT;
		ctrl = &buf[1];
		s = buf[3];
		get = buf[0];
		/* the firmware indexed its per-sensor state with it unchecked */
		if (buf[3] >= TUN_SENSORS)
			return -EINVAL;
	}

	if (!get) {
		ret = apical_isp_core_ops_s_ctrl(t, s, ctrl);
	} else {
		ret = fwc_apical_isp_core_ops_g_ctrl(t, s, ctrl);
		if (ret && ret != FW_ENOIOCTLCMD)
			return ret;
		if (copy_to_user(uarg, buf, n < TUNING_CMDS ? 8 : 16))
			return -EFAULT;
	}
	return ret == FW_ENOIOCTLCMD ? 0 : ret;
}

/* ---- tisp_code_tuning_ioctl -------------------------------------------------
 *
 * The code tuning node (ioctl type 0x74). tisp_par_ioctl is the firmware's
 * one 0x501c byte exchange buffer: {id, length, reg_map address or table
 * length, ..., sensor at +0x1a, payload at +0x1b}. The get commands fill it
 * from the module the id selects, the set commands apply it.
 */
#define TISP_PAR_SIZE		0x501c
#define TISP_PAR_SENSOR		0x1a
#define TISP_PAR_DATA		0x1b
#define TISP_IOC_TYPE		0x74
#define TISP_IOC_NR_MAX		0x32

enum {
	TISP_GET_PAR = 0x20007400, TISP_SET_PAR, TISP_GET_VER, TISP_GET_VER_ISP,
	TISP_GET_AE, TISP_SET_AE, TISP_SET_AE_MINMAX, TISP_GET_AWB,
	TISP_SET_AWB,
};

extern u8 *tisp_par_ioctl;

extern int tisp_reg_map_get(u32 addr, void *par, void *len);
extern int tisp_reg_map_set(void *par);
extern int tisp_get_ae_info(int s, void *par);
extern int tisp_set_ae_info(int s, void *par);
extern int tisp_set_ae_minmax(void *par);
extern int tisp_get_awb_info(int s, void *par);
extern int tisp_set_awb_info(int s, void *par);

/* the per-module parameter arrays: get(sensor, data, &len), set(sensor, data) */
#define TISP_MODULES(X)							\
	X(top_par, 1) X(dpc_param, 6) X(gib_param, 7) X(adr_param, 8)	\
	X(dmsc_param, 9) X(ccm_param, 10) X(gamma_param, 11)		\
	X(defog_param, 12) X(mdns_param, 14) X(ydns_param, 15)		\
	X(bcsh_param, 17) X(clm_param, 18) X(sharpen_param, 19)		\
	X(sdns_param, 20) X(hldc_param, 21) X(ae_param, 22)		\
	X(awb_param, 23) X(af_param, 24) X(msca_param, 26)		\
	X(comn_param, 37) X(csccr_param, 41)

#define X(m, id)							\
	extern int tisp_##m##_array_get(int s, void *data, void *len);	\
	extern int tisp_##m##_array_set(int s, void *data);
TISP_MODULES(X)
#undef X
/* the shading and api tables take their length (+8) as well */
extern int tisp_lsc_param_array_get(int s, void *data, void *len);
extern int tisp_lsc_param_array_set(int s, void *data, u16 len);
extern int tisp_api_param_array_get(int s, void *data, void *len, u16 n);
extern int tisp_api_param_array_set(int s, void *data, u16 n);

enum { ID_LSC = 3, ID_REG_MAP = 0x1c, ID_AWB_INFO = 0x1e, ID_TEST = 0x24, ID_API = 0x2a };

static void tisp_par_get(u8 *par)
{
	u32 id = FW(par, 0, u32);
	int s = par[TISP_PAR_SENSOR];
	void *data = par + TISP_PAR_DATA;
	void *len = par + 4;

	switch (id) {
#define X(m, mid)							\
	case mid:							\
		tisp_##m##_array_get(s, data, len);			\
		break;
	TISP_MODULES(X)
#undef X
	case ID_LSC:
		tisp_lsc_param_array_get(s, data, len);
		break;
	case ID_API:
		tisp_api_param_array_get(s, data, len, FW(par, 8, u16));
		break;
	case ID_REG_MAP:
		tisp_reg_map_get(FW(par, 8, u32), par, len);
		break;
	case ID_AWB_INFO:
		tisp_get_awb_info(s, par);
		break;
	case ID_TEST:
		FW(par, 4, u32) = 4;
		isp_printf(0, "this is test mode!!!");
		break;
	/* ids the firmware knows and has nothing for */
	case 2: case 4: case 5: case 0xd: case 0x10: case 0x19:
		break;
	default:
		isp_printf(2, "[ %s:%d ] Have no this ID(%d)\n", "tisp_get_par_process", 0x170, id);
	}
}

static void tisp_par_set(u8 *par)
{
	u32 id = FW(par, 0, u32);
	int s = par[TISP_PAR_SENSOR];
	void *data = par + TISP_PAR_DATA;

	switch (id) {
#define X(m, mid)							\
	case mid:							\
		tisp_##m##_array_set(s, data);				\
		break;
	TISP_MODULES(X)
#undef X
	case ID_LSC:
		tisp_lsc_param_array_set(s, data, FW(par, 8, u16));
		break;
	case ID_API:
		tisp_api_param_array_set(s, data, FW(par, 8, u16));
		break;
	case ID_REG_MAP:
		tisp_reg_map_set(par);
		break;
	case ID_AWB_INFO:
		tisp_set_awb_info(s, par);
		break;
	case ID_TEST:
		isp_printf(0, "this is test mode!!!");
		break;
	case 2: case 4: case 5: case 0xd: case 0x10: case 0x19:
		break;
	default:
		isp_printf(2, "[ %s:%d ] Have no this ID(%d)\n", "tisp_set_par_process", 0x1e2, id);
	}
}

static int tisp_par_fail(void)
{
	isp_printf(2, "get ioctl par copy_to_user error!!!\n");
	return -EFAULT;
}

static int tisp_par_in(const void __user *uarg)
{
	return copy_from_user(tisp_par_ioctl, uarg, TISP_PAR_SIZE);
}

static int tisp_par_out(void __user *uarg)
{
	if (copy_to_user(uarg, tisp_par_ioctl, TISP_PAR_SIZE))
		return tisp_par_fail();
	return 0;
}

/* a set command's exchange buffer in, its own message when the copy fails */
static int tisp_set_in(const void __user *uarg, int line)
{
	if (tisp_par_in(uarg)) {
		isp_printf(2, "%s:%d:set ioctl par copy_to_user error!!!\n",
			   "tisp_code_tuning_ioctl", line);
		return -EFAULT;
	}
	return 0;
}

static int tisp_version(const void __user *uarg, const char *ver, size_t len)
{
	if (tisp_par_in(uarg))
		return tisp_par_fail();
	FW(tisp_par_ioctl, 4, u32) = len;
	memcpy(tisp_par_ioctl + TISP_PAR_DATA, ver, len);
	return tisp_par_out((void __user *)uarg);
}

long fwc_tisp_code_tuning_ioctl(void *twin, unsigned int cmd, unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	int ret;

	if (((cmd >> 8) & 0xff) != TISP_IOC_TYPE) {
		isp_printf(2, "_IOC_TYPE error !!! cmd is %d\n", cmd);
		return -EINVAL;
	}
	if ((cmd & 0xff) > TISP_IOC_NR_MAX) {
		isp_printf(2, "_IOC_NR error !!! cmd is %d\n", cmd);
		return -EINVAL;
	}
	if (cmd < TISP_GET_PAR || cmd > TISP_SET_AWB)
		return -EINVAL;
	/* tisp_code_tuning_open does not check its kmalloc */
	if (!tisp_par_ioctl)
		return -EINVAL;

	switch (cmd) {
	case TISP_GET_PAR:
		if (tisp_par_in(uarg))
			return tisp_par_fail();
		tisp_par_get(tisp_par_ioctl);
		return tisp_par_out(uarg);
	case TISP_SET_PAR:
		if (tisp_par_in(uarg))
			return tisp_par_fail();
		tisp_par_set(tisp_par_ioctl);
		return 0;
	case TISP_GET_VER:
		return tisp_version(uarg, "V-20240827", 11);
	case TISP_GET_VER_ISP:
		return tisp_version(uarg, "V-ISP-20240827", 15);
	case TISP_GET_AE:
		/* for the sensor the previous exchange left at +0x1a */
		tisp_get_ae_info(tisp_par_ioctl[TISP_PAR_SENSOR], tisp_par_ioctl);
		return tisp_par_out(uarg);
	case TISP_SET_AE:
		ret = tisp_set_in(uarg, 0x245);
		if (!ret)
			tisp_set_ae_info(tisp_par_ioctl[TISP_PAR_SENSOR], tisp_par_ioctl);
		return ret;
	case TISP_SET_AE_MINMAX:
		ret = tisp_set_in(uarg, 0x25a);
		if (!ret)
			tisp_set_ae_minmax(tisp_par_ioctl);
		return ret;
	case TISP_GET_AWB:
		tisp_get_awb_info(tisp_par_ioctl[TISP_PAR_SENSOR], tisp_par_ioctl);
		return tisp_par_out(uarg);
	case TISP_SET_AWB:
		/* the firmware takes the buffer in and applies nothing */
		return tisp_set_in(uarg, 0x253);
	}
	return 0;
}
