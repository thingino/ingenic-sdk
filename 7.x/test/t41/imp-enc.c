// SPDX-License-Identifier: GPL-2.0
/*
 * imp-enc: libimp encoder path: ISP + sensor, FrameSource ch0 (sensor size)
 * and ch1 (scaled), Encoder groups bound to them, H.264 on both, a JPEG
 * snapshot channel per group.
 *
 * usage: imp-enc <outdir> <frames> [width height [subw subh]]
 * The sensor is gc4023 at 0x29 (T41NQ), override with SENSOR and I2CADDR,
 * NRVB sets the main channel buffer count (a 22 MiB rmem fits one 2560x1440).
 * IMP_RST, IMP_BUS and IMP_MCLK (T41, T40) are the sensor reset gpio, its i2c adapter and its MCLK source.
 * CODEC=h265 selects H.265 for the two video channels.
 * Writes <outdir>/stream-0.h264, stream-1.h264, snap-0-N.jpg, snap-1-N.jpg.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_isp.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>

#define CHK(call) do { int r_ = (call); printf("%s = %d\n", #call, r_); fflush(stdout); if (r_ < 0) exit(1); } while (0)

#define TD(call) do { printf("teardown %s\n", #call); fflush(stdout); call; } while (0)

static const char *outdir;
static const char *sensor = "gc4023";
static int i2caddr = 0x29;
static int nrvb = 2;
static int nframes;
static IMPEncoderProfile vprof = IMP_ENC_PROFILE_AVC_MAIN;

struct job {
	int chn;
	unsigned frames, bytes, idr, timeouts;
};

static int save_stream(int fd, IMPEncoderStream *s)
{
	unsigned i;

	for (i = 0; i < s->packCount; i++) {
		IMPEncoderPack *p = &s->pack[i];

		if (p->length &&
		    write(fd, (void *)(s->virAddr + p->offset), p->length) != (ssize_t)p->length)
			return -1;
	}
	return 0;
}

static unsigned stream_bytes(IMPEncoderStream *s)
{
	unsigned i, n = 0;

	for (i = 0; i < s->packCount; i++)
		n += s->pack[i].length;
	return n;
}

static void *video_thread(void *arg)
{
	struct job *j = arg;
	char path[128];
	int fd;

	snprintf(path, sizeof(path), "%s/stream-%d.h264", outdir, j->chn);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		perror(path);
		return NULL;
	}
	if (IMP_Encoder_StartRecvPic(j->chn) < 0) {
		printf("StartRecvPic(%d) failed\n", j->chn);
		return NULL;
	}
	while (j->frames < (unsigned)nframes && j->timeouts < 30) {
		IMPEncoderStream st;
		unsigned i;

		if (IMP_Encoder_PollingStream(j->chn, 2000) < 0) {
			j->timeouts++;
			continue;
		}
		if (IMP_Encoder_GetStream(j->chn, &st, 1) < 0) {
			printf("GetStream(%d) failed\n", j->chn);
			break;
		}
		for (i = 0; i < st.packCount; i++)
			if (st.pack[i].nalType.h264NalType == 5)
				j->idr++;
		j->bytes += stream_bytes(&st);
		if (save_stream(fd, &st) < 0)
			printf("write(%d) failed: %s\n", j->chn, strerror(errno));
		IMP_Encoder_ReleaseStream(j->chn, &st);
		j->frames++;
	}
	IMP_Encoder_StopRecvPic(j->chn);
	close(fd);
	return NULL;
}

static int snap(int chn, int n, int grp)
{
	IMPEncoderStream st;
	char path[128];
	int fd;

	snprintf(path, sizeof(path), "%s/snap-%d-%d.jpg", outdir, grp, n);
	CHK(IMP_Encoder_StartRecvPic(chn));
	if (IMP_Encoder_PollingStream(chn, 20000) < 0) {
		printf("snap %d: polling timeout\n", chn);
		IMP_Encoder_StopRecvPic(chn);
		return -1;
	}
	CHK(IMP_Encoder_GetStream(chn, &st, 1));
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0 || save_stream(fd, &st) < 0)
		printf("snap write failed: %s\n", strerror(errno));
	printf("snap %s: %u bytes in %u packs\n", path, stream_bytes(&st), st.packCount);
	close(fd);
	IMP_Encoder_ReleaseStream(chn, &st);
	return IMP_Encoder_StopRecvPic(chn);
}

static void enc_chn(int chn, IMPEncoderProfile prof, IMPEncoderRcMode rc, int w, int h, int bitrate, int group)
{
	IMPEncoderChnAttr a;

	memset(&a, 0, sizeof(a));
	CHK(IMP_Encoder_SetDefaultParam(&a, prof, rc, w, h, 25, 1,
					rc == IMP_ENC_RC_MODE_FIXQP ? 0 : 50, rc == IMP_ENC_RC_MODE_FIXQP ? 0 : 2,
					rc == IMP_ENC_RC_MODE_FIXQP ? 25 : -1, bitrate));
	CHK(IMP_Encoder_CreateChn(chn, &a));
	CHK(IMP_Encoder_RegisterChn(group, chn));
}

int main(int argc, char **argv)
{
	int w = 2560, h = 1440, sw = 640, sh = 360, i;
	IMPSensorInfo si;
	IMPFSChnAttr fa[2];
	IMPCell fs[2], enc[2];
	pthread_t tid[2];
	struct job job[2] = { { .chn = 0 }, { .chn = 1 } };

	if (argc < 3)
		return 2;
	if (getenv("SENSOR"))
		sensor = getenv("SENSOR");
	if (getenv("I2CADDR"))
		i2caddr = strtol(getenv("I2CADDR"), NULL, 0);
	if (getenv("NRVB"))
		nrvb = atoi(getenv("NRVB"));
	outdir = argv[1];
	nframes = atoi(argv[2]);
	if (argc >= 5) {
		w = atoi(argv[3]);
		h = atoi(argv[4]);
	}
	if (argc >= 7) {
		sw = atoi(argv[5]);
		sh = atoi(argv[6]);
	}

	if (getenv("CODEC") && !strcmp(getenv("CODEC"), "h265"))
		vprof = IMP_ENC_PROFILE_HEVC_MAIN;

	memset(&si, 0, sizeof(si));
	snprintf(si.name, sizeof(si.name), "%s", sensor);
	si.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
	snprintf(si.i2c.type, sizeof(si.i2c.type), "%s", sensor);
	si.i2c.addr = i2caddr;
	if (getenv("IMP_BUS"))
		si.i2c.i2c_adapter_id = atoi(getenv("IMP_BUS"));
	si.rst_gpio = getenv("IMP_RST") ? atoi(getenv("IMP_RST")) : -1;
	si.pwdn_gpio = si.power_gpio = -1;
	si.video_interface = IMPISP_SENSOR_VI_MIPI_CSI0;
	si.mclk = getenv("IMP_MCLK") ? (IMPSensorMclk)atoi(getenv("IMP_MCLK")) : IMPISP_SENSOR_MCLK0;

	memset(fa, 0, sizeof(fa));
	for (i = 0; i < 2; i++) {
		fa[i].pixFmt = PIX_FMT_NV12;
		fa[i].outFrmRateNum = 25;
		fa[i].outFrmRateDen = 1;
		fa[i].nrVBs = i ? 2 : nrvb;
		fa[i].type = FS_PHY_CHANNEL;
		fa[i].picWidth = i ? sw : w;
		fa[i].picHeight = i ? sh : h;
		if (i) {
			fa[i].scaler.enable = 1;
			fa[i].scaler.outwidth = sw;
			fa[i].scaler.outheight = sh;
		}
		fa[i].crop.width = w;
		fa[i].crop.height = h;
		fs[i] = (IMPCell){ DEV_ID_FS, i, 0 };
		enc[i] = (IMPCell){ DEV_ID_ENC, i, 0 };
	}

	CHK(IMP_ISP_Open());
	CHK(IMP_ISP_AddSensor(IMPVI_MAIN, &si));
	CHK(IMP_ISP_EnableSensor(IMPVI_MAIN, &si));
	CHK(IMP_System_Init());
	CHK(IMP_ISP_EnableTuning());
	for (i = 0; i < 2; i++) {
		CHK(IMP_FrameSource_CreateChn(i, &fa[i]));
		CHK(IMP_FrameSource_SetChnAttr(i, &fa[i]));
		CHK(IMP_Encoder_CreateGroup(i));
	}
	enc_chn(0, vprof, IMP_ENC_RC_MODE_CBR, w, h, 4000, 0);
	enc_chn(1, vprof, IMP_ENC_RC_MODE_CBR, sw, sh, 800, 1);
	enc_chn(4, IMP_ENC_PROFILE_JPEG, IMP_ENC_RC_MODE_FIXQP, w, h, 0, 0);
	enc_chn(5, IMP_ENC_PROFILE_JPEG, IMP_ENC_RC_MODE_FIXQP, sw, sh, 0, 1);
	for (i = 0; i < 2; i++)
		CHK(IMP_System_Bind(&fs[i], &enc[i]));
	for (i = 0; i < 2; i++)
		CHK(IMP_FrameSource_EnableChn(i));

	for (i = 0; i < 2; i++)
		pthread_create(&tid[i], NULL, video_thread, &job[i]);
	sleep(3);
	snap(4, 0, 0);
	snap(5, 0, 1);
	snap(4, 1, 0);
	for (i = 0; i < 2; i++)
		pthread_join(tid[i], NULL);

	for (i = 0; i < 2; i++)
		printf("STREAM %d: %u frames, %u bytes, %u idr, %u poll timeouts\n", i,
		       job[i].frames, job[i].bytes, job[i].idr, job[i].timeouts);

	for (i = 0; i < 2; i++)
		TD(IMP_FrameSource_DisableChn(i));
	for (i = 0; i < 2; i++)
		TD(IMP_System_UnBind(&fs[i], &enc[i]));
	TD(IMP_Encoder_UnRegisterChn(0));
	TD(IMP_Encoder_UnRegisterChn(1));
	TD(IMP_Encoder_UnRegisterChn(4));
	TD(IMP_Encoder_UnRegisterChn(5));
	TD(IMP_Encoder_DestroyChn(0));
	TD(IMP_Encoder_DestroyChn(1));
	TD(IMP_Encoder_DestroyChn(4));
	TD(IMP_Encoder_DestroyChn(5));
	TD(IMP_Encoder_DestroyGroup(0));
	TD(IMP_Encoder_DestroyGroup(1));
	for (i = 0; i < 2; i++)
		TD(IMP_FrameSource_DestroyChn(i));
	TD(IMP_System_Exit());
	TD(IMP_ISP_DisableSensor(IMPVI_MAIN));
	TD(IMP_ISP_DelSensor(IMPVI_MAIN, &si));
	TD(IMP_ISP_Close());
	printf("DONE\n");
	return job[0].frames < (unsigned)nframes || job[1].frames < (unsigned)nframes;
}
