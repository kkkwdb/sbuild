#ifndef __CFG_FILE_C__
#define __CFG_FILE_C__

////////// 头文件 ///////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#ifdef __NEED_XML__
#include <libxml2/libxml/xmlmemory.h>
#include <libxml2/libxml/parser.h>
#endif

#include "dbg.h"
#include "cfg_file.h"
#include "string_utils.h" 

#define MAX_FILENAME_LEN 128
#define MAX_OPENED_FILE_NUM 5
#define CHARLENGTH 1

#ifdef __NEED_XML__
struct file_info
{
    char filename[MAX_FILENAME_LEN];
    xmlDocPtr doc;
};

struct file_info glob_file_info[MAX_OPENED_FILE_NUM];
#endif
////// 函数 /////////////


///@brief 从普通配置文件中读取一个关键字。
///@param file 配置文件的路径
///@param keyword  读取的关键字
///@param [out] str 保存返回关键字字符串的缓冲区
///@param len  字符串缓冲区长度
///@return 成功：1；失败：-1。
int plain_cfg_file_read(char *file, char *keyword, char *str, int len)
{
    FILE *fp;
    char buf[MAX_CFG_LINE_LEN];
    char *ptr;
    int ret;
    
    ret = -1;
    
    ///if open file 
    fp = fopen(file, "r");
    if (fp)
    {
        str[0] = '\0';
        while (fgets(buf, MAX_CFG_LINE_LEN, fp) != NULL)
        {
            ///--delete spaces and comments
            del_space(buf);
        
            ///--if not match keyword, continue
            if (strncmp(buf, keyword, strlen(keyword)) != 0)
            {
                continue;
            }
            
            ///--the same prefix keyword
            if (buf[strlen(keyword)] != '=')
            {
                continue;
            }

            ///--point to keyword value string
            ptr = strchr(buf, '=');
            if (!ptr)
            {
                printf("\n### ERROR: error parse line: %s ###\n\n", buf);
                break;
            }
            ptr++;
            
            ///--copy str out
            ret = safe_strcpy(str, ptr, len);
            break;          
        }
        fclose(fp);
    }
    
    return ret;
}

#if defined(__NEED_XML__)
///@brief 从打开文件记录表中选取一个可用位置
///@return 返回位置下标，有空位则返回正整数；否则返回-1；
int get_one_position()
{
    int i;

    for(i = 0; i < MAX_OPENED_FILE_NUM; i++)
    {
        if (!(strlen(glob_file_info[i].filename)))
        {
            return i;
        }
    }
    
    return -1;
}

///@brief 打开xml配置文件
///@param file的 配置文件的路径
///@return 成功：要打开的文件指针；失败：NULL。
///@note 注意：由于xml_file_write参数为file，所以为不修改该函数调用部分，将doc信息存到全局变量glob_file_info中，且该全局变量最多支持5个xml的同时写操作
xmlDocPtr xml_cfg_file_open(char *file)
{
    xmlDocPtr doc;
    int pos;

    pos = 0;
    xmlKeepBlanksDefault(0);
    doc = xmlReadFile(file, "UTF-8", XML_PARSE_RECOVER);
    if (doc == NULL)
    {   
        fprintf(stderr,"Document not parsed successfully. \n");
    }
    else
    {
        pos = get_one_position();
        strncpy(glob_file_info[pos].filename, file, strlen(file));
        glob_file_info[pos].doc = doc;
    }

    return doc;
}

///@brief 检查文件打开状态
///@param file 配置文件的路径
///@return 如果已经打开，则返回正整数，作为全局保存的文件信息下标；否则返回-1；
int check_file_open(char *file)
{
    int i;

    for (i = 0; i < MAX_OPENED_FILE_NUM; i++)
    {
        if (!(strncmp(file, glob_file_info[i].filename, strlen(file))))
        {
            return i;
        }
    }

    return -1;
}

///@brief 检查文件是否为空
///@param file 配置文件的路径
///@return 如果正常，则返回0；否则返回-1；
int check_file_empty(char *file)
{
    char *p;
    xmlDocPtr doc;
    xmlNodePtr cur;

    
    p = file + strlen(file);
    p -= 3;
    
    if (strncmp(p, "xml", 3) == 0)
    {
        ///解析文件
        doc = xmlParseFile(file);    
        if (doc == NULL) 
        {
            fprintf(stderr,"Document not parsed successfully. \n");
            return -1;
        }

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) 
        {
            fprintf(stderr, "ERR: Can not parse empty document: %s\n", file);
            xmlFreeDoc(doc);
            return -1;
        }
        xmlFreeDoc(doc);
    }

    return 0;
}

