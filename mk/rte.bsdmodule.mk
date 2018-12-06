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

# sbuild uses a more up-to-date gcc, so clear the override here.
unexport CC
override CFLAGS = $(MODULE_CFLAGS)

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
	$(Q)if [ ! -f $(notdir $(*)) ]; then ln -nfs $(SRCDIR)/$(*) . ; fi,\
	$(Q)if [ ! -f $(notdir $(*)) ]; then ln -nfs $(SRCDIR)/$(*) . ; fi)

# build module
$(MODULE).ko: $(SRCS_LINKS)
	$(Q)if [ ! -f $(notdir Makefile) ]; then ln -nfs $(SRCDIR)/Makefile . ; fi
	$(Q)if [ ! -f $(notdir BSDmakefile) ]; then ln -nfs $(SRCDIR)/BSDmakefile . ; fi
	$(Q)MAKEFLAGS= $(BSDMAKE)

# install module in $(SRTE_OUTPUT)/kmod
$(SRTE_OUTPUT)/kmod/$(MODULE).ko: $(MODULE).ko
	$(Q)echo INSTALL-MODULE $(MODULE).ko
	$(Q)[ -d $(SRTE_OUTPUT)/kmod ] || mkdir -p $(SRTE_OUTPUT)/kmod
	$(Q)cp -f $(MODULE).ko $(SRTE_OUTPUT)/kmod

# install module
modules_install:
	$(Q)MAKEFLAGS= $(BSDMAKE) install

.PHONY: clean
clean: _postclean

# do a make clean and remove links
.PHONY: doclean
doclean:
	$(Q)if [ ! -f $(notdir Makefile) ]; then ln -nfs $(SRCDIR)/Makefile . ; fi
	$(Q)$(MAKE) -C $(SRTE_KERNELDIR) M=$(CURDIR) O=$(SRTE_KERNELDIR) clean
	$(Q)$(foreach FILE,$(SRCS-y) $(SRCS-n) $(SRCS-),\
		if [ -h $(notdir $(FILE)) ]; then rm -f $(notdir $(FILE)) ; fi ;)
	$(Q)if [ -h $(notdir Makefile) ]; then rm -f $(notdir Makefile) ; fi
	$(Q)rm -f $(_BUILD_TARGETS) $(_INSTALL_TARGETS) $(_CLEAN_TARGETS) \
		$(INSTALL-FILES-all)

include $(SRTE_SDK)/mk/internal/rte.install-post.mk
include $(SRTE_SDK)/mk/internal/rte.clean-post.mk
include $(SRTE_SDK)/mk/internal/rte.build-post.mk

.PHONY: FORCE
FORCE:

endif
