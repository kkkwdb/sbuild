# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# This .mk is the generic target rte.var.mk ; it includes .mk for
# the specified machine, architecture, toolchain (compiler) and
# executive environment.
#

#
# machine:
#
#   - can define ARCH variable (overridden by cmdline value)
#   - can define CROSS variable (overridden by cmdline value)
#   - define MACHINE_CFLAGS variable (overridden by cmdline value)
#   - define MACHINE_LDFLAGS variable (overridden by cmdline value)
#   - define MACHINE_ASFLAGS variable (overridden by cmdline value)
#   - can define CPU_CFLAGS variable (overridden by cmdline value) that
#     overrides the one defined in arch.
#   - can define CPU_LDFLAGS variable (overridden by cmdline value) that
#     overrides the one defined in arch.
#   - can define CPU_ASFLAGS variable (overridden by cmdline value) that
#     overrides the one defined in arch.
#
ifneq ($(wildcard $(SRTE_SDK)/mk/machine/$(SRTE_MACHINE)/rte.vars.mk),)
include $(SRTE_SDK)/mk/machine/$(SRTE_MACHINE)/rte.vars.mk
else
MACHINE_CFLAGS := -march=$(SRTE_MACHINE)
endif

#
# arch:
#
#   - define ARCH variable (overridden by cmdline or by previous
#     optional define in machine .mk)
#   - define CROSS variable (overridden by cmdline or previous define
#     in machine .mk)
#   - define CPU_CFLAGS variable (overridden by cmdline or previous
#     define in machine .mk)
#   - define CPU_LDFLAGS variable (overridden by cmdline or previous
#     define in machine .mk)
#   - define CPU_ASFLAGS variable (overridden by cmdline or previous
#     define in machine .mk)
#   - may override any previously defined variable
#
include $(SRTE_SDK)/mk/arch/$(SRTE_ARCH)/rte.vars.mk

#
# toolchain:
#
#   - define CC, LD, AR, AS, ...
#   - define TOOLCHAIN_CFLAGS variable (overridden by cmdline value)
#   - define TOOLCHAIN_LDFLAGS variable (overridden by cmdline value)
#   - define TOOLCHAIN_ASFLAGS variable (overridden by cmdline value)
#   - may override any previously defined variable
#
include $(SRTE_SDK)/mk/toolchain/$(SRTE_TOOLCHAIN)/rte.vars.mk

#
# exec-env:
#
#   - define EXECENV_CFLAGS variable (overridden by cmdline)
#   - define EXECENV_LDFLAGS variable (overridden by cmdline)
#   - define EXECENV_ASFLAGS variable (overridden by cmdline)
#   - may override any previously defined variable
#
include $(SRTE_SDK)/mk/exec-env/$(SRTE_EXEC_ENV)/rte.vars.mk

# Don't set CFLAGS/LDFLAGS flags for kernel module, all flags are
# provided by Kbuild framework.
ifeq ($(KERNELRELEASE),)

# now that the environment is mostly set up, including the machine type we will
# be passing to the compiler, set up the specific CPU flags based on that info.
include $(SRTE_SDK)/mk/rte.cpuflags.mk

# merge all CFLAGS
CFLAGS := $(CPU_CFLAGS) $(EXECENV_CFLAGS) $(TOOLCHAIN_CFLAGS) $(MACHINE_CFLAGS)
CFLAGS += $(TARGET_CFLAGS)

# merge all LDFLAGS
LDFLAGS := $(CPU_LDFLAGS) $(EXECENV_LDFLAGS) $(TOOLCHAIN_LDFLAGS) $(MACHINE_LDFLAGS)
LDFLAGS += $(TARGET_LDFLAGS)

# merge all ASFLAGS
ASFLAGS := $(CPU_ASFLAGS) $(EXECENV_ASFLAGS) $(TOOLCHAIN_ASFLAGS) $(MACHINE_ASFLAGS)
ASFLAGS += $(TARGET_ASFLAGS)

# add default include and lib paths
CFLAGS += -I$(SRTE_OUTPUT)/include
LDFLAGS += -L$(SRTE_OUTPUT)/lib

CFLAGS += -I$(SRTE_SDK_BIN)/include
LDFLAGS += -L$(SRTE_SDK_BIN)/lib

export CFLAGS
export LDFLAGS

endif