///@brief 从xml配置文件中读取一个关键字。
///@param file的 配置文件的路径
///@param keyword  读取的关键字，关键字之间的层次关系用‘KERWORD_SPLIT_MARK’表示
///@param [out] value_str 保存返回关键字字符串的缓冲区
///@param len  字符串缓冲区长度
///@return 成功：1，失败：-1。
int xml_cfg_file_read(char *file, char *keyword, char *value_str, int len)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
    xmlChar *key;
    char *p, *q;
    char split_keyword[MAX_KEYWORD_LEN];
    int more_keyword;
    int i;
    
    ///初始化返回值
    value_str[0] = '\0';
    
    ///解析文件
    doc = xmlParseFile(file);    
    if (doc == NULL) 
    {
        fprintf(stderr,"Document not parsed successfully. \n");
        return -1;
    }
    
    cur = xmlDocGetRootElement(doc);
    if (cur == NULL) 
    {
        fprintf(stderr, "ERR: Can not parse empty document: %s\n", file);
        xmlFreeDoc(doc);
        return -1;
    }
    
    ///从文件的第一层节点和关键字的第一个字段循环
    cur = cur->xmlChildrenNode;    
    p = keyword;
    do
    {
        ///--根据关键字的分隔符，截取出当前字段
        q = strchr(p, KERWORD_SPLIT_MARK);
        if (q == NULL)
        {
            more_keyword = 0;
            safe_strcpy(split_keyword, p, MAX_KEYWORD_LEN);
        }
        else
        {
            if (q - p > MAX_KEYWORD_LEN)
            {
                xmlFreeDoc(doc);
                return -1;
            }
            more_keyword = 1;
            for (i = 0; i < (q - p); i++)
            {
                split_keyword[i] = p[i];
            }
            split_keyword[i] = '\0';
        }
        
        nf_log_msg(DEBUG_LEVEL_VERB, "parsing keyword : %s\n", split_keyword);
        
        ///--循环查看文件中当前层次的节点，直到本层节点全部查看完
        while (cur != NULL) 
        {
            ///----如果找到了和关键字字段相同的节点，
            if (!xmlStrcmp(cur->name, (const xmlChar *)split_keyword)) 
            {
                ///------根据关键字是否扫描完，获取最终值，或进入下层节点扫描
                if (more_keyword == 1)
                {
                    p = q + 1;
                    cur = cur->xmlChildrenNode; 
                }
                else
                {    
                    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                    if (key)
                    {
                        safe_strcpy(value_str, (char *)key, len);
                        xmlFree(key);   
                    }
                }
                
                ///------跳出本层节点查看循环
                break;
            }
            
            ///----否则查看本层下一个节点
            cur = cur->next;
        }
    }
    while(cur != NULL && more_keyword == 1);
    
    xmlFreeDoc(doc);
    if(cur == NULL)
    {
        return -1;
    }
    else
    {
        return 1;
    }
}

///@brief 从xml配置文件中读取一个关键字。
///@param file的 配置文件的路径
///@param keyword  读取的关键字，关键字之间的层次关系用‘KERWORD_SPLIT_MARK’表示
///@param [out] value_str 保存返回关键字字符串的缓冲区
///@param len  字符串缓冲区长度
///@return 成功：1，失败：-1。
int xml_cfg_file_read_no_open(char *file, char *keyword, char *value_str, int len)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
    xmlChar *key;
    char *p, *q;
    char split_keyword[MAX_KEYWORD_LEN];
    int more_keyword;
    int i, ret;
    
    ///初始化返回值
    value_str[0] = '\0';
    
    ///检查配置文件是否已经打开，如果已经打开，返回doc；否则打开文件，并返回doc
    ret = check_file_open(file);
    if (ret >= 0)
    {
        doc = glob_file_info[ret].doc;
    }
    else
    {
        doc = xml_cfg_file_open(file);
    }

    cur = xmlDocGetRootElement(doc);
    if (cur == NULL) 
    {
        fprintf(stderr, "ERR: Can not parse empty document: %s\n", file);
        xmlFreeDoc(doc);
        return -1;
    }
    
    ///从文件的第一层节点和关键字的第一个字段循环
    cur = cur->xmlChildrenNode;    
    p = keyword;
    do
    {
        ///--根据关键字的分隔符，截取出当前字段
        q = strchr(p, KERWORD_SPLIT_MARK);
        if (q == NULL)
        {
            more_keyword = 0;
            safe_strcpy(split_keyword, p, MAX_KEYWORD_LEN);
        }
        else
        {
            if (q - p > MAX_KEYWORD_LEN)
            {
                xmlFreeDoc(doc);
                return -1;
            }
            more_keyword = 1;
            for (i = 0; i < (q - p); i++)
            {
                split_keyword[i] = p[i];
            }
            split_keyword[i] = '\0';
        }
        
        nf_log_msg(DEBUG_LEVEL_VERB, "parsing keyword : %s\n", split_keyword);
        
        ///--循环查看文件中当前层次的节点，直到本层节点全部查看完
        while (cur != NULL) 
        {
            ///----如果找到了和关键字字段相同的节点，
            if (!xmlStrcmp(cur->name, (const xmlChar *)split_keyword)) 
            {
                ///------根据关键字是否扫描完，获取最终值，或进入下层节点扫描
                if (more_keyword == 1)
                {
                    p = q + 1;
                    cur = cur->xmlChildrenNode; 
                }
                else
                {    
                    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                    if (key)
                    {
                        safe_strcpy(value_str, (char *)key, len);
                        xmlFree(key);   
                    }
                }
                
                ///------跳出本层节点查看循环
                break;
            }
            
            ///----否则查看本层下一个节点
            cur = cur->next;
        }
    }
    while(cur != NULL && more_keyword == 1);
    
	/*
    xmlFreeDoc(doc);
	*/
    if(cur == NULL)
    {
        return -1;
    }
    else
    {
        return 1;
    }
}

