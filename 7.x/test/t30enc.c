// SPDX-License-Identifier: GPL-2.0
/*
 * t30enc: libimp H.264 (or H.265 with CODEC=h265) encode of the frame
 * source's channel 0 through the T30 VPU (/dev/soc_vpu). Writes the stream to a file and prints the size and
 * type of each access unit.
 *
 *   t30enc [frames] [out.h264]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_isp.h>

/* SENSOR, SADDR, SW and SH in the environment pick another sensor */
static const char *SENSOR_NAME = "jxf23";
static int SENSOR_ADDR = 0x40;
static int W = 1920;
static int H = 1080;

static void sensor_from_env(void)
{
	if (getenv("SENSOR"))
		SENSOR_NAME = getenv("SENSOR");
	if (getenv("SADDR"))
		SENSOR_ADDR = strtol(getenv("SADDR"), NULL, 0);
	if (getenv("SW"))
		W = atoi(getenv("SW"));
	if (getenv("SH"))
		H = atoi(getenv("SH"));
}

extern int IMP_Encoder_SetPoolSize(int size);

#define STEP(x) do { int r_ = (x); if (r_ < 0) { printf("%s failed %d\n", #x, r_); return 1; } } while (0)

int main(int argc, char **argv)
{
	int frames = argc > 1 ? atoi(argv[1]) : 10;
	const char *out = argc > 2 ? argv[2] : "/tmp/out.h264";
	IMPSensorInfo si;
	IMPFSChnAttr fa;
	IMPEncoderCHNAttr ea;
	IMPCell fs = { DEV_ID_FS, 0, 0 };
	IMPCell enc = { DEV_ID_ENC, 0, 0 };
	int fd, i, got = 0;

	sensor_from_env();
	memset(&si, 0, sizeof(si));
	strcpy(si.name, SENSOR_NAME);
	si.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
	strcpy(si.i2c.type, SENSOR_NAME);
	si.i2c.addr = SENSOR_ADDR;

	IMP_Encoder_SetPoolSize(1 * 1024 * 1024);
	STEP(IMP_ISP_Open());
	STEP(IMP_ISP_AddSensor(&si));
	STEP(IMP_ISP_EnableSensor());
	STEP(IMP_System_Init());
	STEP(IMP_ISP_EnableTuning());
	STEP(IMP_ISP_Tuning_SetSensorFPS(25, 1));
	puts("isp up");

	memset(&fa, 0, sizeof(fa));
	fa.pixFmt = PIX_FMT_NV12;
	fa.outFrmRateNum = 25;
	fa.outFrmRateDen = 1;
	fa.nrVBs = 3;
	fa.type = FS_PHY_CHANNEL;
	fa.picWidth = W;
	fa.picHeight = H;
	STEP(IMP_FrameSource_CreateChn(0, &fa));
	STEP(IMP_FrameSource_SetChnAttr(0, &fa));

	STEP(IMP_Encoder_CreateGroup(0));
	memset(&ea, 0, sizeof(ea));
	/* CODEC=h265 in the environment asks for H.265 (radix) */
	int h265 = getenv("CODEC") && !strcmp(getenv("CODEC"), "h265");

	ea.encAttr.enType = h265 ? PT_H265 : PT_H264;
	ea.encAttr.bufSize = 0;
	ea.encAttr.profile = 1;
	ea.encAttr.picWidth = W;
	ea.encAttr.picHeight = H;
	ea.rcAttr.outFrmRate.frmRateNum = 25;
	ea.rcAttr.outFrmRate.frmRateDen = 1;
	ea.rcAttr.maxGop = 50;
	ea.rcAttr.attrRcMode.rcMode = ENC_RC_MODE_FIXQP;
	if (h265)
		ea.rcAttr.attrRcMode.attrH265FixQp.qp = 38;
	else
		ea.rcAttr.attrRcMode.attrH264FixQp.qp = 38;
	ea.rcAttr.attrHSkip.hSkipAttr.skipType = IMP_Encoder_STYPE_N1X;
	ea.rcAttr.attrHSkip.maxHSkipType = IMP_Encoder_STYPE_N1X;
	STEP(IMP_Encoder_CreateChn(0, &ea));
	STEP(IMP_Encoder_RegisterChn(0, 0));
	STEP(IMP_System_Bind(&fs, &enc));
	STEP(IMP_FrameSource_EnableChn(0));
	puts("encoder bound");
	STEP(IMP_Encoder_StartRecvPic(0));

	fd = open(out, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) { puts("open out failed"); return 1; }
	for (i = 0; i < frames; i++) {
		IMPEncoderStream st;
		int j;
		size_t total = 0;

		if (IMP_Encoder_PollingStream(0, 5000) < 0) {
			printf("poll %d timeout\n", i);
			continue;
		}
		if (IMP_Encoder_GetStream(0, &st, 1) < 0) {
			printf("GetStream %d failed\n", i);
			break;
		}
		for (j = 0; j < (int)st.packCount; j++) {
			write(fd, (void *)st.pack[j].virAddr, st.pack[j].length);
			total += st.pack[j].length;
		}
		printf("au %d: %d packs %zu bytes\n", i, (int)st.packCount, total);
		got++;
		IMP_Encoder_ReleaseStream(0, &st);
	}
	close(fd);
	IMP_Encoder_StopRecvPic(0);
	printf("t30enc done: %d/%d\n", got, frames);
	return got == frames ? 0 : 2;
}
