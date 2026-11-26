// SPDX-License-Identifier: GPL-2.0
/*
 * imp-dual: two sensors on one T23 (libimp 1.3.0 multi-sensor API, the
 * multi-sensor tx-isp-t23) streaming at once. Sensor 0 feeds frame source
 * channels 0 (full size) and 1 (scaled), sensor 1 channels 3 and 4, as the
 * vendor's double-sensor sample lays them out. Encoder groups 0..3 take
 * channels 0, 3, 1, 4: H.264 on all four, plus a JPEG channel on each
 * scaled group, started only for the snapshot. The per-channel frame counts
 * are printed once per second.
 * At the end it takes one JPEG and one NV12 frame from each sensor's scaled
 * channel:
 *
 *   /tmp/s<N>.jpg          JPEG of the scaled channel
 *   /tmp/s<N>.nv12         the scaled channel's NV12 frame
 *   /tmp/s<N>-small.nv12   the same decimated by 4 in both directions
 *
 * and prints each frame's plane means, so the two sensors can be told apart.
 *
 *   imp-dual [seconds]
 *
 * Environment: S0_NAME S0_ADDR S1_NAME S1_ADDR (default sc2336p 0x30,
 * sc2336ps1 0x32, both on i2c adapter I2C_BUS, default 0), IMP_W IMP_H the
 * sensor size (1920x1080), SUB_W SUB_H the scaled size (640x360), FPS (15),
 * IVDC=1 to have the main channels' encoders take their frames over the
 * ISP-VPU direct path (tx-isp direct_mode=2), MAIN_NRVB SUB_NRVB the frame
 * buffers per channel (2, 2), MEMDUMP=1 to have libimp write its rmem
 * blocks to /tmp/continuous_mem_info halfway, ENC_POOL the encoder pool
 * (4 MiB, 0: libimp's default), OSD_POOL the IPU and ISP OSD pools (unset), SNAP=0 to skip the pictures, TEARDOWN=0 to exit without
 * releasing.
 * TUNE=1 runs the tuning test while all four channels stream: every libimp
 * tuning get/set pair on both sensors (set -> get must round-trip), the
 * get-control ioctl of every id per sensor, the code tuning node's commands
 * per id and sensor, the vic parameter ioctl, the video input commands and
 * the tuning node's read, each also with bad user pointers. Every line it
 * prints starts with "T " and carries no addresses or timings, so the
 * output of two module builds can be diffed. TUNE_SHARE=1 adds the vic
 * shared memory on/off commands. HFLIP1 mirrors sensor 1 only before the
 * pictures: 1 by the ISP, 2 by the sensor, 3 both.
 * Build with -DIMP_ENC_AVC_AUTO against the T23 1.3.0 headers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_isp.h>

extern int IMP_Encoder_SetPoolSize(int size);
extern int IMP_OSD_SetPoolSize(int size);
/* libimp's rmem allocator: writes every block to /tmp/continuous_mem_info */
extern void IMP_Alloc_Dump_To_File(void);

#define STEP(x) do { int r_ = (x); if (r_ < 0) { printf("%s failed %d\n", #x, r_); return 1; } } while (0)

/* encoder channel -> group: H.264 0..3 on groups 0..3, JPEG 4 and 5 on 2 and 3 */
#define NCHN	6
static const int chn_grp[NCHN] = { 0, 1, 2, 3, 2, 3 };
/* group -> frame source channel */
static const int grp_fs[4] = { 0, 3, 1, 4 };
static const char *const chn_name[NCHN] = {
	"s0-main", "s1-main", "s0-sub", "s1-sub", "s0-jpeg", "s1-jpeg"
};

static const char *envs(const char *n, const char *d) { const char *v = getenv(n); return v ? v : d; }
static int envi(const char *n, int d) { const char *v = getenv(n); return v ? (int)strtol(v, NULL, 0) : d; }

static volatile int stop;
static volatile int frames[NCHN];
static volatile long bytes[NCHN];

