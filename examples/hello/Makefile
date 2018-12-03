# sdk编译环境目录，包含mk文件
RTE_SDK ?= /usr/local/share/sbuild
# 编译外部程序必须指定RTE_TARGET 
RTE_TARGET ?= x86_64-native-linuxapp-gcc

# 程序名和源文件
APP = hello
SRCS-y := main.c

# 定义RTE_OUTPUT CFLAGS LDFLAGS WERROR_FLAGS等变量
include $(RTE_SDK)/mk/rte.vars.mk
CFLAGS += -O3
CFLAGS_main.o+=-g
CFLAGS += $(WERROR_FLAGS) 

# 定义build目标 ，建立依赖
include $(RTE_SDK)/mk/rte.extapp.mk
