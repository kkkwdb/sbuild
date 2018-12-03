# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2014 6WIND S.A.

MAKEFLAGS += --no-print-directory

ALL_DEPDIRS := $(patsubst DEPDIRS-%,%,$(filter DEPDIRS-%,$(.VARIABLES)))

# output directory
O ?= $(CURDIR)
BASE_OUTPUT ?= $(abspath $(O))
CUR_SUBDIR ?= .

.PHONY: all
all: $(DIRS-y)

.PHONY: clean
clean: $(DIRS-y)

OUTPUT=$(BASE_OUTPUT)/$(CUR_SUBDIR)/$(@)/build
MKGOALS=$(MAKECMDGOALS)
ifeq ($(RTE_DIR_ONE_PROJECT),1)
OUTPUT=$(RTE_OUTPUT)
MKGOALS=$(subst distclean,clean,$(MAKECMDGOALS))
endif

.PHONY: $(DIRS-y)
$(DIRS-y):
	@echo "== $@"
	$(Q)$(MAKE) -C $(@) \
		M=$(CURDIR)/$(@)/Makefile \
		BASE_OUTPUT=$(BASE_OUTPUT) \
		O=$(OUTPUT) \
		CUR_SUBDIR=$(CUR_SUBDIR)/$(@) \
		S=$(CURDIR)/$(@) \
		$(filter-out $(DIRS-y),$(MKGOALS))

.PHONY: distclean
distclean: clean
ifeq ($(RTE_DIR_ONE_PROJECT),1)
	-@rm -rf $(RTE_OUTPUT)/app
	-@rmdir $(RTE_OUTPUT)
endif
	@true

define depdirs_rule
$(DEPDIRS-$(1)):

$(1): | $(DEPDIRS-$(1))

$(if $(D),$(info $(1) depends on $(DEPDIRS-$(1))))
endef

$(foreach dir,$(ALL_DEPDIRS),\
	$(eval $(call depdirs_rule,$(dir))))
