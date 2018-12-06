SRTE_SDK ?= $(abspath ../..)
SRTE_TARGET ?= x86_64-native-linuxapp-gcc
export SRTE_SDK SRTE_TARGET

SRTE_DIR_ONE_PROJECT = 1

DIRS-y := main cfgfile

include $(SRTE_SDK)/mk/rte.vars.mk

POSTBUILD := test_cfgfile
POSTCLEAN := clean_test_cfgfile
include $(SRTE_SDK)/mk/rte.extsubdir.mk

$(POSTBUILD):
	$(Q)$(CC) $(SRTE_OUTPUT)/cfgfile/build/cfgfile.o $(SRTE_OUTPUT)/main/build/main.o -o $@
$(POSTCLEAN):
	$(Q)$(RM) test_cfgfile
