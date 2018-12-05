///////////////////////////////////////////
///@brief 调试信息打印封装接口
///
///修改记录：
///@author xujw@sugon.com
///@date 2011-07-06
///
///建立文档
////////////////////////////////////////////
#ifndef __DBG_H__
#define __DBG_H__

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifndef BUILD_NETOS
#include <execinfo.h> //backtrace
#endif
#include <pthread.h>

#define DEBUG_LEVEL_FATAL   0
#define DEBUG_LEVEL_ERR     1
#define DEBUG_LEVEL_WARN    2    
#define DEBUG_LEVEL_INFO    4 
#define DEBUG_LEVEL_VERB    5
#define DEBUG_LEVEL_SPEW    6
#define STACK_DEP 8

extern void debug_me();
extern int glob_nf_log_level;

///////////// 关键位置输出 //////////////////

///@brief assert函数
///@pre OUTPUT_ENABLED使能
#if 0
#define nf_assert(cond)  \
do \
{ \
   assert(cond);  \
} \
while(0)
#endif

#define nf_assert(cond)

#define nf_log(fmt, args...)  fprintf(stdout, "%s(): "fmt, __FUNCTION__, ## args)
#define nf_log_file(fp, fmt, args...) fprintf(fp, "%s(): "fmt, __FUNCTION__, ## args)

#ifdef OUTPUT_ENABLED
#define nf_log_msg(level, fmt, args...)\
do\
{\
    if(glob_nf_log_level >= level) \
    {\
        nf_log(fmt, ## args); \
    }\
}while(0)

///@brief 将log信息输出到文件
#define nf_log_msg2(level, fp, fmt, args...)\
do\
{\
    if(glob_nf_log_level >= level) \
    {\
        nf_log_file(fp, fmt, ## args); \
    }\
}while(0)

#define nf_thread_log_msg(level, fmt, args...) \
do\
{\
    if(glob_nf_log_level >= level) \
    {\
        nf_log("Thread %lx " fmt, pthread_self(), ## args); \
    }\
}while(0)

#define nf_thread_log_msg2(level, fp, fmt, args...) \
do\
{\
    if(glob_nf_log_level >= level) \
    {\
        nf_log_file(fp, "Thread %lx " fmt, pthread_self(), ## args); \
    }\
}while(0)


#else
#define nf_log_msg(level, fmt, args...)
#define nf_log_msg2(level, fp, fmt, args...)
#define nf_thread_log_msg(level, fmt, args...)
#define nf_thread_log_msg2(level, fp, fmt, args...)
#endif


#ifndef BUILD_NETOS
///@brief 打印调用栈，通过对比反汇编文件可以跟踪调用关系
static inline void bt(void)
{
    void *buffer[STACK_DEP];
    int i = 0;
    size_t size;
    char **strings = NULL;

    size = backtrace(buffer, STACK_DEP);
    strings = (char **)(unsigned long)backtrace_symbols(buffer, size);

    for (i = 0; i < size; ++i) 
    {
        printf("%s\n", strings[i]);
    }
    free(strings);
}
#else
#define bt()
#endif

///@brief 全局程序退出函数
#define nf_exit(err_code) exit(err_code)

////////////   跟踪用 ////////
#define err(fmt, args...) \
do{ \
    fprintf(stdout, "Err: %s "fmt, __FUNCTION__, ##args); \
    bt();  \
    nf_exit(-1); \
}while(0)

#define thread_err(fmt, args...)\
do{ \
    fprintf(stdout, "Err: %lx %s "fmt, pthread_self(), __FUNCTION__, ##args);\
    bt(); \
    nf_exit(-1); \
}while(0)

#define warn(fmt, args...) fprintf(stdout, "Warn: %s "fmt, __FUNCTION__, ##args)
#define thread_warn(fmt, args...) fprintf(stdout, "Warn: %lx %s "fmt, pthread_self(), __FUNCTION__, ##args)

#define spew(fmt, args...) fprintf(stdout, "Spew: %s "fmt, __FUNCTION__, ##args)
#define thread_spew(fmt, args...) fprintf(stdout, "Spew: %lx %s "fmt, pthread_self(), __FUNCTION__, ##args)
///@brief 函数调用，跟踪程序执行流程时使用
#define func_enter() \
do \
{ \
    spew("enter."); \
} \
while (0)

///@brief 无返回值函数退出，跟踪程序执行流程时使用
#define func_return() \
do \
{ \
    spew("return."); \
	return; \
} \
while (0)

///@brief 返回值为整型函数退出，跟踪程序执行流程时使用
#define func_return_int(ret) \
do \
{ \
    spew("return %d.", ret); \
    return ret; \
} \
while (0)

///@brief 返回值为指针型函数退出，跟踪程序执行流程时使用
#define func_return_ptr(ret) \
do \
{ \
    spew("return %p.", ret); \
    return ret; \
} \
while (0)

/* pthread version*/
///@brief 线程版func_enter
#define thread_func_enter() \
do \
{ \
    thread_spew("enter."); \
} \
while (0)

///@brief 线程版func_return 
#define thread_func_return() \
do \
{ \
    thread_spew("return."); \
	return; \
} \
while (0)

///@brief 线程版func_return_int
#define thread_func_return_int(ret) \
do \
{ \
    thread_spew("return %d.", ret); \
    return ret; \
} \
while (0)

///@brief 线程版func_return_ptr
#define thread_func_return_ptr(ret) \
do \
{ \
    thread_spew("return %p.", ret); \
    return ret; \
} \
while (0)

/* action/condition versions */
///@brief 警告并返回错误码
#define warn_ret(eno, fmt, args...) do { \
    warn(fmt, ## args); \
    func_return_int (eno); \
} while (0)

///@brief 警告并返回
#define warn_ret_void(fmt, args...) \
do \
{ \
    warn(fmt, ## args); \
    func_return (); \
} \
while (0)

///@brief 条件返回
#define ret_on(cond, eno) do \
{ \
    if (cond) \
    { \
        func_return (eno); \
    } \
} \
while (0)

///@brief 条件返回，返回值为整数
#define ret_int_on(cond, eno) do \
{ \
    if (cond) \
    { \
        func_return_int (eno); \
    } \
} \
while (0)

///@brief 条件返回，返回值为指针
#define ret_ptr_on(cond, eno) do \
{ \
    if (cond) \
    { \
        func_return_ptr(eno); \
    } \
} \
while (0)

///@brief 条件退出
#define exit_on(cond, eno) \
do \
{ \
    if (cond) \
    { \
        nf_exit((eno)); \
    } \
} \
while (0)

///@brief 条件错误退出
#define err_on(cond, fmt, args...) \
do \
{ \
    if (cond) \
    { \
        err(fmt, ## args); \
    } \
} \
while (0)

///@brief 条件警告
#define warn_on(cond, fmt, args...) \
do \
{ \
    if (cond) \
    { \
        warn(fmt, ## args); \
    } \
} \
while (0)

///@brief 条件警告
#define info_on(cond, fmt, args...) \
do \
{ \
    if (cond) \
    { \
        info(fmt, ## args); \
    } \
} \
while (0)

///@brief 如果满足条件，则调用debug_me哑函数，通过debug_me来反查调用栈
#define debug_on(cond) \
do \
{ \
    if (cond) \
    { \
        debug_me(); \
    } \
} \
while (0)

#endif /*__DBG_H___*/

