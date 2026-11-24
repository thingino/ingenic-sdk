// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-tun.c - C for the tuning ioctl firmware functions that read
 * the task's address limit through $gp:
 *
 *   isp_core_tunning_unlocked_ioctl  the /dev/isp-m0 ioctl (the only caller of
 *                                    apical_isp_core_ops_g_ctrl)
 *   apical_isp_core_ops_g_ctrl       its get-control dispatcher
 *   tisp_code_tuning_ioctl           the ioctl of the code tuning node
 *
 * Firmware-owned objects keep the firmware's offsets (FW()); the user copies
 * the inlined access_ok + __copy_user sequences came from are
 * copy_{from,to}_user(). See tx-isp-fwc.c.
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

/* getters: (0, out) or (out); the first argument is always zero */
extern int tisp_g_ae_it_max(int z, void *out);
extern int tisp_g_wb_attr(int z, void *out);
extern int tisp_g_rgb_coefft(int z, void *out);
extern int tisp_g_awb_start(int z, void *out);
extern int tisp_g_awb_cluster(int z, void *out);
extern int tisp_g_awb_ct_trend(int z, void *out);
extern int tisp_g_wb_ct(int z, void *out);
extern int tisp_g_ev_attr(int z, void *out);
extern int tisp_g_Hilightdepress(int z, void *out);
extern int tisp_g_ae_hist(int z, void *out);
extern int tisp_g_ae_min(int z, void *out);
extern int tisp_g_ae_at_list(int z, void *out);
extern int tisp_g_BacklightComp(int z, void *out);
extern int tisp_g_af_metric(int z, void *out);
extern int tisp_g_dpc_strength(int z, void *out);
extern int tisp_g_drc_strength(int z, void *out);
extern int tisp_g_ccm_attr(int z, void *out);
extern int tisp_g_module_control(int z, void *out);
extern int tisp_g_fcrop_control(int z, void *out);
extern int tisp_g_osd_attr(int z, void *out);
extern int tisp_g_osd_block_attr(int z, void *out);
extern int tisp_g_draw_block_attr(int z, void *out);
extern int tisp_g_mscaler_mask_block_attr(int z, void *out);
extern int tisp_g_ncuinfo(void *out);
extern int tisp_get_defog_strength(int z, void *out);
extern int tisp_get_ae_state(int z, void *out);
extern int tisp_get_ae_comp(int z, void *out);
extern int tisp_get_ae_luma(int z, void *out);
extern int tisp_get_bcsh_hue(int z, void *out);
extern int tisp_get_csc_attr(int z, void *out);
extern int tisp_cust_mode_g_ctrl(int z);

/* local to the firmware, one pointer argument each (the control's payload) */
#define FW_ISRA(fn, n)	extern int fn(void *arg) asm(#fn ".isra." #n)
FW_ISRA(apical_isp_awb_zone_statis_g_attr, 97);
FW_ISRA(apical_isp_awb_zone_weight_g_attr, 115);
FW_ISRA(apical_isp_ae_g_roi, 80);
FW_ISRA(apical_isp_expr_g_ctrl, 75);
FW_ISRA(apical_isp_ev_g_attr, 78);
FW_ISRA(apical_isp_ae_zone_weight_g_attr, 92);
FW_ISRA(apical_isp_ae_zone_g_ctrl, 87);
FW_ISRA(apical_isp_ae_hist_origin_g_attr, 95);
FW_ISRA(apical_isp_max_dgain_g_ctrl, 77);
FW_ISRA(apical_isp_max_again_g_ctrl, 76);
FW_ISRA(apical_isp_gamma_g_attr, 79);
FW_ISRA(apical_isp_af_hist_g_attr, 98);
FW_ISRA(tiziano_isp_ae_manual_attr_g_ctrl, 106);
FW_ISRA(apical_isp_af_weight_g_attr, 99);
FW_ISRA(apical_isp_af_zone_g_ctrl, 88);
FW_ISRA(apical_isp_autozoom_g_attr, 71);

extern int apical_isp_core_ops_s_ctrl(void *t, void *ctrl);

/* the 4.4 build's GFP_KERNEL, which private_kmalloc translates */
#define FW_GFP_KERNEL		0x24000c0

