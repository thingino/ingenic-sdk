# SPDX-License-Identifier: GPL-2.0
# Shared by the per-SoC sensor Kbuilds: build sensor_<name>_<soc>.ko for
# SENSOR_MODEL / SENSOR_1_MODEL / SENSOR_2_MODEL, or for every sensor in the
# directory (SENSORS) when none is given. tx-isp is built in the same pass, so
# modpost resolves the sensors against it without KBUILD_EXTRA_SYMBOLS.
sensors := $(strip $(SENSOR_MODEL) $(SENSOR_1_MODEL) $(SENSOR_2_MODEL))
sensors := $(if $(sensors),$(sort $(sensors)),$(SENSORS))
$(foreach s,$(sensors),$(if $(wildcard $(src)/$(s).c),,$(error ingenic-sdk 7.x: no sensor $(s) for $(SOC_FAMILY), have: $(SENSORS))))

obj-m += $(foreach s,$(sensors),sensor_$(s)_$(SOC_FAMILY).o)
$(foreach s,$(sensors),$(eval sensor_$(s)_$(SOC_FAMILY)-objs := $(s).o))
