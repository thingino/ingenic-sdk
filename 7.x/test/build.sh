#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
# build.sh <program.c>...: cross-build libimp test programs.
#   CC      cross compiler (e.g. mipsel-linux-gcc)
#   IMPINC  the SoC's libimp include directory (the one holding imp/)
#   IMPLIB  directory with that SoC's libimp.so and libalog.so
#
# Programs:  imp-osd.c (T10 T20 T21 T23 T30), t31/imp-osd.c (T31): OSD
#            regions on the H.264 stream, checked on the host by check-osd.py
#            t20cap.c t20enc.c (T10 T20), t30cap.c t30enc.c (T30),
#            imp-enc.c (T21 T23), t31/imp-enc.c (T31): capture and encode
#            t32/imp-enc.c, t32/imp-osd.c (T32, T33): as the T31 ones with the 2.x API;
#            build them with the SoC's include directory (SENSOR=gc5613 I2CADDR=0x31 on the T33)
#            imp-two.c (T10 T20 T30): main and sub stream at once, frames per second of each
#            imp-ai.c imp-ao.c (all): audio capture and playback; imp-dmic.c (T32): DMIC capture;
#            pcmtone.py, wavtone.py, phasejump.py analyse the result
# The T10, T20 and T30 vendor libraries were linked against uClibc 0.9.33:
# on a uClibc-ng root they link libctype_shim.so (built here from
# ctype_shim.c), which then goes next to them in the library path.
set -e
: "${CC:?set CC}" "${IMPINC:?set IMPINC}" "${IMPLIB:?set IMPLIB}"
HERE=$(cd "$(dirname "$0")" && pwd)
$CC -Os -shared -fPIC -o libctype_shim.so "$HERE/ctype_shim.c"
for t in "$@"; do
	n=$(basename "$t" .c)
	$CC -Os -Wall -I"$IMPINC" -o "$n" "$t" -L"$IMPLIB" -limp -lalog ./libctype_shim.so -lpthread -lm -ldl
done