/*
 * The firmware returns -ENOIOCTLCMD (-515) from ops it does not implement;
 * callers treat it as success.
 */
#define FW_ENOIOCTLCMD		(-515)

/* ---- apical_isp_core_ops_g_ctrl ---------------------------------------------
 *
 * t is the tuning state (isp-m0 core subdev +0x1c4); ctrl is {id, value or
 * user pointer}. The ids are V4L2-style: 0x8000xxx are the tuning controls,
 * 0x98xxxx/0x9axxxx the stock V4L2 ones. 0 is success, -1 unknown id; the
 * getters' own errors are returned as they are.
 */
/* getters leave parts of their output alone: the firmware copied those stack bytes out */
#ifdef FWC_EMU
#define FWC_OUT_INIT
#else
#define FWC_OUT_INIT	= { }
#endif

#define GC_VAL(t, off)		FW(t, off, u32)
#define GC_ISP(t)		FW(FW(t, 0, u8 *), 0xd8, u8 *)
#define GC_FDW_BYTES		0x18

static int gc_fail(const char *fn, int line, int ret)
{
	isp_printf(1, "%s:%d:Get Control failed!!!\n", fn, line);
	return ret;
}

static int gc_fail_set(const char *fn, int line, int ret)
{
	isp_printf(1, "%s:%d set control failed!!!\n", fn, line);
	return ret;
}

static int gc_fail_get(const char *fn, int line, int ret)
{
	isp_printf(1, "[ %s:%d ] get control failed!!!\n", fn, line);
	return ret;
}

static int gc_fail_copy(const char *fn, int line, int ret)
{
	isp_printf(2, "[ %s:%d ] copy error!!!\n", fn, line);
	return ret;
}

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

	/* a faulting read leaves w zeroed: the firmware used the stack garbage */
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

static int gc_wb(u32 *ctrl, u32 *out)
{
	u32 pair[2];
	int ret;

	ret = tisp_g_wb_attr(0, out);
	if (ret)
		return gc_fail("apical_isp_wb_g_ctrl", 0x5c7, ret);
	if (out[0] >= 10) {
		isp_printf(1, "%s:%d: Can not support this mode!!!\n",
			   "apical_isp_wb_g_ctrl", 0x5ec);
		return -1;
	}
	pair[0] = out[0];
	pair[1] = (u32)(u16)out[2] << 16 | (u16)out[1];
	gc_out(ctrl, pair, 8);
	return 0;
}

static int gc_ae_hist(u32 *ctrl)
{
	u8 *b = private_kmalloc(0x41c, FW_GFP_KERNEL);
	u32 out[4];

	if (!b) {
		isp_printf(1, "Failed to private_kmalloc ae_hist\n");
		return -1;
	}
	tisp_g_ae_hist(0, b);
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
			return gc_fail_set(name, line, r_);		\
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
		(out)[0] &= 0xff;					\
		(ctrl)[1] = (out)[0];					\
		return 0;						\
	} while (0)

#define GC_ISRA(ctrl, fn)	return fn(ctrl + 1)

