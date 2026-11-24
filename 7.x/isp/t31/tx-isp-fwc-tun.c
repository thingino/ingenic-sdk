// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-fwc-tun.c - C replacements for the tuning ioctl firmware functions
 * that read the current task's address limit by 3.10 offset:
 *
 *   apical_isp_core_ops_g_ctrl  the /dev/isp-m0 get-control dispatcher
 *   tisp_code_tuning_ioctl      the unlocked_ioctl of the tisp (code tuning) node
 *
 * Firmware-owned objects keep the firmware's offsets (FW()); the user copies
 * go through the 7.x copy_from_user()/copy_to_user() the inlined access_ok +
 * __copy_user sequences came from.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

/* ---- firmware symbols ------------------------------------------------------ */

extern int isp_printf(int level, const char *fmt, ...);
extern int private_copy_to_user(void __user *to, const void *from, unsigned long n);
extern void *private_kmalloc(size_t size, unsigned int gfp);
extern void private_kfree(void *p);
extern int isp_frame_done_wait(long timeout, u64 *cnt);

extern int tisp_g_ae_it_max(void *out);
extern int tisp_g_wb_attr(void *out);
extern int tisp_g_rgb_coefft(void *out);
extern int tisp_g_awb_start(void *out);
extern int tisp_g_ev_attr(void *out);
extern int tisp_g_awb_cluster(void *out);
extern int tisp_g_wb_ct(void *out);
extern int tisp_g_awb_ct_trend(void *out);
extern int tisp_g_ae_min(void *out);
extern int tisp_g_ae_hist(void *out);
extern int tisp_g_Hilightdepress(void *out);
extern int tisp_g_ae_at_list(void *out);
extern int tisp_get_defog_strength(void *out);
extern int tisp_get_ae_state(void *out);
extern int tisp_g_BacklightComp(void *out);
extern int tisp_g_ncuinfo(void *out);
extern int tisp_get_blc_attr(void *out);
extern int tisp_g_module_control(void *out);
extern int tisp_g_fcrop_control(void *out);
extern int tisp_get_wdr_output_mode(void *out);
extern int tisp_cust_mode_g_ctrl(void);
extern int tisp_get_bcsh_hue(void *out);
extern int tisp_g_ccm_attr(void *out);
extern int tisp_g_dpc_strength(void *out);
extern int tisp_g_drc_strength(void *out);
extern int tisp_g_af_metric(void *out);
extern int tisp_get_ae_comp(void *out);
extern int tisp_get_ae_luma(void *out);

/* local to the firmware, one pointer argument each (the control's payload) */
#define FW_ISRA(fn, n)	extern int fn(void *arg) asm(#fn ".isra." #n)
FW_ISRA(apical_isp_awb_zone_statis_g_attr, 94);
FW_ISRA(apical_isp_ae_g_roi, 77);
FW_ISRA(apical_isp_expr_g_ctrl, 72);
FW_ISRA(apical_isp_ev_g_attr, 75);
FW_ISRA(apical_isp_ae_zone_weight_g_attr, 89);
FW_ISRA(apical_isp_ae_zone_g_ctrl, 84);
FW_ISRA(apical_isp_ae_hist_origin_g_attr, 92);
FW_ISRA(apical_isp_max_dgain_g_ctrl, 74);
FW_ISRA(apical_isp_max_again_g_ctrl, 73);
FW_ISRA(apical_isp_gamma_g_attr, 76);
FW_ISRA(apical_isp_af_hist_g_attr, 95);
FW_ISRA(tiziano_isp_ae_manual_attr_g_ctrl, 103);
FW_ISRA(apical_isp_af_weight_g_attr, 96);
FW_ISRA(apical_isp_af_zone_g_ctrl, 85);
FW_ISRA(apical_isp_mask_g_attr, 68);
FW_ISRA(tiziano_isp_csc_g_attr, 108);

/* ---- apical_isp_core_ops_g_ctrl ---------------------------------------------
 *
 * t is the tuning state (isp-m0 core subdev +0x1bc); ctrl is {id, value or
 * user pointer}. The ids are V4L2-style: 0x8000xxx are the tuning controls,
 * 0x98xxxx/0x9axxxx the stock V4L2 ones. 0 is success, -1 unknown id; the
 * getters' own errors are returned as they are. Ids that the firmware lists
 * only to answer success without a value are in the first group.
 */
