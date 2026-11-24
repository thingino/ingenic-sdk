#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""pcmtone.py <file.pcm> [rate]: peak frequency and level of a raw mono S16LE capture, per
second, plus how many samples at the start are zero (settle) and the longest run of identical
samples (a repeated or stuck buffer)."""
import sys
import numpy as np

x = np.fromfile(sys.argv[1], dtype='<i2').astype(float)
fs = int(sys.argv[2]) if len(sys.argv) > 2 else 16000
print('%d samples, %.2fs' % (len(x), len(x) / fs))
nz = np.nonzero(x)[0]
print('leading zero samples: %d (%.0f ms)' % (nz[0] if len(nz) else len(x), 1000.0 * (nz[0] if len(nz) else len(x)) / fs))
for i in range(0, len(x) - fs + 1, fs):
    seg = x[i:i + fs]
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
    k = sp.argmax()
    print('  %.0f-%.0fs  peak %.1f Hz  rms %.0f  dc %.0f' % (i / fs, i / fs + 1, k * fs / len(seg), np.sqrt((seg ** 2).mean()), seg.mean()))
run = best = 1
for a, b in zip(x[:-1], x[1:]):
    run = run + 1 if a == b else 1
    best = max(best, run)
print('longest run of identical samples: %d' % best)
