# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# directory where sources are located
#
ifdef S
ifeq ("$(origin S)", "command line")
SRTE_SRCDIR := $(abspath $(S))
endif
endif
SRTE_SRCDIR  ?= $(CURDIR)
export SRTE_SRCDIR

#
# Makefile to call once $(SRTE_OUTPUT) is created
#
ifdef M
ifeq ("$(origin M)", "command line")
SRTE_EXTMK := $(abspath $(M))
endif
endif
SRTE_EXTMK ?= $(SRTE_SRCDIR)/$(notdir $(firstword $(MAKEFILE_LIST)))
export SRTE_EXTMK

# SRTE_SDK_BIN must point to .config, include/ and lib/.
SRTE_SDK_BIN := $(SRTE_SDK)/$(SRTE_TARGET)
ifeq ($(wildcard $(SRTE_SDK_BIN)/.config),)
$(error Cannot find .config in $(SRTE_SDK_BIN))
endif

#
# Output files wil go in a separate directory: default output is
# $(SRTE_SRCDIR)/build
# Output dir can be given as command line using "O="
#
ifdef O
ifeq ("$(origin O)", "command line")
SRTE_OUTPUT := $(abspath $(O))
endif
endif
SRTE_OUTPUT ?= $(SRTE_SRCDIR)/build
export SRTE_OUTPUT

# if we are building an external application, include SDK
# configuration and include project configuration if any
include $(SRTE_SDK_BIN)/.config
ifneq ($(wildcard $(SRTE_OUTPUT)/.config),)
  include $(SRTE_OUTPUT)/.config
endif
# remove double-quotes from config names
SRTE_ARCH := $(CONFIG_SRTE_ARCH:"%"=%)
SRTE_MACHINE := $(CONFIG_SRTE_MACHINE:"%"=%)
SRTE_EXEC_ENV := $(CONFIG_SRTE_EXEC_ENV:"%"=%)
SRTE_TOOLCHAIN := $(CONFIG_SRTE_TOOLCHAIN:"%"=%)
