# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2014 6WIND S.A.

MAKEFLAGS += --no-print-directory

ifeq ($(NOT_FIRST_CALL),)

NOT_FIRST_CALL = 1
export NOT_FIRST_CALL

all clean distclean:
ifeq ($(RTE_DIR_ONE_PROJECT),1)
	$(Q)mkdir -p $(RTE_OUTPUT)
	$(Q)cd $(RTE_OUTPUT);mkdir -p $(DIRS-y)
	$(Q)$(MAKE) -C $(RTE_OUTPUT) -f $(RTE_EXTMK) \
		S=$(RTE_SRCDIR) O=$(RTE_OUTPUT) SRCDIR=$(RTE_SRCDIR) $@
else
	$(Q)$(MAKE) O=$(CURDIR) $@
endif

else

include $(RTE_SDK)/mk/internal/rte.compile-pre.mk
include $(RTE_SDK)/mk/internal/rte.install-pre.mk
include $(RTE_SDK)/mk/internal/rte.clean-pre.mk
include $(RTE_SDK)/mk/internal/rte.build-pre.mk

ALL_DEPDIRS := $(patsubst DEPDIRS-%,%,$(filter DEPDIRS-%,$(.VARIABLES)))

# output directory

_CLEAN = $(DIRS-y)
_BUILD = $(DIRS-y)
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y)

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

OUTPUT=$(RTE_SRCDIR)/$(@)/build
MKGOALS=$(MAKECMDGOALS)
ifeq ($(RTE_DIR_ONE_PROJECT),1)
OUTPUT=$(RTE_OUTPUT)/$(@)
MKGOALS=$(subst distclean,clean,$(MAKECMDGOALS))
endif

.PHONY: $(DIRS-y)
$(DIRS-y):
	@echo "== $@"
	$(Q)mkdir -p $(RTE_OUTPUT)/$(@)/build
	$(Q)$(MAKE) -C $(RTE_OUTPUT)/$(@)/build \
		-f $(RTE_SRCDIR)/$(@)/Makefile \
		O=$(OUTPUT) \
		S=$(RTE_SRCDIR)/$(@) \
		SRCDIR=$(RTE_SRCDIR)/$(@) \
		$(filter-out $(DIRS-y),$(MKGOALS))

.PHONY: clean
clean: _postclean
	@rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

.PHONY: distclean
distclean: clean
ifeq ($(RTE_DIR_ONE_PROJECT),1)
	-@rm -rf $(RTE_OUTPUT)
endif
	@true

define depdirs_rule
$(DEPDIRS-$(1)):

$(1): | $(DEPDIRS-$(1))

$(if $(D),$(info $(1) depends on $(DEPDIRS-$(1))))
endef

$(foreach dir,$(ALL_DEPDIRS),\
	$(eval $(call depdirs_rule,$(dir))))

include $(RTE_SDK)/mk/internal/rte.compile-post.mk
include $(RTE_SDK)/mk/internal/rte.install-post.mk
include $(RTE_SDK)/mk/internal/rte.clean-post.mk
include $(RTE_SDK)/mk/internal/rte.build-post.mk

.PHONY: FORCE
FORCE:

endif
