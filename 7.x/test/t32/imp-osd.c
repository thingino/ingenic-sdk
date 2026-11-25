// SPDX-License-Identifier: GPL-2.0
/*
 * imp-osd: libimp OSD on the encoder's main channel. FrameSource ch0 ->
 * OSD group 0 -> Encoder group 0 (H.264 Main). Four regions, hidden for the
 * first frames (control) and shown from <show_at> on:
 *   COVER  solid red rectangle                    (IPU, mask layer)
 *   PIC    256x64 BGRA colour bars                (IPU, bitmap layer)
 *   PIC    text-like 5x7 glyphs, white on black   (IPU, bitmap layer)
 *   PIC    200x100 solid orange 0xffff8000        (IPU, bitmap layer)
 *   RECT   green outline, 4 px                    (drawn by the CPU)
 * At <update_at> the bars bitmap is replaced (IMP_OSD_UpdateRgnAttrData).
 * Writes <outdir>/stream-0.h264, regions.txt and the bitmaps as raw BGRA.
 *
 * usage: imp-osd <outdir> <frames> [show_at update_at]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_isp.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_osd.h>

extern int IMP_OSD_SetPoolSize(int size);

#define CHK(call) do { int r_ = (call); printf("%s = %d\n", #call, r_); fflush(stdout); if (r_ < 0) exit(1); } while (0)
#define TD(call) do { printf("teardown %s\n", #call); fflush(stdout); call; } while (0)

#define W 2560
#define H 1440
#define GRP 0

#define BARS_W 256
#define BARS_H 64
#define GLYPH_SCALE 6
#define TXT_W (6 * 6 * GLYPH_SCALE)
#define TXT_H (7 * GLYPH_SCALE)

#define SOL_W 200
#define SOL_H 100

static const char *outdir;

/* B, G, R, A */
static void px(uint8_t *p, uint32_t argb)
{
	p[0] = argb;
	p[1] = argb >> 8;
	p[2] = argb >> 16;
	p[3] = argb >> 24;
}

static void make_bars(uint8_t *buf, int flip)
{
	static const uint32_t col[4] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
	int x, y;

	for (y = 0; y < BARS_H; y++)
		for (x = 0; x < BARS_W; x++)
			px(buf + 4 * (y * BARS_W + x), col[flip ? 3 - x / 64 : x / 64]);
}

static const char *glyphs[] = {
	/* O */ ".###." "#...#" "#...#" "#...#" "#...#" "#...#" ".###.",
	/* S */ ".####" "#...." "#...." ".###." "....#" "....#" "####.",
	/* D */ "####." "#...#" "#...#" "#...#" "#...#" "#...#" "####.",
	/* space */ "     " "     " "     " "     " "     " "     " "     ",
	/* 3 */ "####." "....#" "....#" ".###." "....#" "....#" "####.",
	/* 1 */ "..#.." ".##.." "..#.." "..#.." "..#.." "..#.." ".###.",
};

static void make_text(uint8_t *buf)
{
	int g, r, c, y, x;

	for (y = 0; y < TXT_H; y++)
		for (x = 0; x < TXT_W; x++)
			px(buf + 4 * (y * TXT_W + x), 0xff000000);
	for (g = 0; g < 6; g++)
		for (r = 0; r < 7; r++)
			for (c = 0; c < 5; c++) {
				char ch = glyphs[g][r * 5 + c];

				if (ch != '#')
					continue;
				for (y = 0; y < GLYPH_SCALE; y++)
					for (x = 0; x < GLYPH_SCALE; x++)
						px(buf + 4 * ((r * GLYPH_SCALE + y) * TXT_W +
							      (g * 6 + c) * GLYPH_SCALE + x), 0xffffffff);
			}
}

static void dump(const char *name, const void *p, size_t n)
{
	char path[160];
	int fd;

	snprintf(path, sizeof(path), "%s/%s", outdir, name);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0 || write(fd, p, n) != (ssize_t)n)
		perror(path);
	if (fd >= 0)
		close(fd);
}

static IMPRgnHandle mkrgn(IMPOSDRgnAttr *a, int layer, int galpha, int fgalpha)
{
	IMPOSDGrpRgnAttr g;
	IMPRgnHandle h = IMP_OSD_CreateRgn(NULL);

	if (h == INVHANDLE) {
		printf("CreateRgn failed\n");
		exit(1);
	}
	CHK(IMP_OSD_RegisterRgn(h, GRP, NULL));
	CHK(IMP_OSD_SetRgnAttr(h, a));
	memset(&g, 0, sizeof(g));
	g.show = 0;
	g.gAlphaEn = galpha;
	g.fgAlhpa = fgalpha;
	g.layer = layer;
	g.scalex = g.scaley = 1;
	CHK(IMP_OSD_SetGrpRgnAttr(h, GRP, &g));
	return h;
}