///@brief 修改xml配置文件中的一个关键字。
///@param file的 配置文件的路径
///@param keyword  要修改的关键字，关键字之间的层次关系用‘KERWORD_SPLIT_MARK’表示
///@param value_str 要修改的关键字新的值
///@return 成功：1，失败：-1。
int xml_cfg_file_write(char *file, char *keyword, char *value_str)
{
    xmlNodePtr cur, parent, pnode;
    char *p, *q;
    char split_keyword[MAX_KEYWORD_LEN];
    int more_keyword;
    int i, ret;
    xmlDocPtr doc;
    
    ///检查配置文件是否已经打开，如果已经打开，返回doc；否则打开文件，并返回doc
    ret = check_file_open(file);
    if (ret >= 0)
    {
        doc = glob_file_info[ret].doc;
    }
    else
    {
        doc = xml_cfg_file_open(file);
    }

    ///解析文件
    cur = xmlDocGetRootElement(doc);
    if (cur == NULL) 
    {
        xmlFreeDoc(doc);
        return -1;
    }
    
	parent = cur;
    ///从文件的第一层节点和关键字的第一个字段循环
    cur = cur->xmlChildrenNode;    
    p = keyword;
    do
    {
        ///--根据关键字的分隔符，截取出当前字段
        q = strchr(p, KERWORD_SPLIT_MARK);
        if (q == NULL)
        {
            more_keyword = 0;
            safe_strcpy(split_keyword, p, MAX_KEYWORD_LEN);
        }
        else
        {
            if (q - p > MAX_KEYWORD_LEN)
            {
                xmlFreeDoc(doc);
                return -1;
            }
            more_keyword = 1;
            for (i = 0; i < (q - p); i++)
            {
                split_keyword[i] = p[i];
            }
            split_keyword[i] = '\0';
        }
        
        nf_log_msg(DEBUG_LEVEL_VERB, "parsing keyword : %s\n", split_keyword);
        
        ///--循环查看文件中当前层次的节点，直到本层节点全部查看完
        while (cur != NULL) 
        {
            ///----如果找到了和关键字字段相同的节点，
            if (!xmlStrcmp(cur->name, (const xmlChar *)split_keyword)) 
            {
                ///------根据关键字是否扫描完，获取最终值，或进入下层节点扫描
                if (more_keyword == 1)
                {
                    p = q + 1;
					parent = cur;
                    cur = cur->xmlChildrenNode; 
                }
                else
                {    
                    xmlNodeSetContent(cur->xmlChildrenNode, BAD_CAST value_str);
					nf_log_msg(DEBUG_LEVEL_VERB, "Change %s value to %s\n", keyword, value_str);
                }                
                ///------跳出本层节点查看循环
                break;
            }
            
            ///----否则查看本层下一个节点
            cur = cur->next;
        }

		///----如果当前层没找到，则在当前层创建节点或叶子
		if(!cur)
		{
			if(more_keyword == 1)
			{
				pnode =  xmlNewNode(NULL, BAD_CAST split_keyword);
				xmlAddChild (parent, pnode);
				p = q + 1;
				parent = pnode;
			}
			else
			{
				xmlNewTextChild(parent, NULL, BAD_CAST split_keyword, BAD_CAST value_str);
			}
		}
    }
    while(more_keyword == 1);
  /*
	if(cur == NULL)
	{
		///没有找到
		if(more_keyword == 0)
		{
			///最后一级节点
			 xmlNewTextChild (parent, NULL, BAD_CAST split_keyword, BAD_CAST value_str);
		}
		else
		{
			///非最后一级节点，暂不处理
			nf_log_msg(DEBUG_LEVEL_WARN, "Add %s failed, mode not supported\n", keyword);
		}
	}
	*/
//	xmlSaveFormatFileEnc(file, doc, "UTF-8", 1);
//    xmlFreeDoc(doc);
//	xmlMemoryDump();    
    return 1;
}

