# Ingenic SDK out-of-tree module build.
#
# Required inputs (make variables):
#   SOC_FAMILY      t10 t20 t21 t23 t30 t31 c100 t40 t41 a1
#   KERNEL_VERSION  3.10.14 | 4.4.94
#
# Everything else is a component switch named CONFIG_INGENIC_*. Each one
# has a per-SoC default below and can be overridden on the make command
# line, e.g.
#
#   make ... CONFIG_INGENIC_AUDIO=n CONFIG_INGENIC_MOTOR=y
#
# Command-line variables win over the `?=` defaults, so an integrator
# (thingino, CI, a bisect) selects components without editing this file.
#
# The SoC is taken from SOC_FAMILY, never from the kernel's CONFIG_SOC_*.
# Those symbols also exist in the kernel's auto.conf, so testing them here
# mixes two sources of truth: building t31 against a c100 kernel yields
# CONFIG_SOC_T31=y (command line) *and* CONFIG_SOC_C100=y (kernel), and
# the old concatenated test then silently dropped AVPU.

ifeq ($(SOC_FAMILY),)
    $(error SOC_FAMILY missing)
endif
ifeq ($(KERNEL_VERSION),)
    $(error KERNEL_VERSION missing)
endif

soc  := $(SOC_FAMILY)
kver := $(KERNEL_VERSION)

# ---- SoC / kernel predicates -------------------------------------------
is_a1     := $(if $(filter a1,$(soc)),y)
# 3.10 SoCs whose vendor SDK ships the oss3 audio tree rather than oss2.
is_oss3   := $(if $(filter t23 t32,$(soc)),y)
is_k310   := $(if $(filter 3.10.14,$(kver)),y)
has_avpu  := $(if $(filter t31 c100 t40 t41,$(soc)),y)
avpu_impl := $(if $(filter t31 c100,$(soc)),t31,t40)
has_nna   := $(if $(filter t40 t41 a1,$(soc)),y)
has_mpsys := $(if $(filter t40 t41,$(soc)),y)

# ---- component switches (override on the make line) ---------------------
CONFIG_INGENIC_ISP            ?= $(if $(is_a1),n,y)
CONFIG_INGENIC_SENSOR         ?= $(if $(is_a1),n,y)
CONFIG_INGENIC_AUDIO          ?= y
CONFIG_INGENIC_AVPU           ?= $(if $(has_avpu),y,n)
CONFIG_INGENIC_SOC_NNA        ?= $(if $(has_nna),y,n)
CONFIG_INGENIC_MPSYS          ?= $(if $(has_mpsys),y,n)
CONFIG_INGENIC_JZ_DTRNG       ?= $(if $(has_mpsys),y,n)
CONFIG_INGENIC_GPIO_USERKEYS  ?= $(if $(is_a1),n,$(if $(is_k310),y,n))
CONFIG_INGENIC_JZ_AES         ?= $(if $(is_a1),n,$(if $(is_k310),y,n))
# TCU allocator: central TCU-channel ownership registry. Both motor.ko and
# pwm_core.ko link against its symbols, and nothing else provides them.
CONFIG_INGENIC_TCU_ALLOC      ?= $(if $(is_a1),n,$(if $(is_k310),y,n))
# The PWM and motor drivers here are superseded by the standalone
# ingenic-pwm and thingino-motors projects; opt in if you want this copy.
CONFIG_INGENIC_PWM            ?= n
CONFIG_INGENIC_MOTOR          ?= n
CONFIG_INGENIC_MOTOR_SPI      ?= n
CONFIG_INGENIC_A1_MEDIA       ?= $(if $(is_a1),y,n)

# audio flavour: T23 uses oss3 even on 3.10; otherwise it follows the kernel
CONFIG_INGENIC_AUDIO_VARIANT  ?= $(if $(is_oss3),oss3,$(if $(is_k310),oss2,oss3))