static int fwc_apical_isp_core_ops_g_ctrl(void *t, void *arg)
{
	u32 *ctrl = arg;
	u32 out[0x14] FWC_OUT_INIT;
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

	case 0x980900: ctrl[1] = GC_VAL(t, 0x4094); return 0;
	case 0x980901: ctrl[1] = GC_VAL(t, 0x408c); return 0;
	case 0x980902: ctrl[1] = GC_VAL(t, 0x4090); return 0;
	case 0x980914: ctrl[1] = GC_VAL(t, 0xeb4); return 0;
	case 0x980915: ctrl[1] = GC_VAL(t, 0xeb0); return 0;
	case 0x980918: ctrl[1] = GC_VAL(t, 0xefc); return 0;
	case 0x98091b: ctrl[1] = GC_VAL(t, 0x3f6c); return 0;
	case 0x98091f: ctrl[1] = GC_VAL(t, 0x409c); return 0;
	case 0x9a091a: ctrl[1] = GC_VAL(t, 0x4098); return 0;
	case 0x8000167: ctrl[1] = GC_VAL(t, 0xec4); return 0;
	case 0x80000e1: ctrl[1] = GC_VAL(t, 0x40a4) & 1; return 0;
	case 0x8000186: ctrl[1] = FW(t, 0x40a9, u8); return 0;
	case 0x8000187: ctrl[1] = FW(t, 0x40a8, u8); return 0;
	case 0x80000e0: ctrl[1] = FW(GC_ISP(t), 0x130, u32); return 0;
	case 0x80000e4:
		isp = GC_ISP(t);
		ctrl[1] = (FW(isp, 0x178, int) != 0) | (FW(isp, 0x170, int) ? 2 : 0);
		return 0;

	case 0x8000004:
		return gc_wb(ctrl, out);
	case 0x8000005:
		tisp_g_wb_attr(0, out);
		ctrl[1] = out[5] << 16 | (u16)out[6];
		return 0;
	case 0x8000009:
		tisp_g_wb_attr(0, out);
		ctrl[1] = out[3] << 16 | (u16)out[4];
		return 0;
	case 0x8000008: GC_COPY(ctrl, out, tisp_g_rgb_coefft(0, out), 6);
	case 0x800000a: GC_COPY(ctrl, out, tisp_g_awb_start(0, out), 8);
	case 0x800000b: GC_ISRA(ctrl, apical_isp_awb_zone_statis_g_attr);
	case 0x800000d: GC_COPY(ctrl, out, tisp_g_wb_ct(0, out), 4);
	case 0x800000e:
		ret = tisp_g_awb_cluster(0, out);
		gc_out(ctrl, out, 0x28);
		if (!ret)
			return 0;
		isp_printf(1, "%s:%d get awb_cluster attr failed!!!\n",
			   "tiziano_isp_awb_cluster_g_attr", 0x38c);
		return ret;
	case 0x800000f:
		ret = tisp_g_awb_ct_trend(0, out);
		gc_out(ctrl, out, 0x18);
		if (!ret)
			return 0;
		isp_printf(1, "%s:%d get awb_cluster attr failed!!!\n",
			   "tiziano_isp_awb_ct_trend_g_attr", 0x3a6);
		return ret;
	case 0x8000010: GC_ISRA(ctrl, apical_isp_awb_zone_weight_g_attr);

	case 0x8000023: GC_BYTE(ctrl, out, tisp_get_ae_comp(0, out));
	case 0x8000024: GC_ISRA(ctrl, apical_isp_ae_g_roi);
	case 0x8000025: GC_ISRA(ctrl, apical_isp_expr_g_ctrl);
	case 0x8000026: GC_ISRA(ctrl, apical_isp_ev_g_attr);
	case 0x8000027:
		ret = tisp_g_ev_attr(0, out);
		if (ret)
			return gc_fail("apical_isp_g_totalgain", 0x18, ret);
		ctrl[1] = out[10];
		return 0;
	case 0x8000028: GC_ISRA(ctrl, apical_isp_max_again_g_ctrl);
	case 0x800002a:
		GC_VALUE(ctrl, out, tisp_g_Hilightdepress(0, out),
			 "apical_isp_hi_light_depress_g_ctrl", 0x642);
	case 0x800002b: GC_ISRA(ctrl, apical_isp_gamma_g_attr);
	case 0x8000029: GC_ISRA(ctrl, apical_isp_max_dgain_g_ctrl);
	case 0x800002d: GC_ISRA(ctrl, apical_isp_ae_zone_weight_g_attr);
	case 0x800002e: return gc_ae_hist(ctrl);
	case 0x800002f:
		ret = tisp_g_ae_min(0, out);
		if (ret)
			return ret;
		gc_out(ctrl, out, 0x10);
		return 0;
	case 0x8000030: GC_ISRA(ctrl, apical_isp_ae_zone_g_ctrl);
	case 0x8000031: GC_ISRA(ctrl, apical_isp_ae_hist_origin_g_attr);
	case 0x8000032:
		GC_VALUE(ctrl, out, tisp_g_ae_it_max(0, out),
			 "apical_isp_ae_it_max_g_ctrl", 0x726);
	case 0x8000033: GC_BYTE(ctrl, out, tisp_get_ae_luma(0, out));
	case 0x8000035: GC_ISRA(ctrl, tiziano_isp_ae_manual_attr_g_ctrl);
	case 0x8000036: GC_COPY(ctrl, out, tisp_get_ae_state(0, out), 0xc);
	case 0x8000037:
		GC_VALUE(ctrl, out, tisp_g_BacklightComp(0, out),
			 "tiziano_isp_backlight_comp_g_ctrl", 0x65e);
	case 0x8000038:
		ret = tisp_g_ae_at_list(0, out);
		gc_out(ctrl, out, 0x28);
		if (!ret)
			return 0;
		isp_printf(1, "%s:%d get ae target list failed!!!\n",
			   "tiziano_isp_ae_target_list_g_ctrl", 0x486);
		return ret;
	case 0x8000039:
		ret = tisp_get_defog_strength(0, out);
		gc_out(ctrl, out, 1);
		return ret;
	case 0x8000042: GC_ISRA(ctrl, apical_isp_af_hist_g_attr);
	case 0x8000043:
		ret = tisp_g_af_metric(0, out);
		if (ret)
			return gc_fail_set("apical_isp_af_metric_g_ctrl", 0x66e, ret);
		gc_out(ctrl, out, 4);
		return 0;
	case 0x8000044: GC_ISRA(ctrl, apical_isp_af_weight_g_attr);
	case 0x8000045:
		isp = GC_ISP(t);
		out[0] = FW(FW(isp, 0x124, u8 *), 0xb4, u16);
		out[1] = FW(FW(isp, 0x124, u8 *), 0xb6, u16);
		out[2] = FW(isp, 0x130, u32);
		out[3] = FW(isp, 0x128, u32);
		out[4] = FW(isp, 0x12c, u32);
		gc_out(ctrl, out, 0x14);
		return 0;
	case 0x8000046: GC_ISRA(ctrl, apical_isp_af_zone_g_ctrl);
	case 0x8000062:
		GC_VALUE(ctrl, out, tisp_g_dpc_strength(0, out),
			 "apical_isp_dpc_strength_g_ctrl", 0x742);
	case 0x8000084: GC_COPY(ctrl, out, tisp_g_ncuinfo(out), 0x14);
	case 0x80000a2:
		GC_VALUE(ctrl, out, tisp_g_drc_strength(0, out),
			 "apical_isp_drc_strength_g_ctrl", 0x75e);
	case 0x80000a6:
		ret = tisp_get_csc_attr(0, out);
		gc_out(ctrl, out, 0x40);
		return ret;
	case 0x80000e2: GC_COPY(ctrl, out, tisp_g_module_control(0, out), 4);
	case 0x80000e3: GC_COPY(ctrl, out, tisp_g_fcrop_control(0, out), 0x14);
	case 0x80000e7:
		ret = tisp_cust_mode_g_ctrl(0);
		if (ret >= 0) {
			ctrl[1] = ret;
			return 0;
		}
		isp_printf(2, "make sure xxx-cust-t31.bin exist\n");
		return -1;
	case 0x80000e8: GC_ISRA(ctrl, apical_isp_autozoom_g_attr);
	case 0x8000100: GC_COPY(ctrl, out, tisp_g_ccm_attr(0, out), 0x28);
	case 0x8000101: GC_BYTE(ctrl, out, tisp_get_bcsh_hue(0, out));
	case 0x8000162:
		return gc_frame_done(ctrl);
	case 0x8000180:
		ret = tisp_g_draw_block_attr(0, out);
		if (ret)
			return gc_fail_get("tx_isp_draw_block_g_attr", 0x957, ret);
		ret = private_copy_to_user((void __user *)ctrl[1], out, 0x20);
		return ret ? gc_fail_copy("tx_isp_draw_block_g_attr", 0x95d, ret) : 0;
	case 0x8000181:
		ret = tisp_g_osd_attr(0, out);
		if (ret)
			return gc_fail_get("tx_isp_osd_g_attr", 0x912, ret);
		ret = private_copy_to_user((void __user *)ctrl[1], out, 0xc);
		return ret ? gc_fail_copy("tx_isp_osd_g_attr", 0x918, ret) : 0;
	case 0x8000182:
		ret = tisp_g_osd_block_attr(0, out);
		if (ret)
			return gc_fail_get("tx_isp_osd_block_g_attr", 0x935, ret);
		ret = private_copy_to_user((void __user *)ctrl[1], out, 0x14);
		return ret ? gc_fail_copy("tx_isp_osd_block_g_attr", 0x93b, ret) : 0;
	case 0x8000183:
		tisp_g_mscaler_mask_block_attr(0, out);
		ret = private_copy_to_user((void __user *)ctrl[1], out, 0x14);
		return ret ? gc_fail_copy("tx_isp_mask_block_g_attr", 0x8f6, ret) : 0;
	}
	return -1;
}