#define GC_VAL(t, off)		FW(t, off, u32)
#define GC_ISP(t)		FW(FW(t, 0, u8 *), 0xd4, u8 *)
#define GC_FDW_WORDS		6

static int gc_out(const u32 *ctrl, const void *buf, unsigned long n)
{
	private_copy_to_user((void __user *)ctrl[1], buf, n);
	return 0;
}

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

/* getter that fills out[0] as the control's value */
static int gc_val(u32 *ctrl, int ret, const u32 *out)
{
	ctrl[1] = out[0];
	return ret;
}

static int gc_wb(u32 *ctrl, u32 *out)
{
	u32 mode;
	u16 lo, hi;
	u32 pair[2];
	int ret;

	ret = tisp_g_wb_attr(out);
	if (ret)
		return gc_fail("apical_isp_wb_g_ctrl", 0x57f, ret);
	mode = out[0];
	if (mode >= 10) {
		isp_printf(1, "%s:%d: Can not support this mode!!!\n",
			   "apical_isp_wb_g_ctrl", 0x5a3);
		return -1;
	}
	lo = out[1];
	hi = out[2];
	pair[0] = mode;
	pair[1] = (u32)hi << 16 | lo;
	private_copy_to_user((void __user *)ctrl[1], pair, 8);
	return 0;
}

static int gc_frame_done(u32 *ctrl)
{
	void __user *uptr = (void __user *)ctrl[1];
	u32 w[GC_FDW_WORDS];
	u64 cnt = 0;
	int ret;

	/* a faulting read leaves w zeroed: the firmware used the stack garbage */
	if (copy_from_user(w, uptr, sizeof(w)))
		memset(w, 0, sizeof(w));
	ret = isp_frame_done_wait(w[0], &cnt);
	w[2] = (u32)cnt;
	w[3] = cnt >> 32;
	if (copy_to_user(uptr, w, sizeof(w)))
		return ret;
	return ret;
}

