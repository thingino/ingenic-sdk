// SPDX-License-Identifier: GPL-2.0
/*
 * imp-two: libimp H.264 encode of two frame source channels at once
 * (chn 0 full size, chn 1 scaled down), the way a main + sub stream is
 * set up. Each channel runs in its own thread; the per-channel frame
 * count and rate are printed once per second and at the end.
 *
 *   imp-two [seconds]
 *
 * Like a streamer, it reads the sensor size from /proc/jz/sensor and only
 * enables the frame source scaler on a channel smaller than the sensor; with
 * the files missing the sub channel gets no scaler and EnableChn(1) fails.
 * Build flags: -DIMP_API_T41 for the T41 1.2.6 libimp, -DIMP_API2 for the 2.x libimp (T31, T32, T33: IMPVI_MAIN
 * sensor calls, IMP_Encoder_SetDefaultParam), -DIMP_ENC_AVC_AUTO for the
 * T21/T23 encoder attributes (profile -1 and a stream buffer size).
 * IMP_SENSOR, IMP_ADDR pick the sensor; IMP_W and IMP_H its size when the
 * proc files do not say; SUB_W and SUB_H the second channel's size
 * (default 640x360).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_isp.h>

static const char *envs(const char *n, const char *d) { const char *v = getenv(n); return v ? v : d; }
static int envi(const char *n, int d) { const char *v = getenv(n); return v ? (int)strtol(v, NULL, 0) : d; }

#ifdef IMP_API_T41
#define IMP_API2
/* the T41 1.2.6 libimp spells these differently and its sensor info has no fps or switch pin */
#define IMPEncoderCHNAttr IMPEncoderChnAttr
#define ENC_RC_MODE_FIXQP IMP_ENC_RC_MODE_FIXQP
#endif

#ifndef IMP_API2
extern int IMP_Encoder_SetPoolSize(int size);
#endif

#define STEP(x) do { int r_ = (x); if (r_ < 0) { printf("%s failed %d\n", #x, r_); return 1; } } while (0)

static int procint(const char *key)
{
	char path[64], buf[32] = "";
	FILE *f;

	snprintf(path, sizeof(path), "/proc/jz/sensor/sensor0/%s", key);
	f = fopen(path, "r");
	if (!f)
		return 0;
	if (!fgets(buf, sizeof(buf), f))
		buf[0] = 0;
	fclose(f);
	return atoi(buf);
}

static volatile int stop;
static volatile int frames[2];

static double now(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
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
		IMP_Encoder_ReleaseStream(ch, &st);
	}
	return NULL;
}

static int setup(int ch, int w, int h, int nr, int scale)
{
	IMPFSChnAttr fa;
	IMPEncoderCHNAttr ea;
	IMPCell fs = { DEV_ID_FS, ch, 0 };
	IMPCell enc = { DEV_ID_ENC, ch, 0 };

	memset(&fa, 0, sizeof(fa));
	fa.pixFmt = PIX_FMT_NV12;
	fa.outFrmRateNum = 25;
	fa.outFrmRateDen = 1;
	fa.nrVBs = nr;
	fa.type = FS_PHY_CHANNEL;
	fa.picWidth = w;
	fa.picHeight = h;
	fa.scaler.enable = scale;
	fa.scaler.outwidth = w;
	fa.scaler.outheight = h;
	STEP(IMP_FrameSource_CreateChn(ch, &fa));
	STEP(IMP_FrameSource_SetChnAttr(ch, &fa));

	STEP(IMP_Encoder_CreateGroup(ch));
	memset(&ea, 0, sizeof(ea));
#ifdef IMP_API2
	STEP(IMP_Encoder_SetDefaultParam(&ea, IMP_ENC_PROFILE_AVC_MAIN, ENC_RC_MODE_FIXQP, w, h,
					 25, 1, 0, 0, 25, 0));
#else
	ea.encAttr.enType = PT_H264;
#ifdef IMP_ENC_AVC_AUTO
	ea.encAttr.profile = -1;
	ea.encAttr.bufSize = w * h / 2;
#else
	ea.encAttr.profile = 1;
#endif
	ea.encAttr.picWidth = w;
	ea.encAttr.picHeight = h;
	ea.rcAttr.outFrmRate.frmRateNum = 25;
	ea.rcAttr.outFrmRate.frmRateDen = 1;
	ea.rcAttr.maxGop = 50;
	ea.rcAttr.attrRcMode.rcMode = ENC_RC_MODE_FIXQP;
	ea.rcAttr.attrRcMode.attrH264FixQp.qp = 38;
	ea.rcAttr.attrHSkip.hSkipAttr.skipType = IMP_Encoder_STYPE_N1X;
	ea.rcAttr.attrHSkip.maxHSkipType = IMP_Encoder_STYPE_N1X;
#endif
	STEP(IMP_Encoder_CreateChn(ch, &ea));
	STEP(IMP_Encoder_RegisterChn(ch, ch));
	STEP(IMP_System_Bind(&fs, &enc));
	return 0;
}

