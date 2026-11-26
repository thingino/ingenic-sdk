// SPDX-License-Identifier: GPL-2.0
/*
 * imp-ao: play a generated sine through libimp's IMP_AO.
 *
 * usage: imp-ao [seconds] [hz] [rate] [amplitude]
 * AO_VOL and AO_GAIN override the default volume (60) and gain (28).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <imp/imp_log.h>
#include <imp/imp_audio.h>

#define CHK(call) do { int r_ = (call); printf("%s = %d\n", #call, r_); fflush(stdout); if (r_ < 0) return 1; } while (0)

int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 4;
	double hz = argc > 2 ? atof(argv[2]) : 1000;
	int rate = argc > 3 ? atoi(argv[3]) : 16000;
	double amp = argc > 4 ? atof(argv[4]) : 0.5;
	int per = rate / 25;
	int total = secs * rate, done = 0, n = 0;
	IMPAudioIOAttr attr;
	IMPAudioFrame frm;
	int16_t *buf = malloc(per * 2);
	/* A table keeps the guest's soft-float sin() off the playback path */
	static int16_t tab[4096];
	double ph = 0, step = 4096.0 * hz / rate;
	int t;

	for (t = 0; t < 4096; t++)
		tab[t] = (int16_t)(32767 * amp * sin(2 * M_PI * t / 4096));

	memset(&attr, 0, sizeof(attr));
	attr.samplerate = rate;
	attr.bitwidth = AUDIO_BIT_WIDTH_16;
	attr.soundmode = AUDIO_SOUND_MODE_MONO;
	attr.frmNum = 20;
	attr.numPerFrm = per;
	attr.chnCnt = 1;
	CHK(IMP_AO_SetPubAttr(0, &attr));
	CHK(IMP_AO_Enable(0));
	CHK(IMP_AO_EnableChn(0, 0));
	CHK(IMP_AO_SetVol(0, 0, getenv("AO_VOL") ? atoi(getenv("AO_VOL")) : 60));
	CHK(IMP_AO_SetGain(0, 0, getenv("AO_GAIN") ? atoi(getenv("AO_GAIN")) : 28));

	while (done < total) {
		int i;

		for (i = 0; i < per; i++) {
			buf[i] = tab[(int)ph & 4095];
			ph += step;
			if (ph >= 4096)
				ph -= 4096;
		}
		memset(&frm, 0, sizeof(frm));
		frm.bitwidth = AUDIO_BIT_WIDTH_16;
		frm.soundmode = AUDIO_SOUND_MODE_MONO;
		frm.virAddr = (uint32_t *)buf;
		frm.len = per * 2;
		if (IMP_AO_SendFrame(0, 0, &frm, BLOCK) < 0) {
			printf("SendFrame failed at frame %d\n", n);
			break;
		}
		done += per;
		n++;
	}
	printf("sent %d frames, %d samples\n", n, done);
	/* the oss2 driver's flush drops what is still queued; let it play out first */
	usleep(1000000);
	CHK(IMP_AO_FlushChnBuf(0, 0));
	CHK(IMP_AO_DisableChn(0, 0));
	CHK(IMP_AO_Disable(0));
	printf("AO DONE\n");
	return 0;
}