int fwc_apical_isp_core_ops_g_ctrl(void *t, void *arg);
int fwc_apical_isp_core_ops_g_ctrl(void *t, void *arg)
{
	u32 *ctrl = arg;
	u32 out[0x14];
	int ret;

	switch (ctrl[0]) {
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
	case 0x80000e0: ctrl[1] = FW(GC_ISP(t), 0x12c, u32); return 0;
	case 0x80000e4:
		ctrl[1] = (FW(GC_ISP(t), 0x168, int) ? 2 : 0) |
			  (FW(GC_ISP(t), 0x170, int) != 0);
		return 0;

	case 0x8000004:
		return gc_wb(ctrl, out);
	case 0x8000005:
		tisp_g_wb_attr(out);
		ctrl[1] = out[5] << 16 | (u16)out[6];
		return 0;
	case 0x8000009:
		tisp_g_wb_attr(out);
		ctrl[1] = out[3] << 16 | (u16)out[4];
		return 0;
	case 0x8000008:
		tisp_g_rgb_coefft(out);
		return gc_out(ctrl, out, 6);
	case 0x800000a:
		tisp_g_awb_start(out);
		return gc_out(ctrl, out, 8);
	case 0x800000b:
		return apical_isp_awb_zone_statis_g_attr(ctrl + 1);
	case 0x800000d:
		tisp_g_wb_ct(out);
		return gc_out(ctrl, out, 4);
	case 0x800000e:
		ret = tisp_g_awb_cluster(out);
		private_copy_to_user((void __user *)ctrl[1], out, 0x28);
		if (!ret)
			return 0;
		isp_printf(1, "%s:%d get awb_cluster attr failed!!!\n",
			   "tiziano_isp_awb_cluster_g_attr", 0x344);
		return ret;
	case 0x800000f:
		ret = tisp_g_awb_ct_trend(out);
		private_copy_to_user((void __user *)ctrl[1], out, 0x18);
		if (!ret)
			return 0;
		isp_printf(1, "%s:%d get awb_cluster attr failed!!!\n",
			   "tiziano_isp_awb_ct_trend_g_attr", 0x35e);
		return ret;

	case 0x8000023:
		tisp_get_ae_comp(out);
		ctrl[1] = out[0] & 0xff;
		return 0;
	case 0x8000024:
		return apical_isp_ae_g_roi(ctrl + 1);
	case 0x8000025:
		return apical_isp_expr_g_ctrl(ctrl + 1);
	case 0x8000026:
		return apical_isp_ev_g_attr(ctrl + 1);
	case 0x8000027:
		ret = tisp_g_ev_attr(out);
		if (ret)
			return gc_fail("apical_isp_g_totalgain", 0x19, ret);
		ctrl[1] = out[7];
		return 0;
	case 0x8000028:
		return apical_isp_max_again_g_ctrl(ctrl + 1);
	case 0x8000029:
		return apical_isp_max_dgain_g_ctrl(ctrl + 1);
	case 0x800002a:
		ret = tisp_g_Hilightdepress(out);
		if (ret)
			return gc_fail_set("apical_isp_hi_light_depress_g_ctrl", 0x5f9, ret);
		return gc_val(ctrl, 0, out);
	case 0x800002b:
		return apical_isp_gamma_g_attr(ctrl + 1);
	case 0x800002d:
		return apical_isp_ae_zone_weight_g_attr(ctrl + 1);
	case 0x800002e: {
		u8 *h = private_kmalloc(0x42c, 0xd0);
		u8 *o = (u8 *)out;

		if (!h) {
			isp_printf(1, "Failed to private_kmalloc ae_hist\n");
			return -1;
		}
		tisp_g_ae_hist(h);
		o[0] = FW(h, 0x414, u32);
		o[1] = FW(h, 0x418, u32);
		o[2] = FW(h, 0x41c, u32);
		o[3] = FW(h, 0x420, u32);
		FW(o, 4, u16) = FW(h, 0x400, u32);
		FW(o, 6, u16) = FW(h, 0x404, u32);
		FW(o, 8, u16) = FW(h, 0x408, u32);
		FW(o, 10, u16) = FW(h, 0x40c, u32);
		FW(o, 12, u16) = FW(h, 0x410, u32);
		o[14] = FW(h, 0x424, u32);
		o[15] = FW(h, 0x428, u32);
		private_copy_to_user((void __user *)ctrl[1], out, 0x10);
		private_kfree(h);
		return 0;
	}
	case 0x800002f:
		ret = tisp_g_ae_min(out);
		if (ret)
			return ret;
		private_copy_to_user((void __user *)ctrl[1], out, 0x10);
		return 0;
	case 0x8000030:
		return apical_isp_ae_zone_g_ctrl(ctrl + 1);
	case 0x8000031:
		return apical_isp_ae_hist_origin_g_attr(ctrl + 1);
	case 0x8000032:
		ret = tisp_g_ae_it_max(out);
		if (ret)
			return gc_fail_set("apical_isp_ae_it_max_g_ctrl", 0x6e5, ret);
		return gc_val(ctrl, 0, out);
	case 0x8000033:
		tisp_get_ae_luma(out);
		ctrl[1] = out[0] & 0xff;
		return 0;
	case 0x8000035:
		return tiziano_isp_ae_manual_attr_g_ctrl(ctrl + 1);
	case 0x8000036:
		tisp_get_ae_state(out);
		return gc_out(ctrl, out, 0xc);
	case 0x8000037:
		ret = tisp_g_BacklightComp(out);
		if (ret)
			return gc_fail_set("tiziano_isp_backlight_comp_g_ctrl", 0x615, ret);
		return gc_val(ctrl, 0, out);
	case 0x8000038:
		ret = tisp_g_ae_at_list(out);
		private_copy_to_user((void __user *)ctrl[1], out, 0x28);
		if (!ret)
			return 0;
		isp_printf(1, "%s:%d get ae target list failed!!!\n",
			   "tiziano_isp_ae_target_list_g_ctrl", 0x43e);
		return ret;
	case 0x8000039:
		ret = tisp_get_defog_strength(out);
		private_copy_to_user((void __user *)ctrl[1], out, 1);
		return ret;
	case 0x8000042:
		return apical_isp_af_hist_g_attr(ctrl + 1);
	case 0x8000043:
		if (tisp_g_af_metric(out)) {
			gc_fail_set("apical_isp_af_metric_g_ctrl", 0x625, 0);
			return 0;
		}
		return gc_out(ctrl, out, 4);
	case 0x8000044:
		return apical_isp_af_weight_g_attr(ctrl + 1);
	case 0x8000045: {
		u8 *isp = GC_ISP(t);
		u8 *af = FW(isp, 0x120, u8 *);

		out[0] = FW(af, 0xb0, u16);
		out[1] = FW(af, 0xb2, u16);
		out[2] = FW(isp, 0x12c, u32);
		out[3] = FW(isp, 0x124, u32);
		out[4] = FW(isp, 0x128, u32);
		return gc_out(ctrl, out, 0x14);
	}
	case 0x8000046:
		return apical_isp_af_zone_g_ctrl(ctrl + 1);
	case 0x8000062:
		ret = tisp_g_dpc_strength(out);
		if (ret)
			return gc_fail_set("apical_isp_dpc_strength_g_ctrl", 0x701, ret);
		return gc_val(ctrl, 0, out);
	case 0x8000084:
		tisp_g_ncuinfo(out);
		return gc_out(ctrl, out, 0x14);
	case 0x80000a2:
		ret = tisp_g_drc_strength(out);
		if (ret)
			return gc_fail_set("apical_isp_drc_strength_g_ctrl", 0x71d, ret);
		return gc_val(ctrl, 0, out);
	case 0x80000a5:
		tisp_get_blc_attr(out);
		return gc_out(ctrl, out, 0x14);
	case 0x80000a6:
		return tiziano_isp_csc_g_attr(ctrl + 1);
	case 0x80000e2:
		tisp_g_module_control(out);
		return gc_out(ctrl, out, 4);
	case 0x80000e3:
		tisp_g_fcrop_control(out);
		return gc_out(ctrl, out, 0x14);
	case 0x80000e5:
		return apical_isp_mask_g_attr(ctrl + 1);
	case 0x80000e7:
		ret = tisp_cust_mode_g_ctrl();
		if (ret >= 0) {
			ctrl[1] = ret;
			return 0;
		}
		isp_printf(2, "make sure xxx-cust-t31.bin exist\n");
		return -1;
	case 0x80000ea:
		ret = tisp_get_wdr_output_mode(out);
		private_copy_to_user((void __user *)ctrl[1], out, 4);
		return ret;
	case 0x8000100:
		tisp_g_ccm_attr(out);
		return gc_out(ctrl, out, 0x28);
	case 0x8000101:
		tisp_get_bcsh_hue(out);
		ctrl[1] = out[0] & 0xff;
		return 0;
	case 0x8000162:
		return gc_frame_done(ctrl);
	}
	return -1;
}

