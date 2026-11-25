// SPDX-License-Identifier: GPL-2.0
/*
 * imp-dmic: record through libimp's IMP_DMIC (T32), write raw PCM16 and
 * report the level and the zero-crossing frequency.
 *
 * usage: imp-dmic <out.pcm> [seconds] [rate] [gain]
 *
 * libimp's IMP_DMIC_SetGain after IMP_DMIC_SetVol returns silence (the
 * kernel buffer holds the signal), so the gain is set only when asked for.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <imp/imp_log.h>
#include <imp/imp_dmic.h>

#define CHK(call) do { int r_ = (call); printf("%s = %d\n", #call, r_); fflush(stdout); if (r_ < 0) return 1; } while (0)

int main(int argc, char **argv)
{
	const char *out = argc > 1 ? argv[1] : "/tmp/dmic.pcm";
	int secs = argc > 2 ? atoi(argv[2]) : 4;
	int rate = argc > 3 ? atoi(argv[3]) : 16000;
	int gain = argc > 4 ? atoi(argv[4]) : -1;
	int per = rate / 25;
	long total = (long)secs * rate, got = 0, zc = 0, nz = 0;
	int n = 0, prev = 0;
	double sq = 0;
	IMPDmicAttr attr;
	IMPDmicChnFrame frm;
	IMPDmicChnParam cp;
	FILE *f = fopen(out, "wb");

	if (!f)
		return 2;
	memset(&attr, 0, sizeof(attr));
	memset(&cp, 0, sizeof(cp));
	attr.samplerate = rate;
	attr.bitwidth = DMIC_BIT_WIDTH_16;
	attr.soundmode = DMIC_SOUND_MODE_MONO;
	attr.frmNum = 20;
	attr.numPerFrm = per;
	attr.chnCnt = 1;
	CHK(IMP_DMIC_SetUserInfo(0, 1, 0));
	CHK(IMP_DMIC_SetPubAttr(0, &attr));
	CHK(IMP_DMIC_Enable(0));
	cp.usrFrmDepth = 20;
	CHK(IMP_DMIC_SetChnParam(0, 0, &cp));
	CHK(IMP_DMIC_EnableChn(0, 0));
	CHK(IMP_DMIC_SetVol(0, 0, 60));
	if (gain >= 0)
		CHK(IMP_DMIC_SetGain(0, 0, gain));

	while (got < total) {
		int i, polled = IMP_DMIC_PollingFrame(0, 0, 1000);

		if (polled < 0) {
			printf("poll timeout at frame %d\n", n);
			break;
		}
		if (IMP_DMIC_GetFrame(0, 0, &frm, BLOCK) < 0)
			break;
		{
			int16_t *s = (int16_t *)frm.rawFrame.virAddr;
			int ns = frm.rawFrame.len / 2;

			fwrite(s, 2, ns, f);
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
		IMP_DMIC_ReleaseFrame(0, 0, &frm);
		n++;
	}
	fclose(f);
	printf("frames %d samples %ld nonzero %ld rms %.1f zero-crossing-hz %.1f\n", n, got, nz,
	       got ? __builtin_sqrt(sq / got) : 0.0, got ? zc * rate / 2.0 / got : 0.0);
	CHK(IMP_DMIC_DisableChn(0, 0));
	CHK(IMP_DMIC_Disable(0));
	printf("DMIC DONE\n");
	return 0;
}
