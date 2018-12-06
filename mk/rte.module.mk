# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

##### if sourced from kernel Kbuild system
ifneq ($(KERNELRELEASE),)
override EXTRA_CFLAGS = $(MODULE_CFLAGS) $(EXTRA_KERNEL_CFLAGS)
obj-m          += $(MODULE).o
ifneq ($(MODULE),$(notdir $(SRCS-y:%.c=%)))
$(MODULE)-objs += $(notdir $(SRCS-y:%.c=%.o))
endif

##### if launched from rte build system
else

include $(SRTE_SDK)/mk/internal/rte.install-pre.mk
include $(SRTE_SDK)/mk/internal/rte.clean-pre.mk
include $(SRTE_SDK)/mk/internal/rte.build-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

_BUILD = $(MODULE).ko
_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y) \
	$(SRTE_OUTPUT)/kmod/$(MODULE).ko
_CLEAN = doclean

SRCS_LINKS = $(addsuffix _link,$(SRCS-y))

compare = $(strip $(subst $(1),,$(2)) $(subst $(2),,$(1)))

.PHONY: all
all: install

.PHONY: install
install: build _postinstall

_postinstall: build

.PHONY: build
build: _postbuild

# Link all sources in build directory
%_link: FORCE
	$(if $(call compare,$(notdir $*),$*),\
	@if [ ! -f $(notdir $(*)) ]; then ln -nfs $(SRCDIR)/$(*) . ; fi,\
	@if [ ! -f $(notdir $(*)) ]; then ln -nfs $(SRCDIR)/$(*) . ; fi)

# build module
$(MODULE).ko: $(SRCS_LINKS)
	@if [ ! -f $(notdir Makefile) ]; then ln -nfs $(SRCDIR)/Makefile . ; fi
	@$(MAKE) -C $(SRTE_KERNELDIR) M=$(CURDIR) O=$(SRTE_KERNELDIR) \
		CC="$(KERNELCC)" CROSS_COMPILE=$(CROSS) V=$(if $V,1,0)

# install module in $(SRTE_OUTPUT)/kmod
$(SRTE_OUTPUT)/kmod/$(MODULE).ko: $(MODULE).ko
	@echo INSTALL-MODULE $(MODULE).ko
	@[ -d $(SRTE_OUTPUT)/kmod ] || mkdir -p $(SRTE_OUTPUT)/kmod
	@cp -f $(MODULE).ko $(SRTE_OUTPUT)/kmod

# install module
modules_install:
	@$(MAKE) -C $(SRTE_KERNELDIR) M=$(CURDIR) O=$(SRTE_KERNELDIR) \
		modules_install

.PHONY: clean
clean: _postclean

# do a make clean and remove links
.PHONY: doclean
doclean:
	@if [ ! -f $(notdir Makefile) ]; then ln -nfs $(SRCDIR)/Makefile . ; fi
	$(Q)$(MAKE) -C $(SRTE_KERNELDIR) M=$(CURDIR) O=$(SRTE_KERNELDIR) clean
	@$(foreach FILE,$(SRCS-y) $(SRCS-n) $(SRCS-),\
		if [ -h $(notdir $(FILE)) ]; then rm -f $(notdir $(FILE)) ; fi ;)
	@if [ -h $(notdir Makefile) ]; then rm -f $(notdir Makefile) ; fi
	@rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS) \
		$(INSTALL-FILES-all)

include $(SRTE_SDK)/mk/internal/rte.install-post.mk
include $(SRTE_SDK)/mk/internal/rte.clean-post.mk
include $(SRTE_SDK)/mk/internal/rte.build-post.mk

.PHONY: FORCE
FORCE:

endif