/* ---- tisp_code_tuning_ioctl -------------------------------------------------
 *
 * The code tuning node (ioctl type 0x74). tisp_par_ioctl is the firmware's
 * one 0x500c byte exchange buffer: {id, length, reg_map address, payload...}.
 * The get commands fill it from the module the id selects, the set commands
 * apply it.
 */
#define TISP_PAR_SIZE		0x500c
#define TISP_IOC_TYPE		0x74
#define TISP_IOC_NR_MAX		0x32

enum {
	TISP_GET_PAR = 0x20007400, TISP_SET_PAR, TISP_GET_AE = 0x20007403,
	TISP_SET_AE, TISP_GET_AWB = 0x20007406, TISP_SET_AWB,
	TISP_GET_VER, TISP_GET_VER_T31,
};

enum { TISP_ID_TOP, TISP_ID_REG_MAP = 0x17, TISP_ID_DN_MODE, TISP_ID_END };

extern u32 *tisp_par_ioctl;

#define TISP_MODULES(X) \
	X(lsc, 2) TISP_MODULES_SET(X)
#define TISP_MODULES_SET(X) \
	X(blc, 1) X(wdr, 3) X(dpc, 4) X(gib, 5) X(rdns, 6) X(adr, 7) \
	X(dmsc, 8) X(ccm, 9) X(gamma, 10) X(defog, 11) X(mdns, 12) X(ydns, 13) \
	X(bcsh, 14) X(clm, 15) X(ysp, 16) X(sdns, 17) X(af, 18) X(hldc, 19) \
	X(ae, 20) X(awb, 21)

extern int tisp_top_param_array_get(void *par, void *len);
extern int tisp_reg_map_get(void *addr, void *par, void *len);
extern int tisp_reg_map_set(void *par);
extern int tisp_dn_mode_get(void *par, void *len);
extern int tisp_dn_mode_set(void *par);
extern int tisp_lsc_set_par_cfg(u32 addr, void *par);
extern int tisp_get_ae_info(void *par);
extern int tisp_set_ae_info(void *par);
extern int tisp_get_awb_info(void *par);
extern int tisp_set_awb_info(void *par);

