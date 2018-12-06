# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# install helper .mk

#
# generate rules to install files in SRTE_OUTPUT.
#
# arg1: relative install dir in SRTE_OUTPUT
# arg2: relative file name in a source dir (VPATH)
#
define install_rule
$(addprefix $(SRTE_OUTPUT)/$(1)/,$(notdir $(2))): $(2)
	@echo "  INSTALL-FILE $(addprefix $(1)/,$(notdir $(2)))"
	@[ -d $(SRTE_OUTPUT)/$(1) ] || mkdir -p $(SRTE_OUTPUT)/$(1)
	$(Q)cp -rf $$(<) $(SRTE_OUTPUT)/$(1)
endef

$(foreach dir,$(INSTALL-DIRS-y),\
	$(foreach file,$(INSTALL-y-$(dir)),\
		$(eval $(call install_rule,$(dir),$(file)))))


#
# generate rules to install symbolic links of files in SRTE_OUTPUT.
#
# arg1: relative install dir in SRTE_OUTPUT
# arg2: relative file name in a source dir (VPATH)
#
define symlink_rule
$(addprefix $(SRTE_OUTPUT)/$(1)/,$(notdir $(2))): $(2)
	@echo "  SYMLINK-FILE $(addprefix $(1)/,$(notdir $(2)))"
	@[ -d $(SRTE_OUTPUT)/$(1) ] || mkdir -p $(SRTE_OUTPUT)/$(1)
	$(Q)ln -nsf `$(SRTE_SDK)/buildtools/relpath.sh $$(<) $(SRTE_OUTPUT)/$(1)` \
		$(SRTE_OUTPUT)/$(1)
endef

$(foreach dir,$(SYMLINK-DIRS-y),\
	$(foreach file,$(SYMLINK-y-$(dir)),\
		$(eval $(call symlink_rule,$(dir),$(file)))))


# fast way, no need to do preinstall and postinstall
ifeq ($(PREINSTALL)$(POSTINSTALL),)

_postinstall: $(_INSTALL)
	@touch _postinstall

else # slower way

_preinstall: $(PREINSTALL)
	@touch _preinstall

ifneq ($(_INSTALL),)
$(_INSTALL): _preinstall
else
_INSTALL = _preinstall
endif

_install: $(_INSTALL)
	@touch _install

ifneq ($(POSTINSTALL),)
$(POSTINSTALL): _install
else
POSTINSTALL = _install
endif

_postinstall: $(POSTINSTALL)
	@touch _postinstall
endif
