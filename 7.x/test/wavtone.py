#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""wavtone.py <speaker.wav> [threshold]: list the non-silent segments of a QEMU audiodev
wav capture (header sizes may be 0 if QEMU was killed) with peak frequency, rms and gaps."""
import sys
import numpy as np

raw = open(sys.argv[1], 'rb').read()
thr = float(sys.argv[2]) if len(sys.argv) > 2 else 100
ch = int.from_bytes(raw[22:24], 'little')
fs = int.from_bytes(raw[24:28], 'little')
x = np.frombuffer(raw[44:len(raw) - (len(raw) - 44) % (2 * ch)], dtype='<i2').reshape(-1, ch)[:, 0].astype(float)
blk = fs // 100
n = len(x) // blk
rms = np.sqrt((x[:n * blk].reshape(n, blk) ** 2).mean(axis=1))
print('rate %d ch %d total %.2fs' % (fs, ch, len(x) / fs))
segs, s = [], None
for i, r in enumerate(rms):
    if r > thr and s is None:
        s = i
    elif r <= thr and s is not None:
        segs.append((s, i)); s = None
if s is not None:
    segs.append((s, n))
# merge segments split by gaps shorter than 50 ms, report the gaps seen
merged = []
for a, b in segs:
    if merged and a - merged[-1][1] < 5:
        merged[-1] = (merged[-1][0], b)
    else:
        merged.append((a, b))
for a, b in merged:
    seg = x[a * blk:b * blk]
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
    f = np.fft.rfftfreq(len(seg), 1 / fs)
    k = sp.argmax()
    # parabolic interpolation of the peak
    if 0 < k < len(sp) - 1:
        a1, b1, c1 = np.log(sp[k - 1] + 1e-9), np.log(sp[k] + 1e-9), np.log(sp[k + 1] + 1e-9)
        k = k + 0.5 * (a1 - c1) / (a1 - 2 * b1 + c1)
    fpk = k * fs / len(seg)
    seg_rms = rms[a:b]
    # phase continuity: fit one sinusoid over the steady part, residual per 5 ms block
    core = x[int((a + 30) * blk):int(b * blk)]
    t = np.arange(len(core)) / fs
    best = None
    for df in np.linspace(-0.5, 0.5, 101):
        w = 2 * np.pi * (fpk + df) * t
        m = np.c_[np.sin(w), np.cos(w)]
        c, *_ = np.linalg.lstsq(m, core, rcond=None)
        r = core - m @ c
        e = (r ** 2).sum()
        if best is None or e < best[0]:
            best = (e, r, fpk + df, np.hypot(*c))
    _, r, ffit, amp = best
    nb = len(r) // (fs // 200)
    rb = np.sqrt((r[:nb * (fs // 200)].reshape(nb, -1) ** 2).mean(axis=1))
    bad = [round((a + 30) / 100 + i * 0.005, 3) for i in range(nb) if rb[i] > 0.2 * amp]
    print('  glitch blocks (5 ms, residual > 20%% of amp) at s: %s' % (bad[:30] if bad else 'none'))
    print('  fit %.2f Hz amp %.0f, residual rms %.0f, worst 5 ms block %.0f (%.1f%% of amp)'
          % (ffit, amp, np.sqrt((r ** 2).mean()), rb.max(), 100 * rb.max() / amp))
    inner = [i for i in range(a, b) if rms[i] <= thr]
    print('segment %.2f-%.2fs  dur %.2fs  peak %.1f Hz  rms %.0f (min %.0f max %.0f)  quiet 10ms blocks inside: %d'
          % (a / 100, b / 100, (b - a) / 100, fpk, np.sqrt((seg ** 2).mean()), seg_rms.min(), seg_rms.max(), len(inner)))
