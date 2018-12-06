# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2015 RehiveTech. All rights reserved.

ARCH  ?= arm
CROSS ?=

CPU_CFLAGS  ?= -marm -munaligned-access
CPU_LDFLAGS ?=
CPU_ASFLAGS ?= -felf

export ARCH CROSS CPU_CFLAGS CPU_LDFLAGS CPU_ASFLAGS

SRTE_OBJCOPY_TARGET = elf32-littlearm
SRTE_OBJCOPY_ARCH = arm

export SRTE_OBJCOPY_TARGET SRTE_OBJCOPY_ARCH