static double now(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

static long stream_len(IMPEncoderStream *st)
{
	long n = 0;
	int i;

	for (i = 0; i < st->packCount; i++)
		n += st->pack[i].length;
	return n;
}

static void *reader(void *arg)
{
	int ch = (int)(intptr_t)arg;

	while (!stop) {
		IMPEncoderStream st;

		if (IMP_Encoder_PollingStream(ch, 1000) < 0)
			continue;
		if (IMP_Encoder_GetStream(ch, &st, 1) < 0)
			continue;
		frames[ch]++;
		bytes[ch] += stream_len(&st);
		IMP_Encoder_ReleaseStream(ch, &st);
	}
	return NULL;
}

static void fs_attr(IMPFSChnAttr *fa, int w, int h, int sensor_w, int sensor_h, int fps, int nr)
{
	memset(fa, 0, sizeof(*fa));
	fa->pixFmt = PIX_FMT_NV12;
	fa->outFrmRateNum = fps;
	fa->outFrmRateDen = 1;
	fa->nrVBs = nr;
	fa->type = FS_PHY_CHANNEL;
	fa->picWidth = w;
	fa->picHeight = h;
	fa->scaler.enable = w != sensor_w || h != sensor_h;
	fa->scaler.outwidth = w;
	fa->scaler.outheight = h;
}

static void enc_attr(IMPEncoderCHNAttr *ea, IMPPayloadType type, int w, int h, int fps)
{
	memset(ea, 0, sizeof(*ea));
	ea->encAttr.enType = type;
	ea->encAttr.picWidth = w;
	ea->encAttr.picHeight = h;
	if (type == PT_JPEG) {
		ea->encAttr.profile = 0;
		ea->encAttr.bufSize = w * h;
		return;
	}
#ifdef IMP_ENC_AVC_AUTO
	ea->encAttr.profile = -1;
	ea->encAttr.bufSize = w * h / 2;
#else
	ea->encAttr.profile = 1;
#endif
	ea->rcAttr.outFrmRate.frmRateNum = fps;
	ea->rcAttr.outFrmRate.frmRateDen = 1;
	ea->rcAttr.maxGop = 2 * fps;
	ea->rcAttr.attrRcMode.rcMode = ENC_RC_MODE_FIXQP;
	ea->rcAttr.attrRcMode.attrH264FixQp.qp = 38;
	ea->rcAttr.attrHSkip.hSkipAttr.skipType = IMP_Encoder_STYPE_N1X;
	ea->rcAttr.attrHSkip.maxHSkipType = IMP_Encoder_STYPE_N1X;
}

static int save(const char *path, const void *p, size_t n)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	int ok;

	if (fd < 0)
		return -1;
	ok = write(fd, p, n) == (ssize_t)n;
	close(fd);
	return ok ? 0 : -1;
}

static int snap_jpeg(int sensor)
{
	int ch = 4 + sensor;
	IMPEncoderStream st;
	char path[32];
	int fd, i, ret = -1;

	if (IMP_Encoder_StartRecvPic(ch) < 0 || IMP_Encoder_PollingStream(ch, 2000) < 0 ||
	    IMP_Encoder_GetStream(ch, &st, 1) < 0) {
		printf("sensor%d jpeg: no picture\n", sensor);
		IMP_Encoder_StopRecvPic(ch);
		return -1;
	}
	snprintf(path, sizeof(path), "/tmp/s%d.jpg", sensor);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) {
		for (i = 0; i < st.packCount; i++)
			if (write(fd, (void *)st.pack[i].virAddr, st.pack[i].length) != st.pack[i].length)
				break;
		ret = i == st.packCount ? 0 : -1;
		close(fd);
	}
	printf("sensor%d jpeg: %s %ld bytes%s\n", sensor, path, stream_len(&st), ret ? " WRITE FAILED" : "");
	IMP_Encoder_ReleaseStream(ch, &st);
	IMP_Encoder_StopRecvPic(ch);
	return ret;
}

static int snap_nv12(int sensor, int w, int h)
{
	int fs = sensor ? 4 : 1;
	size_t n = w * h * 3 / 2;
	uint8_t *buf = malloc(n), *small;
	int sw = w / 4, sh = h / 4, x, y;
	unsigned long long sy = 0, su = 0, sv = 0, syl = 0, syr = 0;
	IMPFrameInfo fi;
	char path[40];

	if (!buf)
		return -1;
	if (IMP_FrameSource_SnapFrame(fs, PIX_FMT_NV12, w, h, buf, &fi) < 0) {
		printf("sensor%d nv12: SnapFrame(%d) failed\n", sensor, fs);
		free(buf);
		return -1;
	}
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			sy += buf[y * w + x];
			if (x < w / 2)
				syl += buf[y * w + x];
			else
				syr += buf[y * w + x];
		}
	for (y = 0; y < h / 2; y++)
		for (x = 0; x < w; x += 2) {
			su += buf[w * h + y * w + x];
			sv += buf[w * h + y * w + x + 1];
		}
	snprintf(path, sizeof(path), "/tmp/s%d.nv12", sensor);
	save(path, buf, n);
	small = malloc(sw * sh * 3 / 2);
	if (small) {
		for (y = 0; y < sh; y++)
			for (x = 0; x < sw; x++)
				small[y * sw + x] = buf[y * 4 * w + x * 4];
		for (y = 0; y < sh / 2; y++)
			for (x = 0; x < sw; x += 2) {
				small[sw * sh + y * sw + x] = buf[w * h + y * 4 * w + x * 4];
				small[sw * sh + y * sw + x + 1] = buf[w * h + y * 4 * w + x * 4 + 1];
			}
		snprintf(path, sizeof(path), "/tmp/s%d-small.nv12", sensor);
		save(path, small, sw * sh * 3 / 2);
		free(small);
	}
	printf("sensor%d nv12: %dx%d Y mean %llu (left %llu right %llu) U %llu V %llu, small %dx%d\n",
	       sensor, w, h, sy / (w * h), syl / (w * h / 2), syr / (w * h / 2),
	       su / (w * h / 4), sv / (w * h / 4), sw, sh);
	free(buf);
	return 0;
}

