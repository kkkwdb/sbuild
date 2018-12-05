#ifndef __CFG_FILE_H__
#define __CFG_FILE_H__

/////  宏定义  ///

///最大的配置文件中每行的长度
#define MAX_CFG_LINE_LEN 512
#define KERWORD_SPLIT_MARK '.' ///<关键字的分隔符
#define MAX_KEYWORD_LEN 64
#define rdtscll(val) do { \
    unsigned int __a,__d; \
    asm volatile("rdtsc" : "=a" (__a), "=d" (__d)); \
    (val) = ((unsigned long)__a) | (((unsigned long)__d)<<32); \
} while(0)

////// 函数 /////////////

///@brief 检查文件是否为空
///@param file 配置文件的路径
///@return 如果正常，则返回0；否则返回-1；
int check_file_empty(char *file);

///@brief 从配置文件中读取一个关键字。
///@param file的 配置文件的路径
///@param keyword  读取的关键字，关键字之间的层次关系用‘KERWORD_SPLIT_MARK’表示。
///@note 在文本文件中，不支持文件内容的分段，不支持关键字层次。
///@param [out] value_str 保存返回关键字字符串的缓冲区
///@param len  字符串缓冲区长度
///@return 成功：1，失败：-1。
int cfg_file_read(char *file, char *keyword, char *value_str, int len);

///@brief 从配置文件中读取一个关键字。
///@param file的 配置文件的路径
///@param keyword  读取的关键字，关键字之间的层次关系用‘KERWORD_SPLIT_MARK’表示。
///@note 在文本文件中，不支持文件内容的分段，不支持关键字层次。
///@param [out] value_str 保存返回关键字字符串的缓冲区
///@param len  字符串缓冲区长度
///@return 成功：1，失败：-1。
int cfg_file_read_no_open(char *file, char *keyword, char *value_str, int len);

///@brief 向配置文件中写入一个关键字。
///@param file的 配置文件的路径
///@param keyword  要修改的关键字，关键字之间的层次关系用‘KERWORD_SPLIT_MARK’表示。
///@note 在文本文件中，不支持文件内容的分段，不支持关键字层次。
///@param value_str 关键字的新值
///@return 成功：1，失败：-1。
int cfg_file_write(char *file, char *keyword, char *value_str);

///@brief 创建空的xml配置文件
///@param doc
///@return 成功：；失败：NULL。
void cfg_file_create(char *file);

void cfg_file_close(char *file);

#endif //__CFG_FILE_H__

