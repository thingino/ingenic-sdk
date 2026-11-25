// SPDX-License-Identifier: GPL-2.0
/*
 * imp-two: libimp H.264 encode of two frame source channels at once
 * (chn 0 full size, chn 1 scaled down), the way a main + sub stream is
 * set up. Each channel runs in its own thread; the per-channel frame
 * count and rate are printed once per second and at the end.
 *
 *   imp-two [seconds]
 *
 * IMP_SENSOR, IMP_ADDR, IMP_W, IMP_H pick the sensor and its size;
 * SUB_W and SUB_H the second channel's size (default 640x360).
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

extern int IMP_Encoder_SetPoolSize(int size);

#define STEP(x) do { int r_ = (x); if (r_ < 0) { printf("%s failed %d\n", #x, r_); return 1; } } while (0)

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
	ea.encAttr.enType = PT_H264;
	ea.encAttr.profile = 1;
	ea.encAttr.picWidth = w;
	ea.encAttr.picHeight = h;
	ea.rcAttr.outFrmRate.frmRateNum = 25;
	ea.rcAttr.outFrmRate.frmRateDen = 1;
	ea.rcAttr.maxGop = 50;
	ea.rcAttr.attrRcMode.rcMode = ENC_RC_MODE_FIXQP;
	ea.rcAttr.attrRcMode.attrH264FixQp.qp = 38;
	ea.rcAttr.attrHSkip.hSkipAttr.skipType = IMP_Encoder_STYPE_N1X;
	ea.rcAttr.attrHSkip.maxHSkipType = IMP_Encoder_STYPE_N1X;
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
	double t0, t;
	int i, last[2] = { 0, 0 }, rc = 0;

	memset(&si, 0, sizeof(si));
	strcpy(si.name, name);
	si.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
	strcpy(si.i2c.type, name);
	si.i2c.addr = envi("IMP_ADDR", 0x40);

	IMP_Encoder_SetPoolSize(2 * 1024 * 1024);
	STEP(IMP_ISP_Open());
	STEP(IMP_ISP_AddSensor(&si));
	STEP(IMP_ISP_EnableSensor());
	STEP(IMP_System_Init());
	STEP(IMP_ISP_EnableTuning());
	STEP(IMP_ISP_Tuning_SetSensorFPS(25, 1));
	puts("isp up");

	if (setup(0, w, h, 3, 0) || setup(1, sw, sh, 2, 1))
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