/* one line from libimp's block list: used and free bytes, every block size:used */
static void rmem_report(void)
{
	FILE *f = fopen("/tmp/continuous_mem_info", "r");
	unsigned long sz = 0, used = 0, avail = 0, big = 0, v;
	int n = 0, nu = 0, len = 0;
	char line[128], list[2048] = "";

	if (!f) {
		puts("rmem: no /tmp/continuous_mem_info");
		return;
	}
	while (fgets(line, sizeof(line), f)) {
		if (sscanf(line, "info->size = %lu", &v) == 1) {
			sz = v;
		} else if (sscanf(line, "info->used = %lu", &v) == 1) {
			n++;
			if (v) {
				used += sz;
				nu++;
			} else {
				avail += sz;
				if (sz > big)
					big = sz;
			}
			if (len < (int)sizeof(list) - 24)
				len += snprintf(list + len, sizeof(list) - len, " %lu:%lu", sz, v);
		}
	}
	fclose(f);
	printf("rmem: %d blocks, used %lu bytes (%.2f MiB) in %d, free %lu, largest free %lu\n",
	       n, used, used / 1048576.0, nu, avail, big);
	printf("rmem blocks:%s\n", list);
}

/* ---- TUNE=1 ---------------------------------------------------------------- */

static unsigned char tbuf[4096];

static void tdump(const char *name, int s, int r, const void *p, int nw)
{
	const unsigned int *w = p;
	int i;

	printf("T s%d %-28s = %d", s, name, r);
	for (i = 0; i < nw; i++)
		printf(" %08x", w[i]);
	putchar('\n');
}

/* libimp's call for sensor s: the plain one for sensor 0, _Sec for sensor 1 */
#define API(s, fn, ...)	((s) ? fn##_Sec(__VA_ARGS__) : fn(__VA_ARGS__))