/* ---- isp_core_tunning_unlocked_ioctl ----------------------------------------- */

#define TUN_FILE_CORE		0xcc
#define CORE_TUNING		0x1c4
#define TUN_STATE		0x40c8
#define TUN_STATE_OPEN		3

#define TUNING_G_CTRL		0xc008561b
#define TUNING_S_CTRL		0xc008561c

long fwc_isp_core_tunning_unlocked_ioctl(void *twin, unsigned int cmd, unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	u8 *t = FW(FW(FW(twin, FILE44_PRIVATE_DATA, u8 *), TUN_FILE_CORE, u8 *), CORE_TUNING, u8 *);
	/* the firmware's frame had room for seven words; the set path writes past the 12 it copies */
	int buf[7] = { };
	int ret;

	if (FW(t, TUN_STATE, int) != TUN_STATE_OPEN)
		return -1;

	if (cmd == TUNING_G_CTRL) {
		if (copy_from_user(buf, uarg, 8))
			return -EFAULT;
		ret = fwc_apical_isp_core_ops_g_ctrl(t, buf);
		if (ret && ret != FW_ENOIOCTLCMD)
			return ret;
		if (copy_to_user(uarg, buf, 8))
			return -EFAULT;
	} else if (cmd == TUNING_S_CTRL) {
		if (copy_from_user(buf, uarg, 8))
			return -EFAULT;
		ret = apical_isp_core_ops_s_ctrl(t, buf);
	} else {
		/* {0 = set / else get, id, value} */
		if (copy_from_user(buf, uarg, 12))
			return -EFAULT;
		if (!buf[0]) {
			ret = apical_isp_core_ops_s_ctrl(t, &buf[1]);
		} else {
			ret = fwc_apical_isp_core_ops_g_ctrl(t, &buf[1]);
			if ((!ret || ret == FW_ENOIOCTLCMD) && copy_to_user(uarg, buf, 12))
				ret = -EFAULT;
		}
	}
	return ret == FW_ENOIOCTLCMD ? 0 : ret;
}

