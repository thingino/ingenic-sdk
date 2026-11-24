// SPDX-License-Identifier: GPL-2.0
/*
 * imp-enc: libimp's H.264 path: ISP, one frame source channel bound to an
 * encoder group, N encoded frames written to a file.
 *
 * usage: imp-enc <sensor> <i2c-addr> <width> <height> <frames> <out.h264>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_isp.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_osd.h>

extern int IMP_OSD_SetPoolSize(int size);
extern int IMP_Encoder_SetPoolSize(int size);

#define CHK(call) do { int r_ = (call); printf("%s = %d\n", #call, r_); fflush(stdout); if (r_ < 0) return 1; } while (0)

int main(int argc, char **argv)
{
	IMPSensorInfo si;
	IMPFSChnAttr attr;
	IMPEncoderCHNAttr ch;
	IMPCell fs = { DEV_ID_FS, 0, 0 }, enc = { DEV_ID_ENC, 0, 0 };
	int i, n, fd, frames = 0;
	unsigned long total = 0, total1 = 0;
	int sub = 0, frames1 = 0, subchn = 1;
	const char *outfile = "/tmp/out1.h264";

	setvbuf(stdout, NULL, _IOLBF, 0);
	if (argc < 7)
		return 2;
	n = atoi(argv[5]);
	memset(&si, 0, sizeof(si));
	snprintf(si.name, sizeof(si.name), "%s", argv[1]);
	si.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
	snprintf(si.i2c.type, sizeof(si.i2c.type), "%s", argv[1]);
	si.i2c.addr = strtol(argv[2], NULL, 0);

	memset(&attr, 0, sizeof(attr));
	attr.pixFmt = PIX_FMT_NV12;
	attr.outFrmRateNum = 25;
	attr.outFrmRateDen = 1;
	attr.nrVBs = getenv("NRVBS") ? atoi(getenv("NRVBS")) : 2;
	attr.type = FS_PHY_CHANNEL;
	attr.picWidth = atoi(argv[3]);
	attr.picHeight = atoi(argv[4]);

	memset(&ch, 0, sizeof(ch));
	ch.encAttr.enType = PT_H264;
	ch.encAttr.bufSize = attr.picWidth * attr.picHeight / 2;
	ch.encAttr.profile = -1;
	ch.encAttr.picWidth = attr.picWidth;
	ch.encAttr.picHeight = attr.picHeight;
	ch.rcAttr.outFrmRate.frmRateNum = 25;
	ch.rcAttr.outFrmRate.frmRateDen = 1;
	ch.rcAttr.maxGop = 50;
	ch.rcAttr.attrRcMode.rcMode = ENC_RC_MODE_CBR;
	ch.rcAttr.attrRcMode.attrH264Cbr.outBitRate = 2000 * (attr.picWidth * attr.picHeight) / (1280 * 720);
	ch.rcAttr.attrRcMode.attrH264Cbr.maxQp = 45;
	ch.rcAttr.attrRcMode.attrH264Cbr.minQp = 15;
	ch.rcAttr.attrRcMode.attrH264Cbr.frmQPStep = 3;
	ch.rcAttr.attrRcMode.attrH264Cbr.gopQPStep = 15;
	ch.rcAttr.attrHSkip.hSkipAttr.skipType = IMP_Encoder_STYPE_N1X;
	ch.rcAttr.attrHSkip.maxHSkipType = IMP_Encoder_STYPE_N1X;

	/* the vendor sample's order (sample_system_init) */
	IMP_OSD_SetPoolSize(512 * 1024);
	IMP_Encoder_SetPoolSize(attr.picWidth * attr.picHeight * 3 / 2 / 3);
	CHK(IMP_ISP_Open());
	CHK(IMP_ISP_AddSensor(&si));
	CHK(IMP_ISP_EnableSensor());
	CHK(IMP_System_Init());
	CHK(IMP_ISP_EnableTuning());
	CHK(IMP_ISP_Tuning_SetContrast(128));
	CHK(IMP_ISP_Tuning_SetSharpness(128));
	CHK(IMP_ISP_Tuning_SetSaturation(128));
	CHK(IMP_ISP_Tuning_SetBrightness(128));
	CHK(IMP_ISP_Tuning_SetISPRunningMode(IMPISP_RUNNING_MODE_DAY));
	CHK(IMP_FrameSource_CreateChn(0, &attr));
	CHK(IMP_FrameSource_SetChnAttr(0, &attr));
	CHK(IMP_Encoder_CreateGroup(0));
	CHK(IMP_Encoder_CreateChn(0, &ch));
	CHK(IMP_Encoder_RegisterChn(0, 0));
	CHK(IMP_System_Bind(&fs, &enc));
	if (getenv("SUB")) {
		/* a second, scaled channel with its own H.264 encoder */
		IMPFSChnAttr a1 = attr;
		IMPEncoderCHNAttr c1 = ch;
		IMPCell fs1 = { DEV_ID_FS, 1, 0 }, enc1 = { DEV_ID_ENC, 1, 0 };

		a1.picWidth = 640;
		a1.picHeight = 360;
		a1.scaler.enable = 1;
		a1.scaler.outwidth = 640;
		a1.scaler.outheight = 360;
		c1.encAttr.picWidth = 640;
		c1.encAttr.picHeight = 360;
		c1.encAttr.bufSize = 640 * 360 / 2;
		c1.rcAttr.attrRcMode.attrH264Cbr.outBitRate = 500;
		if (!strcmp(getenv("SUB"), "jpeg")) {
			/* a JPEG channel is numbered 3 + its frame source */
			c1.encAttr.enType = PT_JPEG;
			c1.encAttr.profile = 2;
			memset(&c1.rcAttr, 0, sizeof(c1.rcAttr));
			subchn = 4;
			outfile = "/tmp/out.jpg";
		}
		CHK(IMP_FrameSource_CreateChn(1, &a1));
		CHK(IMP_FrameSource_SetChnAttr(1, &a1));
		CHK(IMP_Encoder_CreateGroup(1));
		CHK(IMP_Encoder_CreateChn(subchn, &c1));
		CHK(IMP_Encoder_RegisterChn(1, subchn));
		CHK(IMP_System_Bind(&fs1, &enc1));
		CHK(IMP_FrameSource_EnableChn(1));
		CHK(IMP_Encoder_StartRecvPic(subchn));
		sub = 1;
	}
	CHK(IMP_FrameSource_EnableChn(0));
	CHK(IMP_Encoder_StartRecvPic(0));

	fd = open(argv[6], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	for (i = 0; i < n; i++) {
		IMPEncoderStream st;
		unsigned int p;

		if (IMP_Encoder_PollingStream(0, 5000) < 0) {
			printf("poll %d failed\n", i);
			continue;
		}
		if (IMP_Encoder_GetStream(0, &st, 1) < 0) {
			printf("GetStream %d failed\n", i);
			continue;
		}
		for (p = 0; p < st.packCount; p++) {
			write(fd, (void *)(unsigned long)st.pack[p].virAddr, st.pack[p].length);
			total += st.pack[p].length;
			if (p == 0 && frames < 4)
				printf("frame %d pack0 len %u type %u first bytes %02x %02x %02x %02x %02x\n", frames,
				       st.pack[p].length, st.pack[p].dataType.h264Type,
				       ((unsigned char *)(unsigned long)st.pack[p].virAddr)[0],
				       ((unsigned char *)(unsigned long)st.pack[p].virAddr)[1],
				       ((unsigned char *)(unsigned long)st.pack[p].virAddr)[2],
				       ((unsigned char *)(unsigned long)st.pack[p].virAddr)[3],
				       ((unsigned char *)(unsigned long)st.pack[p].virAddr)[4]);
		}
		frames++;
		IMP_Encoder_ReleaseStream(0, &st);
	}
	for (i = 0; sub && i < n; i++) {
		IMPEncoderStream st;
		unsigned int p;

		if (IMP_Encoder_PollingStream(subchn, 5000) < 0 || IMP_Encoder_GetStream(subchn, &st, 1) < 0) {
			printf("sub %d failed\n", i);
			continue;
		}
		for (p = 0; p < st.packCount; p++) {
			if (i == 0) {
				int jf = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				write(jf, (void *)(unsigned long)st.pack[p].virAddr, st.pack[p].length);
				close(jf);
			}
			total1 += st.pack[p].length;
		}
		frames1++;
		IMP_Encoder_ReleaseStream(subchn, &st);
	}
	if (sub)
		printf("%s: substream 640x360 %d frames, %lu bytes\n", frames1 == n && total1 ? "PASS" : "FAIL", frames1, total1);
	close(fd);
	printf("%s: %d encoded frames, %lu bytes\n", frames == n && total ? "PASS" : "FAIL", frames, total);
	if (sub) {
		IMPCell f1 = { DEV_ID_FS, 1, 0 }, e1 = { DEV_ID_ENC, 1, 0 };

		IMP_Encoder_StopRecvPic(subchn);
		IMP_System_UnBind(&f1, &e1);
		IMP_FrameSource_DisableChn(1);
		IMP_FrameSource_DestroyChn(1);
		IMP_Encoder_UnRegisterChn(subchn);
		IMP_Encoder_DestroyChn(subchn);
		IMP_Encoder_DestroyGroup(1);
	}
	IMP_Encoder_StopRecvPic(0);
	IMP_System_UnBind(&fs, &enc);
	IMP_FrameSource_DisableChn(0);
	IMP_FrameSource_DestroyChn(0);
	IMP_Encoder_UnRegisterChn(0);
	IMP_Encoder_DestroyChn(0);
	IMP_Encoder_DestroyGroup(0);
	IMP_System_Exit();
	IMP_ISP_DisableTuning();
	IMP_ISP_DisableSensor();
	IMP_ISP_DelSensor(&si);
	IMP_ISP_Close();
	return !(frames == n && total);
}