#define TGET(s, fn, nw)							\
	do {								\
		memset(tbuf, 0, sizeof(tbuf));				\
		tdump(#fn, s, API(s, fn, (void *)tbuf), tbuf, nw);	\
	} while (0)

/* set, read back, then put the value from before the test back */
#define TSETGET(s, fn, type, v)						\
	do {								\
		type o_ = 0, g_ = 0;					\
		int p_ = API(s, IMP_ISP_Tuning_Get##fn, &o_);		\
		int r_ = API(s, IMP_ISP_Tuning_Set##fn, v);		\
		int q_ = API(s, IMP_ISP_Tuning_Get##fn, &g_);		\
									\
		if (!p_)						\
			API(s, IMP_ISP_Tuning_Set##fn, o_);		\
		printf("T s%d %-28s = %d get %d %u%s\n", s, #fn "(" #v ")", r_, q_, \
		       (unsigned int)g_, (unsigned int)g_ == (unsigned int)(v) ? "" : " MISMATCH"); \
	} while (0)

static void tune_api(int s)
{
	int d = s ? 7 : 0;

	TSETGET(s, Brightness, unsigned char, 130 + d);
	TSETGET(s, Contrast, unsigned char, 120 + d);
	TSETGET(s, Sharpness, unsigned char, 110 + d);
	TSETGET(s, Saturation, unsigned char, 140 - d);
	TSETGET(s, BcshHue, unsigned char, 100 + d);
	TSETGET(s, AeComp, int, 60 + d);
	TSETGET(s, HiLightDepress, uint32_t, 2 + s);
	TSETGET(s, BacklightComp, uint32_t, 1 + s);
	TSETGET(s, DPC_Strength, unsigned int, 120 + d);
	TSETGET(s, DRC_Strength, unsigned int, 110 + d);
	TSETGET(s, MaxAgain, uint32_t, 0x2000 + 0x100 * s);
	TSETGET(s, MaxDgain, uint32_t, 0x100 + 0x10 * s);
	TSETGET(s, ISPHflip, IMPISPTuningOpsMode, IMPISP_TUNING_OPS_MODE_ENABLE);
	TSETGET(s, ISPVflip, IMPISPTuningOpsMode, IMPISP_TUNING_OPS_MODE_ENABLE);
	TSETGET(s, ISPRunningMode, IMPISPRunningMode, IMPISP_RUNNING_MODE_DAY);
	{
		uint32_t num = 0, den = 0, on = 0, od = 0;
		int p = API(s, IMP_ISP_Tuning_GetSensorFPS, &on, &od);
		int r = API(s, IMP_ISP_Tuning_SetSensorFPS, 15, 1);
		int q = API(s, IMP_ISP_Tuning_GetSensorFPS, &num, &den);

		if (!p && od)
			API(s, IMP_ISP_Tuning_SetSensorFPS, on, od);

		printf("T s%d %-28s = %d get %d %u/%u\n", s, "SensorFPS(15/1)", r, q, num, den);
	}
	TGET(s, IMP_ISP_Tuning_GetISPCustomMode, 1);
	TGET(s, IMP_ISP_Tuning_GetSensorHflip, 1);
	TGET(s, IMP_ISP_Tuning_GetSensorVflip, 1);
	TGET(s, IMP_ISP_Tuning_GetHVFlip, 1);
	TGET(s, IMP_ISP_Tuning_GetGamma, 8);
	TGET(s, IMP_ISP_Tuning_GetWB, 3);
	TGET(s, IMP_ISP_Tuning_GetAwbClust, 10);
	TGET(s, IMP_ISP_Tuning_GetAwbCtTrend, 6);
	TGET(s, IMP_ISP_Tuning_Awb_GetRgbCoefft, 2);
	TGET(s, IMP_ISP_Tuning_GetAeAttr, 8);
	TGET(s, IMP_ISP_Tuning_GetAeMin, 4);
	TGET(s, IMP_ISP_Tuning_GetAeTargetList, 10);
	TGET(s, IMP_ISP_Tuning_GetAE_IT_MAX, 1);
	TGET(s, IMP_ISP_Tuning_GetAeWeight, 8);
	TGET(s, IMP_ISP_Tuning_AE_GetROI, 8);
	TGET(s, IMP_ISP_Tuning_GetAwbWeight, 8);
	TGET(s, IMP_ISP_Tuning_GetAfWeight, 8);
	TGET(s, IMP_ISP_Tuning_GetAfHist, 8);
	TGET(s, IMP_ISP_Tuning_GetModuleControl, 1);
	TGET(s, IMP_ISP_Tuning_GetFrontCrop, 5);
	TGET(s, IMP_ISP_Tuning_GetCCMAttr, 10);
	TGET(s, IMP_ISP_Tuning_GetMask, 8);
	TGET(s, IMP_ISP_Tuning_GetBlcAttr, 3);
	TGET(s, IMP_ISP_Tuning_GetSensorAttr, 4);
	TGET(s, IMP_ISP_Tuning_GetAntiFlickerAttr, 1);
	TGET(s, IMP_ISP_Tuning_GetExpr, 4);
	/* live statistics: present and the return codes; their values move */
	TGET(s, IMP_ISP_Tuning_GetAWBCt, 0);
	TGET(s, IMP_ISP_Tuning_GetAeLuma, 0);
	TGET(s, IMP_ISP_Tuning_GetTotalGain, 0);
	TGET(s, IMP_ISP_Tuning_GetEVAttr, 0);
	TGET(s, IMP_ISP_Tuning_GetAeState, 0);
	TGET(s, IMP_ISP_Tuning_GetAeHist, 0);
	TGET(s, IMP_ISP_Tuning_GetAeHist_Origin, 0);
	TGET(s, IMP_ISP_Tuning_GetAeZone, 0);
	TGET(s, IMP_ISP_Tuning_GetAwbZone, 0);
	TGET(s, IMP_ISP_Tuning_GetAwbHist, 0);
	TGET(s, IMP_ISP_Tuning_GetWB_Statis, 0);
	TGET(s, IMP_ISP_Tuning_GetWB_GOL_Statis, 0);
	TGET(s, IMP_ISP_Tuning_GetAFMetrices, 0);
	TGET(s, IMP_ISP_Tuning_GetAfZone, 0);
}

/* the descriptor libimp holds /dev/isp-m0 with: tuning state "open" */
static int tune_fd(void)
{
	char link[64], name[64];
	int fd;

	for (fd = 3; fd < 256; fd++) {
		ssize_t n;

		snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
		n = readlink(link, name, sizeof(name) - 1);
		if (n > 0) {
			name[n] = 0;
			if (!strcmp(name, "/dev/isp-m0"))
				return fd;
		}
	}
	return -1;
}

/* ids whose value is live (statistics, frame counts): only the return code is compared */
static int live_id(unsigned int id)
{
	static const unsigned int live[] = {
		0x800000b, 0x800000d, 0x8000027, 0x8000030, 0x8000031, 0x8000033,
		0x800002e, 0x8000036, 0x8000042, 0x8000043, 0x8000046, 0x8000084,
		0x8000162, 0x800000a, 0x8000008, 0x8000005, 0x8000009, 0x8000026,
		0x8000025, 0x98091b, 0x980914, 0x980915, 0x980918, 0x80000a5,
	};
	unsigned int i;

	for (i = 0; i < sizeof(live) / sizeof(live[0]); i++)
		if (live[i] == id)
			return 1;
	return 0;
}

static void tune_gctrl(int fd, int s, unsigned int first, unsigned int last)
{
	unsigned int id;

	for (id = first; id < last; id++) {
		unsigned int ctl[2] = { id, (unsigned int)(uintptr_t)tbuf };
		unsigned int *w = (unsigned int *)tbuf;
		int r;

		memset(tbuf, 0, sizeof(tbuf));
		/* the frame-done wait: a short timeout */
		if (id == 0x8000162)
			w[0] = 100;
		errno = 0;
		r = ioctl(fd, 0xc008561b + 2 * s, ctl);
		if (r == -1 && errno == EINVAL && !w[0])
			continue;
		if (live_id(id))
			printf("T s%d g_ctrl %08x = %d errno %d live\n", s, id, r, r < 0 ? errno : 0);
		else
			printf("T s%d g_ctrl %08x = %d errno %d value %s%08x %08x %08x %08x %08x\n", s, id, r,
			       r < 0 ? errno : 0, ctl[1] == (uintptr_t)tbuf ? "buf " : "",
			       ctl[1] == (uintptr_t)tbuf ? 0 : ctl[1], w[0], w[1], w[2], w[3]);
	}
}

static void tune_ctrl_ioctl(int s)
{
	int fd = tune_fd();
	unsigned int q[4], ctl[2];
	int r;

	if (fd < 0) {
		printf("T no /dev/isp-m0 descriptor\n");
		return;
	}
	tune_gctrl(fd, s, 0x8000000, 0x80001a0);
	tune_gctrl(fd, s, 0x980900, 0x980930);
	tune_gctrl(fd, s, 0x9a0900, 0x9a0930);
	/* set then get through the 8-byte command pair of sensor s */
	ctl[0] = 0x980900; ctl[1] = 99 + s;
	r = ioctl(fd, 0xc008561c + 2 * s, ctl);
	ctl[1] = 0;
	printf("T s%d s_ctrl 980900 = %d, g_ctrl = %d value %u\n", s, r, ioctl(fd, 0xc008561b + 2 * s, ctl), ctl[1]);
	/* the 16-byte form {dir, id, value, sensor} */
	q[0] = 0; q[1] = 0x980901; q[2] = 77 + s; q[3] = s;
	r = ioctl(fd, 0xc010561d, q);
	q[0] = 1; q[2] = 0;
	printf("T s%d 16-byte set 980901 = %d, get = %d value %u sensor %u\n", s, r, ioctl(fd, 0xc010561d, q), q[2], q[3]);
	q[0] = 1; q[1] = 0x8000187; q[2] = 0; q[3] = s;
	printf("T s%d 16-byte get 8000187 = %d value %u\n", s, ioctl(fd, 0x5600, q), q[2]);
	/* bad user pointers */
	errno = 0;
	r = ioctl(fd, 0xc008561b + 2 * s, NULL);
	printf("T s%d g_ctrl NULL = %d errno %d\n", s, r, r < 0 ? errno : 0);
	r = ioctl(fd, 0xc008561c + 2 * s, (void *)0xc0000000);
	printf("T s%d s_ctrl kernel pointer = %d errno %d\n", s, r, r < 0 ? errno : 0);
	r = ioctl(fd, 0xc010561d, (void *)8);
	printf("T s%d 16-byte bad pointer = %d errno %d\n", s, r, r < 0 ? errno : 0);
	ctl[0] = 0x8000008; ctl[1] = 8;
	r = ioctl(fd, 0xc008561b + 2 * s, ctl);
	printf("T s%d g_ctrl 8000008 to a bad payload = %d errno %d\n", s, r, r < 0 ? errno : 0);
	ctl[0] = 0x8000180; ctl[1] = 8;
	r = ioctl(fd, 0xc008561b + 2 * s, ctl);
	printf("T s%d g_ctrl 8000180 to a bad payload = %d errno %d\n", s, r, r < 0 ? errno : 0);
}

#define TISP_PAR	0x501c

static unsigned int csum(const unsigned char *p, unsigned int n)
{
	unsigned int c = 0, i;

	if (n > TISP_PAR - 0x1b)
		n = TISP_PAR - 0x1b;
	for (i = 0; i < n; i++)
		c = c * 31 + p[i];
	return c;
}

static int tisp_cmd(int fd, unsigned int cmd, unsigned char *par)
{
	int r;

	errno = 0;
	r = ioctl(fd, cmd, par);
	return r < 0 ? -errno : r;
}

static void tune_tisp(int s)
{
	static unsigned char par[TISP_PAR + 64];
	static const unsigned int bad[] = { 0x20007409, 0x20007300, 0x21007400, 0x20007433, 0x7400 };
	int fd = open("/dev/tisp", O_RDWR), id, r, r2;
	unsigned int c1, c2, i;

	if (fd < 0) {
		printf("T s%d /dev/tisp: open errno %d\n", s, errno);
		return;
	}
	for (id = 0; id <= 0x2b; id++) {
		unsigned int len;

		memset(par, 0, sizeof(par));
		*(unsigned int *)par = id;
		par[0x1a] = s;
		r = tisp_cmd(fd, 0x20007400, par);
		len = *(unsigned int *)(par + 4);
		c1 = csum(par + 0x1b, len);
		/*
		 * the same parameters back, and read again; not the register
		 * map, whose get and set buffers differ
		 */
		r2 = id == 0x1c ? 0 : tisp_cmd(fd, 0x20007401, par);
		memset(par + 4, 0, TISP_PAR - 4);
		par[0x1a] = s;
		tisp_cmd(fd, 0x20007400, par);
		c2 = csum(par + 0x1b, *(unsigned int *)(par + 4));
		printf("T s%d tisp id %2d get = %d len %u sum %08x, set = %d, again %08x%s\n", s, id, r, len, c1, r2,
		       c2, c1 == c2 ? "" : " CHANGED");
		/* the common parameters: word by word */
		if (id == 0x25) {
			unsigned int w[64];

			memcpy(w, par + 0x1b, sizeof(w));
			for (i = 0; i < len / 4 && i < 64; i += 8)
				printf("T s%d tisp id 37 +%03x: %08x %08x %08x %08x %08x %08x %08x %08x\n",
				       s, i * 4, w[i], w[i + 1], w[i + 2], w[i + 3], w[i + 4], w[i + 5],
				       w[i + 6], w[i + 7]);
		}
	}
	memset(par, 0, sizeof(par));
	r = tisp_cmd(fd, 0x20007402, par);
	printf("T s%d tisp version = %d len %u %.12s\n", s, r, *(unsigned int *)(par + 4), par + 0x1b);
	r = tisp_cmd(fd, 0x20007403, par);
	printf("T s%d tisp isp version = %d len %u %.16s\n", s, r, *(unsigned int *)(par + 4), par + 0x1b);
	/* the ae and awb exchanges: the sensor is the one the last buffer named */
	memset(par, 0, sizeof(par));
	*(unsigned int *)par = 0x16;
	par[0x1a] = s;
	tisp_cmd(fd, 0x20007400, par);
	r = tisp_cmd(fd, 0x20007404, par);
	printf("T s%d tisp get ae = %d sensor %u len %u\n", s, r, par[0x1a], *(unsigned int *)(par + 4));
	r = tisp_cmd(fd, 0x20007405, par);
	printf("T s%d tisp set ae = %d\n", s, r);
	/* set ae minmax has no get: only its bad pointer (below), no made-up limits */
	r = tisp_cmd(fd, 0x20007407, par);
	printf("T s%d tisp get awb = %d sensor %u len %u\n", s, r, par[0x1a], *(unsigned int *)(par + 4));
	r = tisp_cmd(fd, 0x20007408, par);
	printf("T s%d tisp set awb = %d\n", s, r);
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
		printf("T s%d tisp ioctl %08x = %d\n", s, bad[i], tisp_cmd(fd, bad[i], par));
	for (i = 0x20007400; i <= 0x20007408; i++) {
		errno = 0;
		r = ioctl(fd, i, NULL);
		printf("T s%d tisp ioctl %08x NULL = %d errno %d\n", s, i, r, r < 0 ? errno : 0);
	}
	{
		char c[4] = { 0 };

		errno = 0;
		r = read(fd, c, 1);
		printf("T s%d tisp read = %d errno %d\n", s, r, r < 0 ? errno : 0);
		r = read(fd, (void *)8, 1);
		printf("T s%d tisp read bad pointer = %d errno %d\n", s, r, r < 0 ? errno : 0);
	}
	close(fd);
}

static void vic_show(const char *what, int r, const unsigned char *par)
{
	const unsigned int *w = (const unsigned int *)par;

	printf("T vic %-22s = %d errno %d  %08x %08x %08x cmd %u\n", what, r, r < 0 ? errno : 0,
	       w[0], w[1], w[2], w[9]);
}

static void tune_vic(void)
{
	static const unsigned int bad[] = { 0, 0x7600, 0x20007500, 0x20007633, 0x20007602 };
	unsigned char par[40];
	int fd = open("/proc/jz/isp/isp-w02", O_RDWR), r, cmd, sub;
	unsigned int i;

	if (fd < 0) {
		printf("T vic: open errno %d\n", errno);
		return;
	}
	memset(par, 0, sizeof(par));
	errno = 0;
	r = ioctl(fd, 0x20007600, par);
	vic_show("get", r, par);
	r = ioctl(fd, 0x20007600, (void *)8);
	vic_show("get, bad pointer", r, par);
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		char name[32];

		snprintf(name, sizeof(name), "ioctl %08x", bad[i]);
		memset(par, 0, sizeof(par));
		errno = 0;
		r = ioctl(fd, bad[i], par);
		vic_show(name, r, par);
	}
	for (cmd = 0; cmd <= 4; cmd++) {
		for (sub = 0; sub <= 1; sub++) {
			char name[32];

			/* 3 starts a dma into par's address, 1 is the shared memory */
			if (cmd == 3 || cmd == 1)
				continue;
			memset(par, 0, sizeof(par));
			par[27] = sub;
			par[26] = 1;
			*(unsigned int *)(par + 36) = cmd;
			snprintf(name, sizeof(name), "set cmd %d sub %d", cmd, sub);
			errno = 0;
			r = ioctl(fd, 0x20007601, par);
			vic_show(name, r, par);
		}
	}
	for (cmd = 1; envi("TUNE_SHARE", 0) && cmd <= 2; cmd++) {
		memset(par, 0, sizeof(par));
		par[27] = 2;
		par[26] = 1;
		*(unsigned int *)(par + 36) = cmd;
		errno = 0;
		r = ioctl(fd, 0x20007601, par);
		vic_show(cmd == 1 ? "set cmd 1" : "set cmd 2 sub 2", r, par);
	}
	errno = 0;
	r = ioctl(fd, 0x20007601, (void *)8);
	vic_show("set, bad pointer", r, par);
	close(fd);
}

/* one video input command, and what the node reads back; returns that as a number */
static unsigned int vin_cmd(const char *cmd, const void *ubuf, size_t n)
{
	int fd = open("/proc/jz/isp/isp-w00", O_RDWR);
	char out[160];
	unsigned int v;
	ssize_t r, k;

	if (fd < 0) {
		printf("T vin: open errno %d\n", errno);
		return 0;
	}
	errno = 0;
	r = write(fd, ubuf ? ubuf : cmd, ubuf || n ? n : strlen(cmd));
	printf("T vin %-26s = %d errno %d", cmd, (int)r, r < 0 ? errno : 0);
	close(fd);
	fd = open("/proc/jz/isp/isp-w00", O_RDONLY);
	k = fd < 0 ? -1 : read(fd, out, sizeof(out) - 1);
	if (fd >= 0)
		close(fd);
	out[k > 0 ? k : 0] = 0;
	v = strtoul(out, NULL, 0);
	for (k = 0; out[k]; k++)
		if (out[k] == '\n')
			out[k] = '|';
	printf(" -> %s\n", out);
	return v;
}

static void tune_vin(void)
{
	static char big[300];
	char w[40];
	unsigned int v;

	vin_cmd("r sen_reg 0x3107", NULL, 0);
	vin_cmd("r sen_reg 0x3108", NULL, 0);
	vin_cmd("r sen_reg1 0x3107", NULL, 0);
	vin_cmd("r sen_reg1 0x3108", NULL, 0);
	vin_cmd("r sen_reg2 0x3107", NULL, 0);
	vin_cmd("r sen_reg2 0x3108", NULL, 0);
	vin_cmd("r sen_reg1 0x3221", NULL, 0);
	vin_cmd("r sen_reg2 0x3221", NULL, 0);
	/* the writes put back what the register holds */
	v = vin_cmd("r sen_reg 0x3902", NULL, 0);
	snprintf(w, sizeof(w), "w sen_reg 0x3902 0x%x", v);
	vin_cmd(w, NULL, 0);
	v = vin_cmd("r sen_reg1 0x3902", NULL, 0);
	snprintf(w, sizeof(w), "w sen_reg1 0x3902 0x%x", v);
	vin_cmd(w, NULL, 0);
	vin_cmd("w sen_reg2 0x3902 0x85", NULL, 0);
	vin_cmd("r sen_reg2 0x3902", NULL, 0);
	vin_cmd("w sen_reg2", NULL, 0);
	vin_cmd("r sen_reg1", NULL, 0);
	vin_cmd("r list", NULL, 0);
	vin_cmd("r all", NULL, 0);
	vin_cmd("junk", NULL, 0);
	memset(big, 'x', sizeof(big));
	memcpy(big, "r sen_reg2 0x3107 ", 18);
	vin_cmd("(300 bytes, r sen_reg2)", big, sizeof(big));
	vin_cmd("(bad pointer, 16)", (void *)8, 16);
	vin_cmd("(bad pointer, 200)", (void *)8, 200);
}

static void tune_read(void)
{
	int fd = open("/dev/isp-m0", O_RDONLY | O_NONBLOCK), r;
	char c[4] = { 0 };

	if (fd < 0) {
		printf("T isp-m0 read: open errno %d\n", errno);
		return;
	}
	errno = 0;
	r = read(fd, c, 1);
	printf("T isp-m0 read nonblock = %d errno %d\n", r, r < 0 ? errno : 0);
	close(fd);
}

static void tune_all(void)
{
	int s;

	printf("T == tuning test\n");
	for (s = 0; s < 2; s++)
		tune_api(s);
	for (s = 0; s < 2; s++)
		tune_ctrl_ioctl(s);
	for (s = 0; s < 2; s++)
		tune_tisp(s);
	tune_vic();
	tune_vin();
	tune_read();
	printf("T == tuning test done\n");
}

/* the vendor sample's order, so a later run in the same boot starts clean */
static int teardown(IMPSensorInfo *si)
{
	int i, g, bad = 0;

	for (i = 0; i < 4; i++)
		bad |= IMP_Encoder_StopRecvPic(i) < 0;
	for (g = 0; g < 4; g++)
		bad |= IMP_FrameSource_DisableChn(grp_fs[g]) < 0;
	for (g = 0; g < 4; g++) {
		IMPCell fs = { DEV_ID_FS, grp_fs[g], 0 };
		IMPCell enc = { DEV_ID_ENC, g, 0 };

		bad |= IMP_System_UnBind(&fs, &enc) < 0;
	}
	for (i = 0; i < NCHN; i++) {
		bad |= IMP_Encoder_UnRegisterChn(i) < 0;
		bad |= IMP_Encoder_DestroyChn(i) < 0;
	}
	for (g = 0; g < 4; g++) {
		bad |= IMP_Encoder_DestroyGroup(g) < 0;
		bad |= IMP_FrameSource_DestroyChn(grp_fs[g]) < 0;
	}
	IMP_System_Exit();
	bad |= IMP_ISP_DisableSensor() < 0;
	bad |= IMP_ISP_DelSensor(&si[0]) < 0;
	bad |= IMP_ISP_DelSensor(&si[1]) < 0;
	bad |= IMP_ISP_DisableTuning() < 0;
	bad |= IMP_ISP_Close() < 0;
	printf("teardown %s\n", bad ? "FAILED" : "ok");
	return bad;
}

int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 10;
	int w = envi("IMP_W", 1920), h = envi("IMP_H", 1080);
	int sw = envi("SUB_W", 640), sh = envi("SUB_H", 360);
	int fps = envi("FPS", 15), ivdc = envi("IVDC", 0), bus = envi("I2C_BUS", 0);
	int main_nr = envi("MAIN_NRVB", 2), sub_nr = envi("SUB_NRVB", 2);
	int lo[NCHN];
	IMPSensorInfo si[2];
	pthread_t th[NCHN];
	double t0, t;
	int i, g, last[NCHN] = { 0 }, rc = 0;

	setvbuf(stdout, NULL, _IOLBF, 0);
	for (i = 0; i < 2; i++) {
		const char *name = envs(i ? "S1_NAME" : "S0_NAME", i ? "sc2336ps1" : "sc2336p");

		memset(&si[i], 0, sizeof(si[i]));
		snprintf(si[i].name, sizeof(si[i].name), "%s", name);
		si[i].cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
		snprintf(si[i].i2c.type, sizeof(si[i].i2c.type), "%s", name);
		si[i].i2c.addr = envi(i ? "S1_ADDR" : "S0_ADDR", i ? 0x32 : 0x30);
		si[i].i2c.i2c_adapter_id = bus;
		si[i].sensor_id = i;
	}

	if (envi("ENC_POOL", 4 << 20))
		IMP_Encoder_SetPoolSize(envi("ENC_POOL", 4 << 20));
	if (envi("OSD_POOL", 0)) {
		IMP_OSD_SetPoolSize(envi("OSD_POOL", 0));
		IMP_ISP_Tuning_SetOsdPoolSize(envi("OSD_POOL", 0));
	}
	STEP(IMP_ISP_Open());
	STEP(IMP_ISP_AddSensor(&si[0]));
	STEP(IMP_ISP_AddSensor(&si[1]));
	STEP(IMP_ISP_EnableSensor());
	STEP(IMP_System_Init());
	STEP(IMP_ISP_EnableTuning());
	printf("isp up: %s@0x%x + %s@0x%x, ivdc %d, nrVBs main %d sub %d\n", si[0].name,
	       si[0].i2c.addr, si[1].name, si[1].i2c.addr, ivdc, main_nr, sub_nr);

	for (g = 0; g < 4; g++) {
		IMPFSChnAttr fa;
		int fw = g < 2 ? w : sw, fh = g < 2 ? h : sh;

		fs_attr(&fa, fw, fh, w, h, fps, g < 2 ? main_nr : sub_nr);
		STEP(IMP_FrameSource_CreateChn(grp_fs[g], &fa));
		STEP(IMP_FrameSource_SetChnAttr(grp_fs[g], &fa));
		STEP(IMP_Encoder_CreateGroup(g));
	}
	for (i = 0; i < NCHN; i++) {
		IMPEncoderCHNAttr ea;
		int grp = chn_grp[i], fw = grp < 2 ? w : sw, fh = grp < 2 ? h : sh;

		enc_attr(&ea, i < 4 ? PT_H264 : PT_JPEG, fw, fh, fps);
		ea.bEnableIvdc = ivdc && grp < 2;
		STEP(IMP_Encoder_CreateChn(i, &ea));
		STEP(IMP_Encoder_RegisterChn(grp, i));
	}
	for (g = 0; g < 4; g++) {
		IMPCell fs = { DEV_ID_FS, grp_fs[g], 0 };
		IMPCell enc = { DEV_ID_ENC, g, 0 };

		STEP(IMP_System_Bind(&fs, &enc));
	}
	for (g = 0; g < 4; g++) {
		STEP(IMP_FrameSource_EnableChn(grp_fs[g]));
		printf("EnableChn(%d) ok\n", grp_fs[g]);
	}
	for (i = 0; i < 4; i++)
		STEP(IMP_Encoder_StartRecvPic(i));

	for (i = 0; i < 4; i++)
		pthread_create(&th[i], NULL, reader, (void *)(intptr_t)i);
	if (envi("TUNE", 0)) {
		sleep(2);
		tune_all();
	}
	for (i = 0; i < NCHN; i++)
		lo[i] = 1 << 30;
	t0 = now();
	for (i = 1; i <= secs; i++) {
		int c;

		sleep(1);
		printf("t=%2d", i);
		for (c = 0; c < 4; c++) {
			printf(" %s %d", chn_name[c], frames[c] - last[c]);
			/* the first seconds are the pipeline filling */
			if (i > 3 && frames[c] - last[c] < lo[c])
				lo[c] = frames[c] - last[c];
			last[c] = frames[c];
		}
		putchar('\n');
		if (i == secs / 2 && envi("MEMDUMP", 0)) {
			IMP_Alloc_Dump_To_File();
			rmem_report();
		}
	}
	t = now() - t0;
	stop = 1;
	for (i = 0; i < 4; i++)
		pthread_join(th[i], NULL);
	for (i = 0; i < 4; i++) {
		printf("%s: %d frames %.1f fps (lowest second %d) %ld bytes (%s %dx%d)\n", chn_name[i],
		       frames[i], frames[i] / t, secs > 3 ? lo[i] : -1, bytes[i],
		       i < 2 ? "fs main" : "fs sub", i < 2 ? w : sw, i < 2 ? h : sh);
		if (!frames[i])
			rc = 2;
	}
	if (envi("HFLIP1", 0) & 1)
		printf("sensor1 ISP hflip: %d\n",
		       IMP_ISP_Tuning_SetISPHflip_Sec(IMPISP_TUNING_OPS_MODE_ENABLE));
	if (envi("HFLIP1", 0) & 2)
		printf("sensor1 sensor hflip: %d\n",
		       IMP_ISP_Tuning_SetSensorHflip_Sec(IMPISP_TUNING_OPS_MODE_ENABLE));
	if (envi("HFLIP1", 0)) {
		sleep(2);
		/*
		 * the mirror/flip register (0x3221 on the SC2336) of the active
		 * sensor (0) and of the firmware's second one ("sen_reg1": 1)
		 */
		vin_cmd("r sen_reg 0x3221", NULL, 0);
		vin_cmd("r sen_reg1 0x3221", NULL, 0);
	}
	for (i = 0; envi("SNAP", 1) && i < 2; i++) {
		if (snap_jpeg(i))
			rc = 3;
		if (snap_nv12(i, sw, sh))
			rc = 3;
	}
	if (envi("TEARDOWN", 1) && teardown(si))
		rc = 4;
	printf("imp-dual done rc=%d\n", rc);
	return rc;
}
