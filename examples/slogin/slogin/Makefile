# sdk编译环境目录，包含mk文件
RTE_SDK ?= $(abspath ../../..)
# 编译外部程序必须指定RTE_TARGET 
RTE_TARGET ?= x86_64-native-linuxapp-gcc
export RTE_SDK RTE_TARGET

# 程序名和源文件
APP = slogin
SRCS-y := slogin.c

# 定义RTE_OUTPUT CFLAGS LDFLAGS WERROR_FLAGS等变量
include $(RTE_SDK)/mk/rte.vars.mk
CFLAGS += -O3
LD_FLAGS += -lpthread

# 定义build目标 ，建立依赖
include $(RTE_SDK)/mk/rte.extapp.mk
