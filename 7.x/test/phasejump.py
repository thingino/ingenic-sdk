#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""phasejump.py <speaker.wav> <t0> <t1> <hz>: report phase steps of a tone in a QEMU wav capture
between t0 and t1 seconds (a gap shows as a step of +phase, a repeat as -phase)."""
import sys
import numpy as np

raw = open(sys.argv[1], 'rb').read()
ch = int.from_bytes(raw[22:24], 'little')
fs = int.from_bytes(raw[24:28], 'little')
x = np.frombuffer(raw[44:len(raw) - (len(raw) - 44) % (2 * ch)], dtype='<i2').reshape(-1, ch)[:, 0].astype(float)
t0, t1, hz = float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
x = x[int(t0 * fs):int(t1 * fs)]
n = len(x)
X = np.fft.fft(x)
h = np.zeros(n)
h[0] = 1
h[1:(n + 1) // 2] = 2
if n % 2 == 0:
    h[n // 2] = 1
z = np.fft.ifft(X * h)
ph = np.unwrap(np.angle(z)) - 2 * np.pi * hz * np.arange(n) / fs
d = np.diff(ph)
med = np.median(d)
for i in np.nonzero(np.abs(d - med) > 0.25)[0]:
    print('t=%.4fs step %.2f rad = %.2f samples of the tone period (%.3f ms)' % (t0 + i / fs, d[i] - med, (d[i] - med) / (2 * np.pi) * fs / hz, 1000 * (d[i] - med) / (2 * np.pi) / hz))
print('median drift %.2e rad/sample' % med)
