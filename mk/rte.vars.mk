# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# To be included at the beginning of all RTE user Makefiles. This
# .mk will define the RTE environment variables by including the
# config file of SDK. It also includes the config file from external
# application if any.
#

ifeq ($(SRTE_SDK),)
$(error SRTE_SDK is not defined)
endif
ifeq ($(wildcard $(SRTE_SDK)),)
$(error SRTE_SDK variable points to an invalid location)
endif

# define Q to '@' or not. $(Q) is used to prefix all shell commands to
# be executed silently.
Q=@
ifeq '$V' '0'
override V=
endif
ifdef V
ifeq ("$(origin V)", "command line")
Q=
endif
endif
export Q

# if we are building SDK, only includes SDK configuration
ifneq ($(BUILDING_SRTE_SDK),)
  include $(SRTE_OUTPUT)/.config
  # remove double-quotes from config names
  SRTE_ARCH := $(CONFIG_SRTE_ARCH:"%"=%)
  SRTE_MACHINE := $(CONFIG_SRTE_MACHINE:"%"=%)
  SRTE_EXEC_ENV := $(CONFIG_SRTE_EXEC_ENV:"%"=%)
  SRTE_TOOLCHAIN := $(CONFIG_SRTE_TOOLCHAIN:"%"=%)
  SRTE_SDK_BIN := $(SRTE_OUTPUT)
endif

SRTE_TARGET ?= $(SRTE_ARCH)-$(SRTE_MACHINE)-$(SRTE_EXEC_ENV)-$(SRTE_TOOLCHAIN)

ifeq ($(BUILDING_SRTE_SDK),)
# if we are building an external app/lib, include internal/rte.extvars.mk that will
# define SRTE_OUTPUT, SRTE_SRCDIR, SRTE_EXTMK, SRTE_SDK_BIN, (etc ...)
include $(SRTE_SDK)/mk/internal/rte.extvars.mk
endif

ifeq ($(SRTE_ARCH),)
$(error SRTE_ARCH is not defined)
endif

ifeq ($(SRTE_MACHINE),)
$(error SRTE_MACHINE is not defined)
endif

ifeq ($(SRTE_EXEC_ENV),)
$(error SRTE_EXEC_ENV is not defined)
endif

ifeq ($(SRTE_TOOLCHAIN),)
$(error SRTE_TOOLCHAIN is not defined)
endif

# can be overridden by make command line or exported environment variable
SRTE_KERNELDIR ?= /lib/modules/$(shell uname -r)/build

export SRTE_TARGET
export SRTE_ARCH
export SRTE_MACHINE
export SRTE_EXEC_ENV
export SRTE_TOOLCHAIN

# developer build automatically enabled in a git tree
ifneq ($(wildcard $(SRTE_SDK)/.git),)
SRTE_DEVEL_BUILD ?= y
endif

# SRCDIR is the current source directory
ifdef S
SRCDIR := $(abspath $(SRTE_SRCDIR)/$(S))
else
SRCDIR := $(SRTE_SRCDIR)
endif

# helper: return y if option is set to y, else return an empty string
testopt = $(if $(strip $(subst y,,$(1)) $(subst $(1),,y)),,y)

# helper: return an empty string if option is set, else return y
not = $(if $(strip $(subst y,,$(1)) $(subst $(1),,y)),,y)

ifneq ($(wildcard $(SRTE_SDK)/mk/target/$(SRTE_TARGET)/rte.vars.mk),)
include $(SRTE_SDK)/mk/target/$(SRTE_TARGET)/rte.vars.mk
else
include $(SRTE_SDK)/mk/target/generic/rte.vars.mk
endif
