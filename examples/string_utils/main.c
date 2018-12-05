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

#ifndef _STRING_UTILS_C__
#define _STRING_UTILS_C__

//系统路径下头文件
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>

//////// 宏定义  ///////


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
inline int del_space(char *str)
{
    int len;
    int i, j;
    char *p;
    
    ///删除注释
    p = strchr(str, '#');
    if (p != NULL) 
    {
        *p = '\0';
    }
        
    ///获得原始字符串长度
    len = strlen(str); 
    
    ///扫描字符串中每个字符
    i = 0;
    j = 0;   
    while (i < len)
    {
        ///——如果发现回车,结束扫描
        if (str[i] == '\n')    
        {
            break;
        }
        
        ///——如果发现空格,继续扫描
        if ((str[i] == ' ') || (str[i] == '\t')) 
        {
            i++;
            continue;
        }
        
        ///——用非空格字符填充前面的空格
        str[j] = str[i];
        i++;
        j++;
    }
    str[j] = '\0';
    
    ///返回最终字符串长度
    return j;
}

//not delete space
inline int del_unusedspace(char *str)  
{
    int len;
    int i, j;
    char *p;
    
    ///删除注释
    p = strchr(str, '#');
    if (p != NULL) 
    {
        *p = '\0';
    }
        
    ///获得原始字符串长度
    len = strlen(str); 
    
    ///扫描字符串中每个字符
    i = 0;
    j = 0;   
    while (i < len)
    {
        ///——如果发现回车,结束扫描
        if ((str[i] == '\n') || (str[i] == '\0'))    
        {
            break;
        }
        
        ///——如果发现空格,继续扫描
        if (str[i] == '\t')
        {
            i++;
            continue;
        }
        
        ///——用非空格字符填充前面的空格
        str[j] = str[i];
        i++;
        j++;
    }
    str[j] = '\0';
    
    ///返回最终字符串长度
    return j;
}
    