/* ---- tisp_code_tuning_ioctl -------------------------------------------------
 *
 * The code tuning node (ioctl type 0x74). tisp_par_ioctl is the firmware's
 * one 0x501c byte exchange buffer: {id, length, reg_map address, ...,
 * payload at +0x1b}. The get commands fill it from the module the id
 * selects, the set commands apply it.
 */
#define TISP_PAR_SIZE		0x501c
#define TISP_PAR_DATA		0x1b
#define TISP_IOC_TYPE		0x74
#define TISP_IOC_NR_MAX		0x32

enum {
	TISP_GET_PAR = 0x20007400, TISP_SET_PAR, TISP_GET_VER, TISP_GET_VER_ISP,
	TISP_GET_PAR_AGAIN, TISP_SET_AE, TISP_SET_AE_MINMAX, TISP_GET_AWB,
	TISP_SET_AWB,
};

extern u8 *tisp_par_ioctl;

extern int tisp_reg_map_get(u32 addr, void *par, void *len);
extern int tisp_reg_map_set(void *par);
extern int tisp_get_ae_info(void *par);
extern int tisp_set_ae_info(void *par);
extern int tisp_set_ae_minmax(void *par);
extern int tisp_get_awb_info(void *par);
extern int tisp_set_awb_info(void *par);

/* the per-module parameter arrays: get(0, data, &len), set(0, data) */
#define TISP_MODULES(X)							\
	X(dpc, 6) X(gib, 7) X(adr, 8) X(dmsc, 9) X(ccm, 10)		\
	X(gamma, 11) X(defog, 12) X(mdns, 14) X(ydns, 15) X(bcsh, 17)	\
	X(clm, 18) X(sharpen, 19) X(sdns, 20) X(hldc, 21) X(ae, 22)	\
	X(awb, 23) X(af, 24) X(msca, 26) X(csccr, 41)

