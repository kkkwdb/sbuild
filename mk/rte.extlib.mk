# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

MAKEFLAGS += --no-print-directory

EXTLIB_BUILD := y

# we must create the output dir first and recall the same Makefile
# from this directory
ifeq ($(NOT_FIRST_CALL),)

NOT_FIRST_CALL = 1
export NOT_FIRST_CALL

all:
	$(Q)mkdir -p $(SRTE_OUTPUT)
	$(Q)[ -e $(SRTE_OUTPUT)/.config ] || [ ! -e config ] || cp config $(SRTE_OUTPUT)/.config
	$(Q)$(MAKE) -C $(SRTE_OUTPUT) -f $(SRTE_EXTMK) \
		S=$(SRTE_SRCDIR) O=$(SRTE_OUTPUT) SRCDIR=$(SRTE_SRCDIR)

%::
	$(Q)mkdir -p $(SRTE_OUTPUT)
	$(Q)$(MAKE) -C $(SRTE_OUTPUT) -f $(SRTE_EXTMK) $@ \
		S=$(SRTE_SRCDIR) O=$(SRTE_OUTPUT) SRCDIR=$(SRTE_SRCDIR)
else
include $(SRTE_SDK)/mk/rte.lib.mk
endif