#endif

///@brief 从配置文件中读取一个关键字。
///@param file的 配置文件的路径
///@param keyword  读取的关键字，关键字之间的层次关系用‘KERWORD_SPLIT_MARK’表示。
///@note 在文本文件中，不支持文件内容的分段，不支持关键字层次。
///@param [out] value_str 保存返回关键字字符串的缓冲区
///@param len  字符串缓冲区长度
///@return 成功：1，失败：-1。
int cfg_file_read(char *file, char *keyword, char *value_str, int len)
{
    char *p;
    
    p = file + strlen(file);
    p -= 3;

    if (strncmp(p, "xml", 3) == 0)
    {
        nf_log_msg(DEBUG_LEVEL_SPEW, "reading file : %s ...\n", file);
#ifdef __NEED_XML__
        return xml_cfg_file_read(file, keyword, value_str, len);
#else
        return -1;
#endif
    }
    
    nf_log_msg(DEBUG_LEVEL_SPEW, "reading file : %s ...\n", file);
    return plain_cfg_file_read(file, keyword, value_str, len);
}

///@brief 从配置文件中读取一个关键字。
///@param file的 配置文件的路径
///@param keyword  读取的关键字，关键字之间的层次关系用‘KERWORD_SPLIT_MARK’表示。
///@note 在文本文件中，不支持文件内容的分段，不支持关键字层次。
///@param [out] value_str 保存返回关键字字符串的缓冲区
///@param len  字符串缓冲区长度
///@return 成功：1，失败：-1。
int cfg_file_read_no_open(char *file, char *keyword, char *value_str, int len)
{
    char *p;
    
    p = file + strlen(file);
    p -= 3;

    if (strncmp(p, "xml", 3) == 0)
    {
        nf_log_msg(DEBUG_LEVEL_SPEW, "reading file : %s ...\n", file);
#ifdef __NEED_XML__
        return xml_cfg_file_read_no_open(file, keyword, value_str, len);
#else
        return -1;
#endif
    }
    
    nf_log_msg(DEBUG_LEVEL_SPEW, "reading file : %s ...\n", file);
    return plain_cfg_file_read(file, keyword, value_str, len);
}

///@brief 向配置文件中写入一个关键字。
///@param file的 配置文件的路径
///@param keyword  要修改的关键字，关键字之间的层次关系用‘KERWORD_SPLIT_MARK’表示。
///@note 在文本文件中，不支持文件内容的分段，不支持关键字层次。
///@param value_str 关键字的新值
///@return 成功：1，失败：-1。
int cfg_file_write(char *file, char *keyword, char *value_str)
{
    char *p;
    
    p = file + strlen(file);
    p -= 3;
    
    if (strncmp(p, "xml", 3) == 0)
    {
        nf_log_msg(DEBUG_LEVEL_VERB, "write file : %s ...\n", file);
#ifdef __NEED_XML__
        return xml_cfg_file_write(file, keyword, value_str);
#else
        return -1;
#endif
    }
    
    nf_log_msg(DEBUG_LEVEL_WARN, "Plain file %s not support write\n", file);
    return 0;
}

///@brief 创建空的xml配置文件
///@param doc
///@return 成功：；失败：NULL。
#ifdef __NEED_XML__
void cfg_file_create(char *file)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr root_node = NULL;

    doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "root");
    xmlDocSetRootElement(doc, root_node);

    xmlSaveFormatFileEnc(file, doc, "UTF-8", 1);

    xmlFreeDoc(doc);
    xmlCleanupParser();
}
#endif

///@brief 关闭xml配置文件
///@param doc
///@return 成功：；失败：NULL。
#ifdef __NEED_XML__
void cfg_file_close(char *file)
{
    int ret;
    xmlDocPtr doc;

    ret = check_file_open(file);
    if (ret == -1)
    {
        nf_log_msg(DEBUG_LEVEL_WARN, "File %s is not open\n", file);
    }
    else
    {
        doc = glob_file_info[ret].doc;

        xmlSaveFormatFileEnc(file, doc, "UTF-8", 1);
        xmlFreeDoc(doc);
        xmlMemoryDump();

        memset(glob_file_info[ret].filename, 0, MAX_FILENAME_LEN);
        glob_file_info[ret].doc = NULL;
    }
}
#endif
#endif //__PLAIN_CFG_FILE_C__
