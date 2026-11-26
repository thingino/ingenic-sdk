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
 * Build with -DIMP_ENC_AVC_AUTO against the T23 1.3.0 headers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

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
