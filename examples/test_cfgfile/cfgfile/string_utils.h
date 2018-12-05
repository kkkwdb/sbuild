////////////////////////////////////////
///@brief 一些常用的字符串操作。
///
/// 修改记录:
///
///@author liuzhh@sugon.com
///@date 2012-08-30
///
///在编写测试工具过程中，增加一些字符串操作
///包括：切分、合并、填充字符串，16进制的字符串和u8型数组转换
///
///@author liuzhh@dawning.com.cn
///@date 2012-02-01
///
///建立文档
///
////////////////////////////////////////

#ifndef _STRING_UTILS_H__
#define _STRING_UTILS_H__


//////// 头文件  ////////////////

#ifndef BUILD_NETOS
#include <linux/types.h>  //__u8
#else
#include "ppb_types.h"
#endif

///////   函数列表  ///////////////////////


///@brief 删除字符串中的空格、注释和末尾回车
///
/// 详细流程：
/// 用下标i,j指向字符串第一个字符,通过一个while循环,i对整个字符串扫描,
/// 如果i对应的位置是空格,则i继续向前扫描,
/// 如果i对应的位置非空格,
/// 则把i位置的字符向前移动到j位置,i和j同时向前一个移动字符,
/// 如果i遇到回车,扫描结束。
/// 最后把j位置作为字符串结束,返回j即为新串的长度。
///@param [out] str 字符串指针(输入输出)
///@return  删除空格后的字符串长度
inline int del_space(char *str);

inline int del_unusedspace(char *str);

///@brief 把整数转换成字符串
///@param [out] str 转换成的字符串。
///@param i 要转换的整数
///@param base 转换进制
void itostr(char *str, int i, int base);

///@brief 安全的字符串拷贝函数
///@return 1：成功；-1：拷贝时发生了截断
int safe_strcpy(char *dest, char *src, int dest_buf_len);

///@brief 将字符串切分为整数数组
///@param str 待切分字符串，函数执行后该字符串被修改
///@param delims 间隔符
///@param array [out] 结果数组
///@param num 要获取的整数数量，不大于array数组的容量
///@return -1：参数错误，-2：待切割字符串含有非法字符，正数：array数组中的结果数量
int splitstr2intarray(char *str, char *delims, int *array, int num);

///@brief 将整数数组合并为用间隔符连接的字符串
///@param str [out] 合并得到的字符串
///@param len str的最大长度，不包括结束符
///@param delims 间隔符
///@param array 待合并数组
///@param num 要合并的整数数量
///@return -1：出错，正数：合并的整数数量
int intarray2str(char *str, int len, char *delims, int *array, int num);

///@brief 对用户显示的mac字符串形式转换为6字节形式
///@param str 字符串形式的MAC地址
///@param bytes 6字节存储的MAC地址
///@return -1：参数错误，0：成功
int mac_str2bytes(char *str, unsigned char *bytes);

///@brief 将字符串指针数组中的多个字符串，合并为用间隔符连接的一个字符串
///@param str [out] 合并得到的字符串
///@param str_parts 要合并的字符串片段数组
///@param delim_str 间隔符
///@param num 要合并的片段数量
///@return -1：出错，1：成功
int combine_str(char *str, char *str_parts[], char *delim_str, int parts_num);

///@brief 将间隔符连接的一个字符串分隔，保存为字符串指针数组中的多个字符串
///@warning 被分割的字符串如果不能修改，需要调用该函数前先复制一份。
///@param str [out] 要分割的字符串，分割后字符串内容会被修改
///@param str_parts [out] 分割后的字符串片段数组
///@param delim_c 间隔符
///@return 出错，-1; 成功:分割出的片段数
int split_str(char *str, char *str_parts[], char delim_c);

///@brief 把字符串填充到固定长度
///@param str 要填充的字符串
///@param len 要填充到的长度
///@param c 用来填充的字符
void fill_str(char *str, int len, char c);


///@brief 把16进制的字符串转换为u8型数组
///比如字符串“3132”，转换为数组{31,32}
///@param str 字符串
///@param arr 数组
int hexstr2u8array(char *str, __u8 *arr);    

///@brief 把16进制的字符串转换为u8型数组
///比如数组{31,32}，转换为字符串“3132”
///@param arr 数组
///@param arr_len 数组长度
///@param str 字符串
int u8array2hexstr(__u8 *arr, int arr_len, char *str);
    
#endif //_STRING_UTILS_H__