#define X(m, id) \
	extern int tisp_##m##_get_par_cfg(void *par, void *len);
TISP_MODULES(X)
#undef X
#define X(m, id) extern int tisp_##m##_set_par_cfg(void *par);
TISP_MODULES_SET(X)
#undef X

typedef int (*tisp_get_fn)(void *par, void *len);
typedef int (*tisp_set_fn)(void *par);

static const tisp_get_fn tisp_get_fns[] = {
	[TISP_ID_TOP] = tisp_top_param_array_get,
#define X(m, id) [id] = tisp_##m##_get_par_cfg,
	TISP_MODULES(X)
#undef X
};

static const tisp_set_fn tisp_set_fns[] = {
#define X(m, id) [id] = tisp_##m##_set_par_cfg,
	TISP_MODULES_SET(X)
#undef X
	[TISP_ID_REG_MAP] = tisp_reg_map_set,
	[TISP_ID_DN_MODE] = tisp_dn_mode_set,
};

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

static int tisp_par_get(void)
{
	u32 *par = tisp_par_ioctl;
	u32 id = par[0];

	if (id == TISP_ID_REG_MAP)
		tisp_reg_map_get((void *)par[2], par, par + 1);
	else if (id == TISP_ID_DN_MODE)
		tisp_dn_mode_get(par, par + 1);
	else if (id < ARRAY_SIZE(tisp_get_fns) && tisp_get_fns[id])
		tisp_get_fns[id](par + 3, par + 1);
	return 0;
}

static void tisp_par_set(void)
{
	u32 *par = tisp_par_ioctl;
	u32 id = par[0];

	if (id == 2)
		tisp_lsc_set_par_cfg(par[2], par + 3);
	else if (id < ARRAY_SIZE(tisp_set_fns) && tisp_set_fns[id])
		tisp_set_fns[id](id == TISP_ID_REG_MAP || id == TISP_ID_DN_MODE ?
				 par : par + 3);
	else
		isp_printf(2, "[ %s:%d ] Have no this ID(%d)\n",
			   "tisp_set_par_process", 0x4b0, id);
}

long fwc_tisp_code_tuning_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
long fwc_tisp_code_tuning_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	const char *ver;

	if (((cmd >> 8) & 0xff) != TISP_IOC_TYPE) {
		isp_printf(2, "_IOC_TYPE error !!! cmd is %d\n", cmd);
		return -EINVAL;
	}
	if ((cmd & 0xff) > TISP_IOC_NR_MAX) {
		isp_printf(2, "_IOC_NR error !!! cmd is %d\n", cmd);
		return -EINVAL;
	}

	/* tisp_code_tuning_open does not check its kmalloc */
	if (!tisp_par_ioctl)
		return -EINVAL;

	switch (cmd) {
	case TISP_GET_PAR:
		if (tisp_par_in(uarg))
			return tisp_par_fail();
		tisp_par_get();
		return tisp_par_out(uarg);
	case TISP_SET_PAR:
		if (tisp_par_in(uarg))
			return tisp_par_fail();
		tisp_par_set();
		return 0;
	case TISP_GET_AE:
		tisp_get_ae_info(tisp_par_ioctl);
		return tisp_par_out(uarg);
	case TISP_GET_AWB:
		tisp_get_awb_info(tisp_par_ioctl);
		return tisp_par_out(uarg);
	case TISP_SET_AE:
	case TISP_SET_AWB:
		if (tisp_par_in(uarg)) {
			isp_printf(2, "%s:%d:set ioctl par copy_to_user error!!!\n",
				   "tisp_code_tuning_ioctl",
				   cmd == TISP_SET_AE ? 0x506 : 0x514);
			return -EFAULT;
		}
		if (cmd == TISP_SET_AE)
			tisp_set_ae_info(tisp_par_ioctl);
		else
			tisp_set_awb_info(tisp_par_ioctl);
		return 0;
	case TISP_GET_VER:
	case TISP_GET_VER_T31:
		if (tisp_par_in(uarg))
			return tisp_par_fail();
		ver = cmd == TISP_GET_VER ? "V-20221024" : "V-T31-20221024";
		tisp_par_ioctl[1] = strlen(ver) + 1;
		memcpy(tisp_par_ioctl + 3, ver, tisp_par_ioctl[1]);
		return tisp_par_out(uarg);
	}
	return 0;
}
