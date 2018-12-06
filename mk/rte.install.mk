# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# install-only makefile (no build target)

include $(SRTE_SDK)/mk/internal/rte.install-pre.mk
include $(SRTE_SDK)/mk/internal/rte.clean-pre.mk

# VPATH contains at least SRCDIR
VPATH += $(SRCDIR)

_INSTALL = $(INSTALL-FILES-y) $(SYMLINK-FILES-y)
_CLEAN = doclean

.PHONY: all
all: _postinstall
	@true

.PHONY: clean
clean: _postclean

.PHONY: doclean
doclean:
	@rm -rf $(INSTALL-FILES-all)
	@rm -f $(_INSTALL_TARGETS) $(_CLEAN_TARGETS)

include $(SRTE_SDK)/mk/internal/rte.install-post.mk
include $(SRTE_SDK)/mk/internal/rte.clean-post.mk
