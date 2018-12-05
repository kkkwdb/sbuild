RTE_SDK ?= $(abspath ../..)
RTE_TARGET ?= x86_64-native-linuxapp-gcc
export RTE_SDK RTE_TARGET

RTE_DIR_ONE_PROJECT = 1

DIRS-y := main cfgfile

include $(RTE_SDK)/mk/rte.vars.mk

POSTBUILD := test_cfgfile
POSTCLEAN := clean_test_cfgfile
include $(RTE_SDK)/mk/rte.extsubdir.mk

$(POSTBUILD):
	$(Q)$(CC) $(RTE_OUTPUT)/cfgfile/build/cfgfile.o $(RTE_OUTPUT)/main/build/main.o -o $@
$(POSTCLEAN):
	$(Q)$(RM) test_cfgfile
