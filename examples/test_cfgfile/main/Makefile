# sdk编译环境目录，包含mk文件
SRTE_SDK ?= $(abspath ../../..)
# 编译外部程序必须指定SRTE_TARGET 
SRTE_TARGET ?= x86_64-native-linuxapp-gcc
export SRTE_SDK SRTE_TARGET

# 程序名和源文件
SRCS-y := main.c

# 定义SRTE_OUTPUT CFLAGS LDFLAGS WERROR_FLAGS等变量
include $(SRTE_SDK)/mk/rte.vars.mk
CFLAGS += -O3
CFLAGS_main.o += -g -I$(abspath $(SRTE_SRCDIR)/../cfgfile)

# 定义build目标 ，建立依赖
include $(SRTE_SDK)/mk/rte.extobj.mk
