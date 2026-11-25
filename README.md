## Open Source Ingenic kernel modules

### Building Ingenic Kernel Modules

#### How to Build

To compile the kernel modules for Ingenic SoCs, run the following command from your terminal:

```console
./build.sh <soc> <kernel_version> <make_args>
./build.sh clean
```

Example for building the kernel module for the GC2053 sensor on a T31 SoC with kernel version 3.10:

```console
SENSOR_MODEL=gc2053 ./build.sh t31 3.10
```

### Parameters:
- `<soc>`: Specify the Ingenic SoC model you are using, such as `t31`, `t40`, etc.
- `<kernel_version>`: Indicate the kernel version. Supported versions include `3.10` and `4.4`.
- `<make_args>`: Additional make arguments as required.

The build needs `SOC_FAMILY` (from `<soc>`) and `KERNEL_VERSION`; the
SoC is taken from `SOC_FAMILY` alone, never from the kernel's
`CONFIG_SOC_*`. Set `SENSOR_MODEL` (or `SENSOR_1_MODEL` /
`SENSOR_2_MODEL` for dual sensors) to build a sensor driver; with none
set, the standalone `sinfo` prober is built instead.

### Component selection

Each buildable component is a `CONFIG_INGENIC_*` switch. Every switch
has a per-SoC / per-kernel default, and any of them can be overridden on
the make command line (command-line variables win over the defaults):

```console
# ISP + sensor only, nothing else
SENSOR_MODEL=gc4653 ./build.sh t31 3.10 \
    CONFIG_INGENIC_AUDIO=n CONFIG_INGENIC_GPIO_USERKEYS=n CONFIG_INGENIC_JZ_AES=n

# non-camera device (SBC, alarm): no ISP or sensor, keep audio
./build.sh t31 3.10 CONFIG_INGENIC_ISP=n CONFIG_INGENIC_SENSOR=n
```

| switch | default | notes |
|---|---|---|
| `CONFIG_INGENIC_ISP` | on (off on a1) | tx-isp core |
| `CONFIG_INGENIC_SENSOR` | on (off on a1) | sensor driver / `sinfo` prober; needs ISP |
| `CONFIG_INGENIC_AUDIO` | on (off on a1) | AIC/codec; independent of ISP |
| `CONFIG_INGENIC_AUDIO_VARIANT` | `oss2` on 3.10, else `oss3`; `oss3` on t23 | audio ABI |
| `CONFIG_INGENIC_AVPU` | on for t31/c100/t40/t41 | H.264/H.265 encoder |
| `CONFIG_INGENIC_AIP` | on for a1 (7.x) | AIP engines (`ingenic-aip.ko`) |
| `CONFIG_INGENIC_VO` / `_FB` | on for a1 (7.x) | video output with HDMI (`vo.ko`, `/dev/vde`) and the framebuffer (`fb.ko`) |
| `CONFIG_INGENIC_VDEC` | on for a1 (7.x) | VC8000D decoder (`vdec.ko`, `/dev/vdec`) |
| `CONFIG_INGENIC_SOC_NNA` | on for t40/t41/a1 | neural accelerator |
| `CONFIG_INGENIC_MPSYS` / `_JZ_DTRNG` | on for t40/t41 | mpsys + hardware TRNG |
| `CONFIG_INGENIC_GPIO_USERKEYS` | on for 3.10 | GPIO buttons |
| `CONFIG_INGENIC_JZ_AES` | on for 3.10 | hardware AES (`/dev/aes`); under thingino, selected by the mbedtls hardware-AES option |
| `CONFIG_INGENIC_TCU_ALLOC` | on for 3.10 | TCU channel allocator; the motor driver links its symbols |
| `CONFIG_INGENIC_PWM` | off | pwm_core / pwm_hal; kernel side of the ingenic-pwm userspace utility |
| `CONFIG_INGENIC_MOTOR` / `_MOTOR_SPI` | off | PTZ motor driver; kernel side of the thingino-motors userspace utility |

Enabling a component on a SoC that lacks it is allowed but will fail to
build. Under thingino these switches are also exposed in menuconfig
(package -> Ingenic SDK -> SDK components), where the camera pipeline
(ISP, sensor, AVPU) follows the selected device type.

## Licensing

The kernel driver source in this repository is licensed **GPL-2.0-or-later**,
as declared by the `SPDX-License-Identifier` headers in the source files and
the `MODULE_LICENSE("GPL")` markings in the modules. Being Linux kernel
modules, they must be built and distributed under GPL-compatible terms.

The precompiled ISP/audio/AVPU firmware archives (the `*.a` and `*.o_shipped`
blobs under `<kernel>/sdk/`, `<kernel>/misc/`, etc.) are **proprietary
firmware, copyright their respective owners** (Ingenic Semiconductor and its
vendors). They are redistributed here for use with these drivers; they are
not covered by the GPL and no source is available for them. Linking the GPL
drivers against these vendor blobs is what the Ingenic BSP does, and the
resulting modules load the firmware at runtime.

If you redistribute builds of this SDK, keep the driver source available
under the GPL and preserve the firmware copyright notices.