# Dependency enforcement: pwm_core.ko and motor.ko both link tcu_alloc's
# exported symbols (tcu_alloc_claim/release/set_max_channels). The allocator
# is K310-only, so force it on whenever either driver is built there, even if
# the caller passed CONFIG_INGENIC_TCU_ALLOC=n (e.g. a PWM board with no motor).
ifeq ($(is_k310),y)
ifeq ($(CONFIG_INGENIC_PWM),y)
override CONFIG_INGENIC_TCU_ALLOC := y
endif
ifeq ($(CONFIG_INGENIC_MOTOR),y)
override CONFIG_INGENIC_TCU_ALLOC := y
endif
endif

ccflags-y := -DRELEASE -DUSER_BIT_32 -DKERNEL_BIT_32 -Wno-date-time -D_GNU_SOURCE
ccflags-y += -I$(src)/$(kver)/isp/$(soc)/include

$(info ingenic-sdk: soc=$(soc) kernel=$(kver))

# ---- ISP -----------------------------------------------------------------
ifeq ($(CONFIG_INGENIC_ISP),y)
    include $(src)/$(kver)/isp/Kbuild
endif

# ---- misc drivers --------------------------------------------------------
ifeq ($(CONFIG_INGENIC_GPIO_USERKEYS),y)
    include $(src)/$(kver)/misc/gpio-userkeys/Kbuild
endif

ifeq ($(CONFIG_INGENIC_JZ_AES),y)
    include $(src)/$(kver)/misc/jz-aes/Kbuild
endif

# TCU allocator: central ownership registry. pwm_core.ko and motor.ko need its symbols.
ifeq ($(CONFIG_INGENIC_TCU_ALLOC),y)
    include $(src)/$(kver)/misc/tcu_alloc/Kbuild
endif

ifeq ($(CONFIG_INGENIC_PWM),y)
    include $(src)/$(kver)/misc/pwm/Kbuild
endif

ifeq ($(CONFIG_INGENIC_MOTOR),y)
    include $(src)/$(kver)/misc/motor/Kbuild
    ifeq ($(CONFIG_INGENIC_MOTOR_SPI)$(is_k310),yy)
        include $(src)/$(kver)/misc/ms419xx/Kbuild
    endif
endif

# ---- audio ---------------------------------------------------------------
ifeq ($(CONFIG_INGENIC_AUDIO),y)
    include $(src)/$(kver)/audio/$(soc)/$(CONFIG_INGENIC_AUDIO_VARIANT)/Kbuild
endif

# ---- video accelerators --------------------------------------------------
ifeq ($(CONFIG_INGENIC_AVPU),y)
    include $(src)/$(kver)/avpu/$(avpu_impl)/Kbuild
endif

ifeq ($(CONFIG_INGENIC_SOC_NNA),y)
    include $(src)/$(kver)/misc/soc-nna/Kbuild
endif

ifeq ($(CONFIG_INGENIC_MPSYS),y)
    include $(src)/$(kver)/misc/mpsys-driver/Kbuild
endif

ifeq ($(CONFIG_INGENIC_JZ_DTRNG),y)
    include $(src)/$(kver)/misc/jz-dtrng/Kbuild
endif

# ---- sensors -------------------------------------------------------------
# With no sensor model selected, build the sinfo prober instead. The
# sensor-src Kbuild handles SENSOR_MODEL / SENSOR_1_MODEL / SENSOR_2_MODEL
# in one pass, so it is included exactly once (it used to be included twice
# when both models were set).
ifeq ($(CONFIG_INGENIC_SENSOR),y)
    ifeq ($(strip $(SENSOR_MODEL)$(SENSOR_1_MODEL)$(SENSOR_2_MODEL)),)
        $(info ingenic-sdk: no sensor model selected, building sinfo prober)
        include $(src)/sinfo/Kbuild
    else
        include $(src)/$(kver)/sensor-src/Kbuild
    endif
endif

# ---- A1 media pipeline ---------------------------------------------------
ifeq ($(CONFIG_INGENIC_A1_MEDIA),y)
    include $(src)/$(kver)/aip/a1/Kbuild
    include $(src)/$(kver)/fb/Kbuild
    include $(src)/$(kver)/ipu/Kbuild
    include $(src)/$(kver)/video/a1/vde/Kbuild
    include $(src)/$(kver)/video/a1/vdec/Kbuild
    include $(src)/$(kver)/audio/$(soc)/hdmi_audio/Kbuild
endif
