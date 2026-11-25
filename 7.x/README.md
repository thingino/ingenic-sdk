# Ingenic modules for mainline Linux 7.x

Out-of-tree kernel modules that run the stock Ingenic userspace (libimp,
libalog) on mainline Linux 7.x on the T10, T20, T21, T23, T30, T31, T32, T33, T40 and T41.

They pair with the single kernel branch `ingenic-7.4` (T10 to T33, one
`ingenic_tNN_defconfig` per SoC) of https://github.com/gtxaspec/linux, which
carries the SoC support, the device tree nodes these modules look up (ISP,
encoder, IPU, AIC, codec) and the `rmem=` carve-out.

## Build

```console
KDIR=/path/to/linux-7.4-build CROSS_COMPILE=mipsel-linux- ./build.sh t31 7.x
```

`KDIR` is the kernel tree built from the SoC's `ingenic_tNN_defconfig` (or
its `O=` build directory). One `M=`
pass builds every module of the SoC; the `.ko` files land next to their
sources under `7.x/`. The T10 uses the T20 ISP, encoder and sensor sources,
built under T10 names.

Components are switched with `CONFIG_INGENIC_ISP`, `_SENSOR`, `_VPU`, `_AVPU`,
`_IPU`, `_AUDIO`, `_RMEM` and `_LOGGER` (all `y` where the SoC has the block),
e.g. `./build.sh t20 7.x CONFIG_INGENIC_AUDIO=n`. `SENSOR_MODEL=<name>` (and
`SENSOR_1_MODEL`, `SENSOR_2_MODEL`) limits the sensor drivers to those named;
without it every sensor in `7.x/sensor-src/<soc>` is built.

## Modules

| SoC | ISP (`tx-isp-<soc>.ko`) | encoder | OSD | audio (`audio.ko`) | sensors |
|---|---|---|---|---|---|
| T10 | vendor source + Apical blob (T20 tree) | `soc-vpu-t10.ko` (helix) | `ipu-t10.ko` | oss2 | jxf23, jxh42, ov9712 |
| T20 | vendor source + Apical blob | `soc-vpu-t20.ko` (helix) | `ipu-t20.ko` | oss2 | jxf23, jxh42, ov9712 |
| T21 | closed firmware 1.0.33 (3.10 build) + ABI adapter | `soc-vpu-t21.ko` (helix) | `ipu-t21.ko` | oss2 | ov2735b |
| T23 | closed firmware 1.3.0 (4.4.94 build) + ABI adapter | `soc-vpu-t23.ko` (helix) | `ipu-t23.ko` | oss3 | sc2336 |
| T30 | vendor source + Apical blob | `soc-vpu-t30.ko` (helix H.264/JPEG, radix H.265) | `ipu-t30.ko` | oss2 | jxf23, sc2235, sc4236 |
| T31 | closed firmware 1.1.6 (3.10 build) + ABI adapter | `avpu.ko` (EL150 H.264/H.265/JPEG) | `ipu.ko` | oss3 | gc4653 |
| T32 | closed firmware isp_20260618a (5.15 build) + ABI adapter | `soc-vpu-t32.ko` (helix H.264/JPEG, radix H.265) | `ipu-t32.ko` (IPU v13) | oss3 (AIC, inner codec, DMIC with `dmic_enable=1`) | sc4336p |
| T33 | closed firmware isp_20260706a (5.15 build) + ABI adapter | `soc-vpu-t33.ko` (helix H.264/JPEG, radix H.265; sources in `vpu/t32`) | `ipu-t33.ko` (IPU v13; sources in `ipu/t32`) | oss3 (AIC, inner codec) | gc5613 |
| T40 | closed firmware 1.3.1 (4.4.94 build) + ABI adapter | `avpu.ko` (EL150 H.264/H.265/JPEG) | | oss3 (AIC, inner codec) | imx307 |
| T41 | closed firmware 1.2.6 (4.4.94 build) + ABI adapter | `avpu.ko` (EL200 H.264/H.265/JPEG) | | oss3 (AIC, inner codec) | gc4023 |

Every SoC also gets `rmem.ko` (`/dev/rmem`, libimp's buffers in the `rmem=`
region, and `ispmem=` on the T10/T20) and `android_logger.ko` (`/dev/log_*`).

The closed ISP firmware is the vendor object, unmodified. The adapter
(`tx-isp-abi.c`, `txx-funcs.c`, `tx-isp-fwc*.c`) redirects its imports whose
semantics changed since 3.10/4.4 to translators, replaces the firmware
functions that read kernel structures by old offsets with C, and re-exports
the symbols the sensor drivers use. The objcopy steps are in each `Kbuild`.

Layout: `isp/<soc>`, `vpu/<soc>`, `avpu/t31`, `sensor-src/<soc>` per SoC;
`ipu`, `rmem`, `logger` shared by all; `audio/oss2` shared by the T10, T20,
T21 and T30; `audio/oss3/t23`, `audio/oss3/t31`, `audio/oss3/t32`, `audio/oss3/t40` and `audio/oss3/t41` (different vendor driver
releases with different ioctl sets); `audio/oss3/t33` uses the T32's dsp core
with its own AIC and codec.

## Verified on hardware

The stock libimp of each SoC, on a board with the matching sensor:

| SoC | capture + encode | OSD (IPU) | audio | other |
|---|---|---|---|---|
| T10 | H.264 | yes | playback and capture path | |
| T20 | H.264 | yes | loopback | |
| T21 | H.264 | yes | playback and capture path | |
| T23 | H.264 | yes | playback and capture path | |
| T30 | H.264, H.265 | yes | loopback | |
| T31 | H.264 main + sub stream, JPEG | yes | loopback | RTSP streaming |
| T32 | H.264 main + sub stream, JPEG | yes | | |
| T33 | H.264 main + sub stream | yes (QEMU) | | QEMU only |
| T40 | H.264 main + sub stream, JPEG | | capture and playback (QEMU) | silicon: video, test pattern only |
| T41 | H.264, H.265 main + sub stream, JPEG | | capture and playback | silicon: video only so far |

## Tests

`test/` holds the libimp test programs used for the checks above and the host
scripts that analyse their output. They build against the SoC's libimp
headers and libraries, see `test/build.sh`.
