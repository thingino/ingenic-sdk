#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""check-osd.py <outdir>: pixel check of imp-osd's stream-0.h264 against regions.txt and the dumped bitmaps."""
import subprocess, sys, os
import numpy as np
from PIL import Image

d = sys.argv[1]
reg = {}
for l in open(f"{d}/regions.txt"):
    k, *v = l.split()
    reg[k] = v
W, H = int(reg["frame"][0]), int(reg["frame"][1])
show_at, update_at = int(reg["show_at"][0]), int(reg["update_at"][0])


def frame(n):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", f"{d}/stream-0.h264", "-vf", f"select=eq(n\\,{n})",
                          "-frames:v", "1", "-vsync", "0", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.uint8).reshape(H, W, 3).astype(int)


def bgra(name, w, h):
    return np.fromfile(f"{d}/{name}", np.uint8).reshape(h, w, 4)[:, :, [2, 1, 0]].astype(int)


fails = 0


def check(name, ok, info=""):
    global fails
    print(("PASS " if ok else "FAIL ") + name + " " + info)
    fails += not ok


def box(f, x, y, w, h):
    return f[y:y + h, x:x + w]


ctl, on, upd = frame(show_at - 10), frame(update_at - 10), frame(update_at + 15)
Image.fromarray(on.astype(np.uint8)).save(f"{d}/frame-shown.png")
Image.fromarray(upd.astype(np.uint8)).save(f"{d}/frame-updated.png")
Image.fromarray(ctl.astype(np.uint8)).save(f"{d}/frame-control.png")

bx, by, bw, bh = (int(v) for v in reg["bars"][:4])
for name, fr, order in (("bars (shown)", on, "bars-a.bgra"), ("bars (after UpdateRgnAttrData)", upd, "bars-b.bgra")):
    exp = bgra(order, bw, bh)
    err = []
    for i in range(4):
        got = box(fr, bx + i * 64 + 16, by + 16, 32, 32).reshape(-1, 3).mean(0)
        err.append(np.abs(got - exp[32, i * 64 + 32]).max())
    check(name, max(err) < 40, f"max channel error per bar {[int(e) for e in err]}")
ctlbars = np.abs(box(ctl, bx, by, bw, bh) - bgra("bars-a.bgra", bw, bh)).mean()
check("bars absent in the control frame", ctlbars > 30, f"mean diff {ctlbars:.0f}")

if "isp" not in reg:
    tx, ty, tw, th = (int(v) for v in reg["text"][:4])
    exp = bgra("text.bgra", tw, th)[:, :, 0] > 128
    got = box(on, tx, ty, tw, th).mean(2) > 128
    agree = (exp == got).mean()
    check("text glyphs", agree > 0.97, f"{agree * 100:.1f}% of {tw}x{th} pixels match")

    sx, sy, sw, sh = (int(v) for v in reg["solid"][:4])
    col = np.array([255, 128, 0])
    got = box(on, sx + 8, sy + 8, sw - 16, sh - 16).reshape(-1, 3).mean(0)
    check("solid orange rectangle", np.abs(got - col).max() < 40, f"mean RGB {got.astype(int).tolist()} want {col.tolist()}")
    cs = box(ctl, sx + 8, sy + 8, sw - 16, sh - 16).reshape(-1, 3).mean(0)
    check("solid rectangle absent in the control frame", np.abs(cs - col).max() > 40, f"mean RGB {cs.astype(int).tolist()}")

rx, ry, rw, rh, lw = (int(v) for v in reg["rect"][:5])
rcol = reg["rect"][5]
edge = np.concatenate([box(on, rx + 20, ry + 1, rw - 40, 2).reshape(-1, 3), box(on, rx + 20, ry + rh - 3, rw - 40, 2).reshape(-1, 3)])
m = edge.mean(0)
cedge = np.concatenate([box(ctl, rx + 20, ry + 1, rw - 40, 2).reshape(-1, 3), box(ctl, rx + 20, ry + rh - 3, rw - 40, 2).reshape(-1, 3)]).mean(0)
if rcol == "ff00ff00":
    check("rect outline (CPU path)", m[1] > m[0] + 60 and m[1] > m[2] + 60, f"mean top/bottom edge RGB {m.astype(int).tolist()}")
else:
    # the T23 libimp takes its own colour enum (palette not decoded here): the edge must differ from the control frame
    check("rect outline (CPU path, colour enum " + rcol + ")", np.abs(m - cedge).max() > 60, f"edge RGB {m.astype(int).tolist()} control {cedge.astype(int).tolist()}")

cx, cy, cw, ch = (int(v) for v in reg["cover"][:4])
m = box(on, cx + 20, cy + 20, cw - 40, ch - 40).reshape(-1, 3).mean(0)
cm = box(ctl, cx + 20, cy + 20, cw - 40, ch - 40).reshape(-1, 3).mean(0)
check("cover (mask layer)", m[0] > 200 and m[1] < 60 and m[2] < 60 and np.abs(cm - [255, 0, 0]).max() > 60, f"shown RGB {m.astype(int).tolist()} control {cm.astype(int).tolist()}")
sys.exit(1 if fails else 0)