///@brief 把整数转换成字符串
///@param [out] str 转换成的字符串。
///@param i 要转换的整数
///@param base 转换进制
void itostr(char *str, int i, int base)
{
    char tmp[16];
    int j, k;
    
    if (i == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    j = 0;
    while (i)
    {
        tmp[j] = i % base;
        j++;
        i = i / base;
    }
    
    k = 0;
    while (j)
    {
        j--;
        if (tmp[j] < 10)
        {
            str[k] = '0' + tmp[j];
        }
        else
        {
            str[k] = 'a' + tmp[j] - 10;
        }
        k++;
    }
    str[k] = '\0';
}

///@brief 安全的字符串拷贝函数
///@return 1：成功；-1：拷贝时发生了截断
int safe_strcpy(char *dest, char *src, int dest_buf_len)
{
    if (strlen(src) < dest_buf_len)
    {
        strcpy(dest, src);
        return 1;
    }
    
    strncpy(dest, src, dest_buf_len);
    dest[dest_buf_len - 1] = '\0';
    
    return -1;
}

///@brief 将字符串切分为整数数组
///@param str 待切分字符串，函数执行后该字符串被修改
///@param delim_str 间隔符
///@param array [out] 结果数组
///@param num 要获取的整数数量，不大于array数组的容量
///@return -1：参数错误，-2：待切割字符串含有非法字符，正数：array数组中的结果数量
int splitstr2intarray(char *str, char *delim_str, int *array, int num)
{
    int i, j;
    char *p;
    
    if (str == NULL || delim_str == NULL || array == NULL || num < 1)
    {
		return -1;
	}

    i = 0;
    p = strtok(str, delim_str);
    while ((p != NULL) && (i < num))
    {
        ///--atoi返回0的情况有两种：p确实为0；p中含有非整数字符（0-9，+/-）
        array[i] = atoi(p); 
        
        ///--如果p不是整数字符串，则返回-1
        j = 0;
        while ((p != NULL) && ((p[j] == ' ') || (p[j] == '\t')))
        {
            ++p;
            ++j;
        }
        if ((array[i] == 0) && ((p == NULL) || (p[0] != '0')))
        {
            return -2;
        }
        
        ///--处理下一个
        ++i;
        p = strtok(NULL, delim_str);
    }

    return i;
}

///@brief 将整数数组合并为用间隔符连接的字符串
///@param str [out] 合并得到的字符串
///@param len str的最大长度，不包括结束符
///@param delim_str 间隔符
///@param array 待合并数组
///@param num 要合并的整数数量
///@return -1：出错，正数：合并的整数数量
int intarray2str(char *str, int len, char *delim_str, int *array, int num)
{
    int i, int_len, delim_len;
    char *p, *q, buf[16];
 
	///参数合法性检查
	if (str == NULL || delim_str == NULL || array == NULL || num < 1)
	{
		return -1;
	}
	
    ///变量初始化
    q = str + len;
	delim_len = strlen(delim_str);
	
    p = str;
    sprintf(buf, "%d", array[0]);
    int_len = strlen(buf);
    
	///将前num-1个整数合并到字符串中
    for (i = 1; (i < num) && (p + int_len + delim_len - 1 <= q); ++i)
    {
        sprintf(p, "%s%s", buf, delim_str);
        p += strlen(p);
        sprintf(buf, "%d", array[i]);
        int_len = strlen(buf);
    }
    
    ///第num-1个整数合并到字符串中
    if (p + int_len + delim_len - 1 <= q)
    {
		sprintf(p, "%s%s", buf, delim_str);
        p += strlen(p);
	}
	
	///结束符
    *(p - 1) = 0;
    
    return i;
}

///@brief 对用户显示的mac字符串形式转换为6字节形式
///@param str 字符串形式的MAC地址
///@param bytes 6字节存储的MAC地址
///@return -1：参数错误，0：成功
int mac_str2bytes(char *str, unsigned char *bytes)
{
    unsigned int temp[6], i;
    
    if (str == NULL || bytes == NULL)
    {
		return -1;
	}
    
    sscanf(str, "%x:%x:%x:%x:%x:%x", &temp[0], 
            &temp[1], &temp[2], &temp[3], 
            &temp[4], &temp[5]);
            
    for (i = 0; i < 6; ++i)
    {
        bytes[i] = (unsigned char)temp[i];
    }
    
    return 0;
}


///@brief 将字符串指针数组中的多个字符串，合并为用间隔符连接的一个字符串
///@param str [out] 合并得到的字符串
///@param str_parts 要合并的字符串片段数组
///@param delim_str 间隔符
///@param num 要合并的片段数量
///@return -1：出错，1：成功
int combine_str(char *str, char *str_parts[], char *delim_str, int parts_num)
{
    int i;
    
    if (str == NULL || str_parts == NULL)
    {
        return -1;
    }

    
    str[0] = '\0';
    for (i = 0; i < parts_num; i++)
    {
        if (i > 0)
        {
            strcat(str, delim_str);
        }
        strcat(str, str_parts[i]);
    }
    
    return 1;    
}

///@brief 将间隔符连接的一个字符串分隔，保存为字符串指针数组中的多个字符串
///@warning 被分割的字符串如果不能修改，需要调用该函数前先复制一份。
///@param str [out] 要分割的字符串，分割后字符串内容会被修改
///@param str_parts [out] 分割后的字符串片段数组
///@param delim_c 间隔符
///@return 出错，-1; 成功:分割出的片段数
int split_str(char *str, char *str_parts[], char delim_c)
{
    int i;
    char *p, *q;
    
    if (str == NULL || str_parts == NULL)
    {
        return -1;
    }
    
    ///p指向当前为扫描字符串的头，q指向以扫描出的第一个片段尾，循环扫描字符串
    i = 0;
    p = str;
    while (*p != '\0')
    {
        ///--跳过开始的分隔符
        while (*p == delim_c)
        {
            p++;
        }
        
        ///--如果是空串，跳出循环
        if (*p == '\0')
        {
            break;
        }
        
        ///--找到当前片段尾
        q = strchr(p, delim_c);
        
        ///--如果只有一个片段，保存退出
        if (q == NULL)
        {
            str_parts[i] = p;
            i++;
            break;
        }
        
        ///--分割出当前片段
        *q = '\0';
        str_parts[i] = p;
        i++;
        
        ///--str指向下一个片段头
        p = q + 1;  
    }

    return i;    
}

///@brief 把字符串填充到固定长度
///@param str 要填充的字符串
///@param len 要填充到的长度
///@param c 用来填充的字符
void fill_str(char *str, int len, char c)
{
    int i;
    
    for (i = strlen(str); i < len - 1; i++)
    {
        str[i] = c;
    }
    str[i] = '\0';
}


///@brief 把16进制的字符串转换为u8型数组
///比如字符串“3132”，转换为数组{31,32}
///@param str 字符串
///@param arr 数组
int hexstr2u8array(char *str, __u8 *arr)
{
    int i, len;
    
    len = strlen(str);
    for (i = 0; i < len; i += 2)
    {
        sscanf(&str[i], "%2hhx", &arr[i / 2]);
    }
    
    return 1;
}
    

///@brief 把16进制的字符串转换为u8型数组
///比如数组{31,32}，转换为字符串“3132”
///@param arr 数组
///@param arr_len 数组长度
///@param str 字符串
int u8array2hexstr(__u8 *arr, int arr_len, char *str)
{
    int i;
    
    for (i = 0; i < arr_len; i++)
    {
        sprintf(&str[i * 2], "%02x", arr[i]);
    }
    
    str[arr_len *2] = '\0';
    
    return 1;
}
    
#endif //_STRING_UTILS_C__