#define X(m, id)							\
	extern int tisp_##m##_param_array_get(int z, void *data, void *len); \
	extern int tisp_##m##_param_array_set(int z, void *data);
TISP_MODULES(X)
#undef X
/* the lightweight shading table is the one set that takes a length too */
extern int tisp_lsc_param_array_get(int z, void *data, void *len);
extern int tisp_lsc_param_array_set(int z, void *data, u16 len);

/* these two take no leading zero */
extern int tisp_top_param_array_get(void *data, void *len);
extern int tisp_top_param_array_set(int z, void *data);
extern int tisp_comn_param_array_get(void *data, void *len);
extern int tisp_comn_param_array_set(int z, void *data);

enum { ID_TOP = 1, ID_LSC = 3, ID_AE_INFO = 0x1b, ID_REG_MAP, ID_AWB_INFO = 0x1e, ID_TEST = 0x24, ID_COMN };

static void tisp_par_get(u8 *par)
{
	u32 id = FW(par, 0, u32);
	void *data = par + TISP_PAR_DATA;
	void *len = par + 4;

	switch (id) {
	case ID_TOP:
		tisp_top_param_array_get(data, len);
		break;
	case ID_COMN:
		tisp_comn_param_array_get(data, len);
		break;
	case ID_LSC:
		tisp_lsc_param_array_get(0, data, len);
		break;
#define X(m, mid)							\
	case mid:							\
		tisp_##m##_param_array_get(0, data, len);		\
		break;
	TISP_MODULES(X)
#undef X
	case ID_AE_INFO:
		tisp_get_ae_info(par);
		break;
	case ID_REG_MAP:
		tisp_reg_map_get(FW(par, 8, u32), par, len);
		break;
	case ID_AWB_INFO:
		tisp_get_awb_info(par);
		break;
	case ID_TEST:
		FW(par, 4, u32) = 4;
		isp_printf(0, "this is test mode!!!");
		break;
	/* ids the firmware knows and has nothing for */
	case 2: case 4: case 5: case 0xd: case 0x10: case 0x19:
		break;
	default:
		isp_printf(2, "[ %s:%d ] Have no this ID(%d)\n", "tisp_get_par_process", 0x191, id);
	}
}

static void tisp_par_set(u8 *par)
{
	u32 id = FW(par, 0, u32);
	void *data = par + TISP_PAR_DATA;

	switch (id) {
	case ID_TOP:
		tisp_top_param_array_set(0, data);
		break;
	case ID_COMN:
		tisp_comn_param_array_set(0, data);
		break;
	case ID_LSC:
		tisp_lsc_param_array_set(0, data, FW(par, 8, u16));
		break;
#define X(m, mid)							\
	case mid:							\
		tisp_##m##_param_array_set(0, data);			\
		break;
	TISP_MODULES(X)
#undef X
	case ID_AE_INFO:
		tisp_set_ae_info(par);
		break;
	case ID_REG_MAP:
		tisp_reg_map_set(par);
		break;
	case ID_AWB_INFO:
		tisp_set_awb_info(par);
		break;
	case ID_TEST:
		isp_printf(0, "this is test mode!!!");
		break;
	case 2: case 4: case 5: case 0xd: case 0x10: case 0x19:
		break;
	default:
		isp_printf(2, "[ %s:%d ] Have no this ID(%d)\n", "tisp_set_par_process", 0x202, id);
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
	case TISP_GET_PAR_AGAIN:
		return tisp_par_out(uarg);
	case TISP_SET_AE:
		ret = tisp_set_in(uarg, 0x261);
		if (!ret)
			tisp_set_ae_info(tisp_par_ioctl);
		return ret;
	case TISP_SET_AE_MINMAX:
		ret = tisp_set_in(uarg, 0x276);
		if (!ret)
			tisp_set_ae_minmax(tisp_par_ioctl);
		return ret;
	case TISP_GET_AWB:
		tisp_get_awb_info(tisp_par_ioctl);
		return tisp_par_out(uarg);
	case TISP_SET_AWB:
		ret = tisp_set_in(uarg, 0x26f);
		if (!ret)
			tisp_set_awb_info(tisp_par_ioctl);
		return ret;
	}
	return 0;
}
