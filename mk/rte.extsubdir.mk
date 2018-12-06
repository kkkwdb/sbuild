# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2014 6WIND S.A.

MAKEFLAGS += --no-print-directory

ifeq ($(NOT_FIRST_CALL),)

NOT_FIRST_CALL = 1
export NOT_FIRST_CALL

all clean distclean:
ifeq ($(SRTE_DIR_ONE_PROJECT),1)
	$(Q)mkdir -p $(SRTE_OUTPUT)
	$(Q)cd $(SRTE_OUTPUT);mkdir -p $(DIRS-y)
	$(Q)$(MAKE) -C $(SRTE_OUTPUT) -f $(SRTE_EXTMK) \
		S=$(SRTE_SRCDIR) O=$(SRTE_OUTPUT) SRCDIR=$(SRTE_SRCDIR) $@
else
	$(Q)$(MAKE) O=$(CURDIR) $@
endif

else

include $(SRTE_SDK)/mk/internal/rte.compile-pre.mk
include $(SRTE_SDK)/mk/internal/rte.install-pre.mk
include $(SRTE_SDK)/mk/internal/rte.clean-pre.mk
include $(SRTE_SDK)/mk/internal/rte.build-pre.mk

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

OUTPUT=$(SRTE_SRCDIR)/$(@)/build
MKGOALS=$(MAKECMDGOALS)
ifeq ($(SRTE_DIR_ONE_PROJECT),1)
OUTPUT=$(SRTE_OUTPUT)/$(@)
MKGOALS=$(subst distclean,clean,$(MAKECMDGOALS))
endif

.PHONY: $(DIRS-y)
$(DIRS-y):
	@echo "== $@"
	$(Q)mkdir -p $(SRTE_OUTPUT)/$(@)/build
	$(Q)$(MAKE) -C $(SRTE_OUTPUT)/$(@)/build \
		-f $(SRTE_SRCDIR)/$(@)/Makefile \
		O=$(OUTPUT) \
		S=$(SRTE_SRCDIR)/$(@) \
		SRCDIR=$(SRTE_SRCDIR)/$(@) \
		$(filter-out $(DIRS-y),$(MKGOALS))

.PHONY: clean
clean: _postclean
	@rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

.PHONY: distclean
distclean: clean
ifeq ($(SRTE_DIR_ONE_PROJECT),1)
	-@rm -rf $(SRTE_OUTPUT)
endif
	@true

define depdirs_rule
$(DEPDIRS-$(1)):

$(1): | $(DEPDIRS-$(1))

$(if $(D),$(info $(1) depends on $(DEPDIRS-$(1))))
endef

$(foreach dir,$(ALL_DEPDIRS),\
	$(eval $(call depdirs_rule,$(dir))))

include $(SRTE_SDK)/mk/internal/rte.compile-post.mk
include $(SRTE_SDK)/mk/internal/rte.install-post.mk
include $(SRTE_SDK)/mk/internal/rte.clean-post.mk
include $(SRTE_SDK)/mk/internal/rte.build-post.mk

.PHONY: FORCE
FORCE:

endif