int main(int argc, char **argv)
{
	static uint8_t solid[SOL_W * SOL_H * 4], bars[BARS_W * BARS_H * 4], bars2[BARS_W * BARS_H * 4], txt[TXT_W * TXT_H * 4];
	IMPSensorInfo si;
	IMPFSChnAttr fa;
	IMPCell fs = { DEV_ID_FS, 0, 0 }, osd = { DEV_ID_OSD, GRP, 0 }, enc = { DEV_ID_ENC, 0, 0 };
	IMPRgnHandle hcov, hbars, htxt, hrect, hsol;
	IMPOSDRgnAttr a;
	IMPEncoderCHNAttr ea;
	int nframes, show_at = 30, update_at = 80, shown = 0, updated = 0, fd, i;
	unsigned frames = 0, timeouts = 0, bytes = 0;
	char path[160];
	FILE *f;

	if (argc < 3)
		return 2;
	outdir = argv[1];
	nframes = atoi(argv[2]);
	if (argc >= 5) {
		show_at = atoi(argv[3]);
		update_at = atoi(argv[4]);
	}

	make_bars(bars, 0);
	make_bars(bars2, 1);
	make_text(txt);
	for (i = 0; i < SOL_W * SOL_H; i++)
		px(solid + 4 * i, 0xffff8000);
	dump("bars-a.bgra", bars, sizeof(bars));
	dump("bars-b.bgra", bars2, sizeof(bars2));
	dump("text.bgra", txt, sizeof(txt));
	snprintf(path, sizeof(path), "%s/regions.txt", outdir);
	f = fopen(path, "w");
	if (f) {
		fprintf(f, "frame %d %d\nshow_at %d\nupdate_at %d\n", W, H, show_at, update_at);
		fprintf(f, "cover 200 200 400 200 ffff0000\nbars 1000 200 %d %d\ntext 200 600 %d %d\n",
			BARS_W, BARS_H, TXT_W, TXT_H);
		fprintf(f, "solid 600 900 %d %d ffff8000\n", SOL_W, SOL_H);
		fprintf(f, "rect 1500 600 400 200 4 ff00ff00\n");
		fclose(f);
	}

	memset(&si, 0, sizeof(si));
	snprintf(si.name, sizeof(si.name), "sc4336p");
	si.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C;
	snprintf(si.i2c.type, sizeof(si.i2c.type), "sc4336p");
	si.i2c.addr = 0x30;
	si.rst_gpio = si.pwdn_gpio = si.power_gpio = si.switch_gpio = -1;
	si.video_interface = IMPISP_SENSOR_VI_MIPI_CSI0;
	si.mclk = IMPISP_SENSOR_MCLK0;
	si.fps.num = 25;
	si.fps.den = 1;

	memset(&fa, 0, sizeof(fa));
	fa.pixFmt = PIX_FMT_NV12;
	fa.outFrmRateNum = 25;
	fa.outFrmRateDen = 1;
	fa.nrVBs = getenv("NRVB") ? atoi(getenv("NRVB")) : 2;
	fa.type = FS_PHY_CHANNEL;
	fa.picWidth = W;
	fa.picHeight = H;
	fa.crop.width = W;
	fa.crop.height = H;

	IMP_OSD_SetPoolSize(512 * 1024);
	CHK(IMP_ISP_Open());
	CHK(IMP_ISP_AddSensor(IMPVI_MAIN, &si));
	CHK(IMP_ISP_EnableSensor(IMPVI_MAIN, &si));
	CHK(IMP_System_Init());
	CHK(IMP_ISP_EnableTuning());
	CHK(IMP_FrameSource_CreateChn(0, &fa));
	CHK(IMP_FrameSource_SetChnAttr(0, &fa));
	CHK(IMP_Encoder_CreateGroup(0));
	memset(&ea, 0, sizeof(ea));
	CHK(IMP_Encoder_SetDefaultParam(&ea, IMP_ENC_PROFILE_AVC_MAIN, ENC_RC_MODE_CBR, W, H, 25, 1, 50, 2, -1, 4000));
	CHK(IMP_Encoder_CreateChn(0, &ea));
	CHK(IMP_Encoder_RegisterChn(0, 0));

	CHK(IMP_OSD_CreateGroup(GRP));

	memset(&a, 0, sizeof(a));
	a.type = OSD_REG_COVER;
	a.rect.p0.x = 200; a.rect.p0.y = 200; a.rect.p1.x = 599; a.rect.p1.y = 399;
	a.fmt = PIX_FMT_BGRA;
	a.data.coverData.color = OSD_IPU_RED;
	hcov = mkrgn(&a, 1, 0, 0xff);

	memset(&a, 0, sizeof(a));
	a.type = OSD_REG_PIC;
	a.rect.p0.x = 1000; a.rect.p0.y = 200;
	a.rect.p1.x = 1000 + BARS_W - 1; a.rect.p1.y = 200 + BARS_H - 1;
	a.fmt = PIX_FMT_BGRA;
	a.data.picData.pData = bars;
	hbars = mkrgn(&a, 2, 1, 0xff);

	memset(&a, 0, sizeof(a));
	a.type = OSD_REG_PIC;
	a.rect.p0.x = 200; a.rect.p0.y = 600;
	a.rect.p1.x = 200 + TXT_W - 1; a.rect.p1.y = 600 + TXT_H - 1;
	a.fmt = PIX_FMT_BGRA;
	a.data.picData.pData = txt;
	htxt = mkrgn(&a, 3, 1, 0xff);

	memset(&a, 0, sizeof(a));
	a.type = OSD_REG_PIC;
	a.rect.p0.x = 600; a.rect.p0.y = 900;
	a.rect.p1.x = 600 + SOL_W - 1; a.rect.p1.y = 900 + SOL_H - 1;
	a.fmt = PIX_FMT_BGRA;
	a.data.picData.pData = solid;
	hsol = mkrgn(&a, 5, 1, 0xff);

	memset(&a, 0, sizeof(a));
	a.type = OSD_REG_RECT;
	a.rect.p0.x = 1500; a.rect.p0.y = 600; a.rect.p1.x = 1899; a.rect.p1.y = 799;
	a.fmt = PIX_FMT_MONOWHITE;
	a.data.lineRectData.color = OSD_IPU_GREEN;
	a.data.lineRectData.linewidth = 4;
	hrect = mkrgn(&a, 4, 0, 0);

	CHK(IMP_OSD_Start(GRP));
	CHK(IMP_System_Bind(&fs, &osd));
	CHK(IMP_System_Bind(&osd, &enc));
	CHK(IMP_FrameSource_EnableChn(0));

	snprintf(path, sizeof(path), "%s/stream-0.h264", outdir);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		perror(path);
		return 1;
	}
	CHK(IMP_Encoder_StartRecvPic(0));
	while (frames < (unsigned)nframes && timeouts < 30) {
		IMPEncoderStream st;
		unsigned k;

		if (!shown && frames >= (unsigned)show_at) {
			IMP_OSD_ShowRgn(hcov, GRP, 1);
			IMP_OSD_ShowRgn(hbars, GRP, 1);
			IMP_OSD_ShowRgn(htxt, GRP, 1);
			IMP_OSD_ShowRgn(hrect, GRP, 1);
			IMP_OSD_ShowRgn(hsol, GRP, 1);
			shown = 1;
			printf("SHOW at frame %u\n", frames);
			fflush(stdout);
		}
		if (shown && !updated && frames >= (unsigned)update_at) {
			IMPOSDRgnAttrData d;

			d.picData.pData = bars2;
			CHK(IMP_OSD_UpdateRgnAttrData(hbars, &d));
			updated = 1;
			printf("UPDATE at frame %u\n", frames);
			fflush(stdout);
		}
		if (IMP_Encoder_PollingStream(0, 2000) < 0) {
			timeouts++;
			continue;
		}
		if (IMP_Encoder_GetStream(0, &st, 1) < 0)
			break;
		for (k = 0; k < st.packCount; k++) {
			IMPEncoderPack *p = &st.pack[k];

			if (!p->length)
				continue;
			bytes += p->length;
			if (write(fd, (void *)p->virAddr, p->length) < 0)
				perror("write");
		}
		IMP_Encoder_ReleaseStream(0, &st);
		frames++;
	}
	close(fd);
	printf("STREAM 0: %u frames, %u bytes, %u poll timeouts\n", frames, bytes, timeouts);
	fflush(stdout);

	TD(IMP_Encoder_StopRecvPic(0));
	TD(IMP_FrameSource_DisableChn(0));
	TD(IMP_System_UnBind(&osd, &enc));
	TD(IMP_System_UnBind(&fs, &osd));
	for (i = 0; i < 5; i++) {
		IMPRgnHandle h = i == 0 ? hcov : i == 1 ? hbars : i == 2 ? htxt : i == 3 ? hsol : hrect;

		TD(IMP_OSD_ShowRgn(h, GRP, 0));
		TD(IMP_OSD_UnRegisterRgn(h, GRP));
		TD(IMP_OSD_DestroyRgn(h));
	}
	TD(IMP_OSD_DestroyGroup(GRP));
	TD(IMP_Encoder_UnRegisterChn(0));
	TD(IMP_Encoder_DestroyChn(0));
	TD(IMP_Encoder_DestroyGroup(0));
	TD(IMP_FrameSource_DestroyChn(0));
	TD(IMP_System_Exit());
	TD(IMP_ISP_DisableSensor(IMPVI_MAIN));
	TD(IMP_ISP_DelSensor(IMPVI_MAIN, &si));
	TD(IMP_ISP_Close());
	printf("DONE\n");
	return frames < (unsigned)nframes;
}
