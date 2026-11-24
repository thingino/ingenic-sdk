// SPDX-License-Identifier: GPL-2.0
/*
 * t20cap: bring up the ISP and a frame source channel through libimp and
 * read frames from it. Prints a checksum of each frame's luma and chroma so
 * a run can be compared; with a file name it also writes the first frame.
 *
 *   t20cap [frames] [out.nv12]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_isp.h>

/* IMP_SENSOR, IMP_ADDR, IMP_W and IMP_H pick another sensor (the T10's is a 1280x720 jxh42) */
static const char *envs(const char *n, const char *d) { const char *v = getenv(n); return v ? v : d; }
static int envi(const char *n, int d) { const char *v = getenv(n); return v ? (int)strtol(v, NULL, 0) : d; }
#define SENSOR_NAME	envs("IMP_SENSOR", "jxf23")
#define SENSOR_ADDR	envi("IMP_ADDR", 0x40)
#define W		envi("IMP_W", 1920)
#define H		envi("IMP_H", 1080)

static uint32_t sum(const uint8_t *p, size_t n)
{
	uint32_t h = 2166136261u;

	while (n--)
		h = (h ^ *p++) * 16777619u;
	return h;
}

int main(int argc, char **argv)
{
	int frames = argc > 1 ? atoi(argv[1]) : 5;
	const char *out = argc > 2 ? argv[2] : NULL;
	IMPSensorInfo si;
	IMPFSChnAttr fa;
	IMPFrameInfo *f;
	int ret, i;

	memset(&si, 0, sizeof(si));
	strcpy(si.name, SENSOR_NAME);
	si.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
	strcpy(si.i2c.type, SENSOR_NAME);
	si.i2c.addr = SENSOR_ADDR;

	if (IMP_ISP_Open() < 0) { puts("IMP_ISP_Open failed"); return 1; }
	puts("ISP open");
	if (IMP_ISP_AddSensor(&si) < 0) { puts("AddSensor failed"); return 1; }
	puts("sensor added");
	if (IMP_ISP_EnableSensor() < 0) { puts("EnableSensor failed"); return 1; }
	puts("sensor enabled");
	if (IMP_System_Init() < 0) { puts("System_Init failed"); return 1; }
	if (IMP_ISP_EnableTuning() < 0) { puts("EnableTuning failed"); return 1; }
	if (IMP_ISP_Tuning_SetSensorFPS(25, 1) < 0) { puts("SetSensorFPS failed"); return 1; }
	puts("tuning on");

	memset(&fa, 0, sizeof(fa));
	fa.pixFmt = PIX_FMT_NV12;
	fa.outFrmRateNum = 25;
	fa.outFrmRateDen = 1;
	fa.nrVBs = 3;
	fa.type = FS_PHY_CHANNEL;
	fa.crop.enable = 0;
	fa.scaler.enable = 0;
	fa.picWidth = W;
	fa.picHeight = H;
	if (IMP_FrameSource_CreateChn(0, &fa) < 0) { puts("CreateChn failed"); return 1; }
	if (IMP_FrameSource_SetChnAttr(0, &fa) < 0) { puts("SetChnAttr failed"); return 1; }
	if (IMP_FrameSource_EnableChn(0) < 0) { puts("EnableChn failed"); return 1; }
	if (IMP_FrameSource_SetFrameDepth(0, 2) < 0) { puts("SetFrameDepth failed"); return 1; }
	puts("channel enabled");

	for (i = 0; i < frames; i++) {
		ret = IMP_FrameSource_GetFrame(0, &f);
		if (ret < 0) {
			printf("GetFrame %d failed %d\n", i, ret);
			break;
		}
		printf("frame %d: %ux%u fmt %u size %u phys %08x ts %lld y=%08x uv=%08x\n",
		       i, f->width, f->height, f->pixfmt, f->size, f->phyAddr,
		       (long long)f->timeStamp,
		       sum((uint8_t *)f->virAddr, f->width * f->height),
		       sum((uint8_t *)f->virAddr + f->width * f->height,
			   f->width * f->height / 2));
		if (out && i == 0) {
			FILE *fp = fopen(out, "wb");

			if (fp) {
				fwrite((void *)f->virAddr, 1, f->size, fp);
				fclose(fp);
			}
		}
		IMP_FrameSource_ReleaseFrame(0, f);
	}

	IMP_FrameSource_DisableChn(0);
	IMP_FrameSource_DestroyChn(0);
	IMP_ISP_DisableSensor();
	IMP_ISP_DelSensor(&si);
	IMP_ISP_DisableTuning();
	IMP_ISP_Close();
	puts("t20cap done");
	fflush(stdout);
	/*
	 * Last: it cancels libimp's threads, which needs libgcc_s to be
	 * dlopen()able; the test root's uClibc cannot, so skip it with an
	 * argument of "nosysexit".
	 */
	if (!(argc > 3 && !strcmp(argv[3], "nosysexit")))
		IMP_System_Exit();
	return i == frames ? 0 : 2;
}