int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 10;
	int w = envi("IMP_W", 1920), h = envi("IMP_H", 1080);
	int sw = envi("SUB_W", 640), sh = envi("SUB_H", 360);
	const char *name = envs("IMP_SENSOR", "jxf23");
	IMPSensorInfo si;
	pthread_t th[2];
	int known = 0;
	double t0, t;
	int i, last[2] = { 0, 0 }, rc = 0;

	memset(&si, 0, sizeof(si));
	strcpy(si.name, name);
	si.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
	strcpy(si.i2c.type, name);
	si.i2c.addr = envi("IMP_ADDR", 0x40);

#ifdef IMP_API2
	si.rst_gpio = si.pwdn_gpio = si.power_gpio = -1;
#ifndef IMP_API_T41
	si.switch_gpio = -1;
	si.fps.num = 25;
	si.fps.den = 1;
#endif
	si.video_interface = IMPISP_SENSOR_VI_MIPI_CSI0;
	si.mclk = IMPISP_SENSOR_MCLK0;
	STEP(IMP_ISP_Open());
	STEP(IMP_ISP_AddSensor(IMPVI_MAIN, &si));
	STEP(IMP_ISP_EnableSensor(IMPVI_MAIN, &si));
#else
	IMP_Encoder_SetPoolSize(2 * 1024 * 1024);
	STEP(IMP_ISP_Open());
	STEP(IMP_ISP_AddSensor(&si));
	STEP(IMP_ISP_EnableSensor());
#endif
	STEP(IMP_System_Init());
	STEP(IMP_ISP_EnableTuning());
#if !defined(IMP_API2) && !defined(IMP_ENC_AVC_AUTO)
	STEP(IMP_ISP_Tuning_SetSensorFPS(25, 1));
#endif
	puts("isp up");
	if (procint("width") > 0 && procint("height") > 0) {
		w = procint("width");
		h = procint("height");
		known = 1;
		printf("sensor size from /proc/jz/sensor: %dx%d\n", w, h);
	} else {
		puts("sensor size not in /proc/jz/sensor");
	}

	if (setup(0, w, h, envi("MAIN_NRVB", 3), 0) || setup(1, sw, sh, 2, known && (sw != w || sh != h)))
		return 1;
	STEP(IMP_FrameSource_EnableChn(0));
	puts("EnableChn(0) ok");
	STEP(IMP_FrameSource_EnableChn(1));
	puts("EnableChn(1) ok");
	STEP(IMP_Encoder_StartRecvPic(0));
	STEP(IMP_Encoder_StartRecvPic(1));

	for (i = 0; i < 2; i++)
		pthread_create(&th[i], NULL, reader, (void *)(intptr_t)i);
	t0 = now();
	for (i = 1; i <= secs; i++) {
		sleep(1);
		printf("t=%2d chn0 %d chn1 %d\n", i, frames[0] - last[0], frames[1] - last[1]);
		last[0] = frames[0];
		last[1] = frames[1];
	}
	t = now() - t0;
	stop = 1;
	for (i = 0; i < 2; i++)
		pthread_join(th[i], NULL);
	printf("imp-two done: chn0 %d frames %.1f fps (%dx%d), chn1 %d frames %.1f fps (%dx%d)\n",
	       frames[0], frames[0] / t, w, h, frames[1], frames[1] / t, sw, sh);
	if (!frames[0] || !frames[1])
		rc = 2;
	return rc;
}
