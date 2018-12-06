SRTE_SDK ?= $(abspath ../..)
SRTE_TARGET ?= x86_64-native-linuxapp-gcc
export SRTE_SDK SRTE_TARGET

SRTE_DIR_ONE_PROJECT = 1

include $(SRTE_SDK)/mk/rte.vars.mk
DIRS-y := slogin slogind
include $(SRTE_SDK)/mk/rte.extsubdir.mk
