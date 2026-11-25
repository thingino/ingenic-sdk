// SPDX-License-Identifier: GPL-2.0
/*
 * imp-ai: record through libimp's IMP_AI, write raw PCM16 and report the
 * level and the zero-crossing frequency. Also prints the mean (DC) and rms
 * of each 50 ms window of the first 500 ms, and of the last second.
 *
 * usage: imp-ai <out.pcm> [seconds] [rate] [dev]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <imp/imp_log.h>
#include <imp/imp_audio.h>

#define CHK(call) do { int r_ = (call); printf("%s = %d\n", #call, r_); fflush(stdout); if (r_ < 0) return 1; } while (0)

int main(int argc, char **argv)
{
	const char *out = argc > 1 ? argv[1] : "/tmp/ai.pcm";
	int secs = argc > 2 ? atoi(argv[2]) : 4;
	int rate = argc > 3 ? atoi(argv[3]) : 16000;
	int dev = argc > 4 ? atoi(argv[4]) : 1;
	int per = rate / 25;
	long total = (long)secs * rate, got = 0, zc = 0, nz = 0;
	int n = 0, prev = 0;
	double sq = 0;
	int win = rate / 20, nw = 0;
	long wn = 0;
	double wsum = 0, wsq = 0;
	int16_t *all = malloc((size_t)total * 2 + 65536);
	IMPAudioIOAttr attr;
	IMPAudioFrame frm;
	IMPAudioIChnParam cp;
	memset(&cp, 0, sizeof(cp));
	FILE *f = fopen(out, "wb");

	if (!f)
		return 2;
	memset(&attr, 0, sizeof(attr));
	attr.samplerate = rate;
	attr.bitwidth = AUDIO_BIT_WIDTH_16;
	attr.soundmode = AUDIO_SOUND_MODE_MONO;
	attr.frmNum = 20;
	attr.numPerFrm = per;
	attr.chnCnt = 1;
	CHK(IMP_AI_SetPubAttr(dev, &attr));
	CHK(IMP_AI_Enable(dev));
	cp.usrFrmDepth = 20;
	cp.Rev = 0;
	CHK(IMP_AI_SetChnParam(dev, 0, &cp));
	CHK(IMP_AI_EnableChn(dev, 0));
	CHK(IMP_AI_SetVol(dev, 0, 60));
	CHK(IMP_AI_SetGain(dev, 0, 28));

	while (got < total) {
		int i, polled = IMP_AI_PollingFrame(dev, 0, 1000);

		if (polled < 0) {
			printf("poll timeout at frame %d\n", n);
			break;
		}
		if (IMP_AI_GetFrame(dev, 0, &frm, BLOCK) < 0)
			break;
		{
			int16_t *s = (int16_t *)frm.virAddr;
			int ns = frm.len / 2;

			fwrite(s, 2, ns, f);
			if (all && got + ns <= total + 32768)
				memcpy(all + got, s, ns * 2);
			for (i = 0; i < ns; i++) {
				sq += (double)s[i] * s[i];
				if ((s[i] >= 0) != (prev >= 0))
					zc++;
				prev = s[i];
				if (s[i])
					nz++;
			}
			got += ns;
		}
		IMP_AI_ReleaseFrame(dev, 0, &frm);
		n++;
	}
	fclose(f);
	if (all) {
		long i, lo;

		for (i = 0; i < got && nw < 10; i++) {
			wsum += all[i];
			wsq += (double)all[i] * all[i];
			if (++wn == win) {
				printf("win %3d-%3d ms: mean %7.1f rms %7.1f\n", nw * 50, nw * 50 + 50,
				       wsum / wn, __builtin_sqrt(wsq / wn));
				nw++;
				wn = 0;
				wsum = wsq = 0;
			}
		}
		lo = got > rate ? got - rate : 0;
		wsum = wsq = 0;
		for (i = lo; i < got; i++) {
			wsum += all[i];
			wsq += (double)all[i] * all[i];
		}
		if (got > lo)
			printf("steady last 1 s: mean %.1f rms %.1f\n", wsum / (got - lo),
			       __builtin_sqrt(wsq / (got - lo)));
	}
	printf("frames %d samples %ld nonzero %ld rms %.1f zero-crossing-hz %.1f\n", n, got, nz,
	       got ? __builtin_sqrt(sq / got) : 0.0, got ? zc * rate / 2.0 / got : 0.0);
	CHK(IMP_AI_DisableChn(dev, 0));
	CHK(IMP_AI_Disable(dev));
	printf("AI DONE\n");
	return 0;
}
