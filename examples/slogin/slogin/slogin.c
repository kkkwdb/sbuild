#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sched.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>

#define DEFAULT_TRANS_PORT      31075               ///<发送的端口号
#define MAX_TRANS_DATA_LEN      8192                ///<一次发送的数据最大值
#define DEFAULT_TRANS_DATA_LEN  256                 ///<默认的一次发送的数据大小
#define FILE_DATA_BUF_LEN       4096*1024           ///<每次读文件的数据缓冲区大小
#define MAX_ERROR_LEN           1024
#define DATA_BUF_LEN            FILE_DATA_BUF_LEN   ///<每次接收的数据缓冲区大小
#define MAX_ENV_NUM             16
#define MAX_ENV_LEN             256
#define MAX_CMD_LEN             512

#define DEF_PATH_MAX            PATH_MAX

#define WORK_MODE_TEST          0x0000
#define WORK_MODE_TEST_NOACK    0x0001
#define WORK_MODE_TEST_ACK      0x0002
#define WORK_MODE_TRANS         0x0100
#define WORK_MODE_TRANS_SAVE    0x0101
#define WORK_MODE_TRANS_SEND    0x0102
#define WORK_MODE_CMD           0x0200

#define DATASRC_MEM             0
#define DATASRC_FILE            1
#define DATASRC_DIR             2
#define DATASRC_INPUT           3

#define SLOGIN_BUILDIN_CHAR     '_'-0100

struct envstring
{
    int more;
    char key[MAX_ENV_LEN];
    char value[MAX_ENV_LEN];
};

struct config_details
{
    int sock;
    int nodelay;
    int affinity;
    int noblock;
    int recv_flag;
    int performance;
    int fast_count;
    int pktlen;
    int trans_port;
    int datasrc;
    int quiet;
    int env_num;
    struct envstring *env_strings;
    char cmd[MAX_CMD_LEN];
};

struct msg_head
{
    int data_len;
    int more;
};

struct config_details g_conf;

pthread_t g_tid_rx, g_tid_tx;
int g_sock;
struct termios g_term;

static int app_set_affinity(int cpu)
{
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	if(sched_setaffinity(0, sizeof(mask), &mask) != 0) {
		fprintf(stderr, "Set affinity failed for cpu %d\n", cpu);
		return -1;
	}
	return 0;
}

struct file_update_head
{
    __u32 more;                 ///<为1表示还有文件，为0表示最后一个文件
    char dir[DEF_PATH_MAX];     ///<文件目录名，字符串格式，'\0'结束。
    char name[DEF_PATH_MAX];    ///<文件名，字符串格式，'\0'结束。
    __u32 len;                  ///<文件长度，byte为单位
};
#define N2H_FILE_UPDATE_HEAD(ptr) \
do \
{ \
    (ptr)->more = htonl((ptr)->more); \
    (ptr)->len = htonl((ptr)->len); \
}while(0)
#define H2N_FILE_UPDATE_HEAD(ptr) \
do \
{ \
    (ptr)->more = htonl((ptr)->more); \
    (ptr)->len = htonl((ptr)->len); \
}while(0)

static struct envstring g_env_strings[MAX_ENV_NUM];

char *buf;
static const char *optstring = "hnp:v:f:reu:a:l:m:c:q";
int sendtotal,recvtotal,pkts; 

void usage()
{
    printf("usage:\t./slogin [-h] [-n] [-l packetlength] [-f file] [-a cup_id] [-e] [-r] [-u n] [-p port] [-v key=val] [-m work_mode] [-c cmd] addr1 [addr2]\n"
            "\t-h: print usage\n"
            "\t-n: nonblock translate mode\n"
            "\t-l: set packet length per translation\n"
            "\t-f: set file for test translate\n"
            "\t-e: test performance mode(apply to -f)\n"
            "\t-a: set cpu affinity\n"
            "\t-r: set received mode\n"
            "\t-u: translate n 4MB data\n"
            "\t-p: set translate port\n"
            "\t-v: set login shell environment\n"
            "\t-m: set work mode(cmd,trans,test)\n"
            "\t-c: quick to execute a command\n"
            "\t-q: quiet(suppress output)\n");
}

///@brief 取得工作模式
///@param str 工作模式字符串
int get_work_mode(char *str)
{
    int work_mode;

    if(str == NULL)
        return -1;

    work_mode = -1;
    if(strcmp(str, "test") == 0)
    {
        work_mode = WORK_MODE_TEST;
    }
    else if(strcmp(str, "trans") == 0)
    {
        work_mode = WORK_MODE_TRANS;
    }
    else if(strcmp(str, "cmd") == 0)
    {
        work_mode = WORK_MODE_CMD;
    }

    return work_mode;
}

///@brief 在路径中去除开始的根目录路径名
///@param dir 路径名
void remove_root_dir(char *dir, char *root)
{
    char *begin;
    char tmp[PATH_MAX];

    if(dir==NULL || root==NULL)
        return;
    if((begin=strstr(dir, root)) == dir)
    {
        begin += strlen(root);
        if(*begin == '/')
            begin++;
        strcpy(tmp, begin);
        strcpy(dir, tmp);
    }
}

///@brief 读取dir目录项
///@param dir 目录结构指针
///@return NULL: 遍历完目录，其他：目录项指针
struct dirent *my_readdir(DIR *dir)
{
    struct dirent *entry;
    for(;;)
    {
        entry = readdir(dir);
        if(entry == NULL)
            return NULL;
        if( strcmp(entry->d_name, ".")!=0 && strcmp(entry->d_name, "..")!=0)
        {
            return entry;
        }
    }
}

int trans_file(int sock, char *pathname, struct file_update_head *head, int quiet)
{
    int read_len, len, m_len;
    FILE *file;

    m_len = 0;
    read_len = sizeof(struct file_update_head);
    while(m_len < read_len)
    {
        len = send(sock, (char *)head+m_len, read_len-m_len, 0);
        if(len<0 && errno==EAGAIN)
            continue;
        if(len < 0)
        {
            perror("head send");
            return -1;
        }
        m_len += len;
        pkts++;
    }
    sendtotal += read_len;

    if(pathname == NULL)
    {
        return 0;
    }

    if( (file=fopen(pathname, "r")) == NULL)
    {
        fprintf(stderr, "open file %s fail\n", head->name);
        return -1;
    }

    while( (read_len=fread(buf, 1, 4096, file)) > 0)
    {
        m_len = 0;
        while(m_len < read_len)
        {
            len = send(sock, buf+m_len, read_len-m_len, 0);
            if(len<0 && errno==EAGAIN)
                continue;
            if(len < 0)
            {
                perror("data send");
                fclose(file);
                return -1;
            }
            m_len += len;
            pkts++;
        }
        sendtotal += read_len;
    }

    if(quiet == 0)
    {
        printf("send file: %s\n", head->name);
    }

    fclose(file);

    return 0;
}

///@brief 得到目录的根目录（上层目录）
///@param root [out] 根目录
///@param dir [out] 目录,输出为绝对路径
///@return -1:失败，0:成功
int get_dir_root(char *root, char *dir)
{
    int i;
    char old_cwd[DEF_PATH_MAX];
    char new_cwd[DEF_PATH_MAX];

    getcwd(old_cwd, 256);
    if(chdir(dir) < 0)
    {
        return -1;
    }
    getcwd(new_cwd,256);
    chdir(old_cwd);
    i = strlen(new_cwd);
    if(new_cwd[i-1] != '/')
    {
        new_cwd[i] = '/';
        new_cwd[i+1] = 0;
    }
    strcpy(root, new_cwd);
    strcpy(dir, new_cwd);

    i = strlen(root)-2;
    while(i>=0 && root[i]!='/')
    {
        i--;
    }
    if(i>=0)
    {
        root[i+1] = 0;
    }
    else
    {
        root[0] = 0;
    }

    return 0;
}

///@brief 传输目录中所有文件，包括子目录
///@param sock socket描述符
///@param dirname 目录名
///@param root    根目录名
///@param last 1:标志最后一个目录,0:标志其他目录
///@return 0:成功，-1:失败
int trans_dir(int sock, char *dirname, char *root, int last, int quiet)
{
    FILE *file;
    DIR *dir;
    struct dirent *entry, *entry_n;
    char pathname[256];
    struct file_update_head head;
    int len;
    struct stat st;

    entry = NULL;
    dir = NULL;

    if( (dir=opendir(dirname)) == NULL)
    {
        perror("opendir");
        return -1;
    }

    entry=my_readdir(dir);
    if(entry == NULL)
    {
        strcpy(head.dir, dirname);
        remove_root_dir(head.dir, root);
        head.name[0] = 0;
        head.len = 0;
        if(last)
        {
            head.more = 0;
        }
        else
        {
            head.more = 1;
        }
        H2N_FILE_UPDATE_HEAD(&head);
        if(trans_file(sock, NULL, &head, quiet) < 0)
            return -1;
        return 0;
    }
    for(;;)
    {
        entry_n = my_readdir(dir);

        strcpy(pathname, dirname);
        strcat(pathname, entry->d_name);
        if( (file=fopen(pathname, "r")) == NULL)
        {
            fprintf(stderr, "open file %s fail\n", head.name);
            return -1;
        }

        fstat(fileno(file), &st);
        if( S_ISDIR(st.st_mode))
        {
            int end;

            fclose(file);
            if(entry_n==NULL && last==1)
            {
                end = 1;
            }
            else
            {
                end = 0;
            }
            len = strlen(pathname);
            if(pathname[len-1] != '/')
            {
                pathname[len] = '/';
                pathname[len+1] = 0;
            }
            if(trans_dir(sock, pathname, root, end, quiet) < 0)
            {
                return -1;
            }
            entry = entry_n;
            if( entry == NULL)
            {
                break;
            }
            continue;
        }

        strcpy(head.dir, dirname);
        remove_root_dir(head.dir, root);
        strcpy(head.name, entry->d_name);

        fseek(file, 0, SEEK_END);
        len = ftell(file);
        fseek(file, 0, SEEK_SET);
        len = len - ftell(file);
        head.len = len;
        fclose(file);

        if(entry_n==NULL && last==1)
        {
            head.more = 0;
        }
        else
        {
            head.more = 1;
        }
        H2N_FILE_UPDATE_HEAD(&head);

        if(trans_file(sock, pathname, &head, quiet) < 0)
        {
            return -1;
        }

        entry = entry_n;
        if(entry == NULL)
            break;
    }

    return 0;
}

int save_files(int fd, int trans_flag, char *savedir, int quiet)
{
    int len;
    __u64 m_len,total_len,recvcount;
    struct timeval tv1, tv2;
    __u64 val;
    struct file_update_head head;
    FILE *file;
    __u64 pkts;
    int more;
    char filename[256];
    char cmd[256];
    long writecount;

    recvcount = 0;
    writecount = 0;
    gettimeofday(&tv1, NULL);
    pkts = 0;
    memset(&head, 0, sizeof(struct file_update_head));
    more = 1;

    while(more)
    {
        ///----接收传输文件命令头
        total_len = sizeof(struct file_update_head);
        len = 0;
        m_len = 0;
        while(m_len < total_len)
        {
            len=recv(fd, buf+m_len, total_len-m_len, trans_flag);
            if(len<0 && errno==EAGAIN)
                continue;
            if(len <= 0)
            {
                if(len < 0)
                    perror("head recv");
                close(fd);
                return -1;
            }
            m_len += len;
            pkts++;
        }
        memcpy(&head, buf, sizeof(struct file_update_head));;
        N2H_FILE_UPDATE_HEAD(&head);
        recvcount += total_len;

        strcpy(filename, savedir);
        if(strlen(head.dir))
        {
            strcat(filename, head.dir);
            sprintf(cmd, "mkdir -p %s", filename);
            system(cmd);
        }

        if(head.len == 0)
        {
            continue;
        }

        if(quiet == 0)
        {
            printf("receive file: %s\n", head.name);
        }
        strcat(filename, head.name);
        if( (file = fopen(filename, "w")) == NULL)
        {
            perror("fopen");
            printf("filename: %s\n", filename);
            close(fd);
            return -1;
        }


        ///----接收文件
        total_len = head.len;
        len = 0;
        m_len = 0;
        while(m_len<total_len)
        {
            len=recv(fd, buf, total_len-m_len<DATA_BUF_LEN?total_len-m_len:DATA_BUF_LEN, trans_flag);
            if(len<0 && errno==EAGAIN)
                continue;
            if(len < 0)
            {
                perror("data recv");
                close(fd);
                return -1;
            }
            if(fwrite(buf, 1, len, file) < 0)
            {
                close(fd);
                fclose(file);
                return -1;
            }
            m_len += len;
            pkts++;
        }
        fclose(file);
        recvcount += total_len;
        writecount += total_len;
        more = head.more;
    }
    gettimeofday(&tv2, NULL);
    val = tv2.tv_sec*1000000 + tv2.tv_usec - tv1.tv_sec*1000000 - tv1.tv_usec;
    if(quiet == 0)
    {
        printf("received %ld packets, %ld bytes, %ld pps, %ld Bps,"
                "write %ld bytes, spend %ld microseconds.\n"
                , (long)pkts, (long)recvcount, (long)(pkts*1000000/val),
                (long)(recvcount*1000000/val), (long)writecount, (long)val);
        printf("\n");
    }
    return 0;
}

int do_trans_send(struct config_details conf, char *savedir, char *remnode)
{
    int len;
    struct file_update_head head;

    strcpy(head.dir, remnode);
    if(send(conf.sock, &head, sizeof(head), 0) != sizeof(head))
    {
        perror("send");
        return -1;
    }

    len = strlen(savedir);
    if(savedir[len-1] != '/')
    {
        savedir[len] = '/';
        savedir[len+1] = 0;
    }

    save_files(conf.sock, 0, savedir, conf.quiet);
    return 0;
}

void *slogin_rx_thread(void *unused)
{
    char data;
    int n;

    while(1)
    {
        if( (n=read(g_sock, &data, 1)) <= 0)
        {
            if(n < 0)
                perror("rx read");
            break;
        }
        if(write(STDOUT_FILENO, &data, n)<=0)
        {
            perror("rx write");
            break;
        }
    }
    pthread_cancel(g_tid_tx);
    pthread_exit(NULL);
}

void change_window_size(int signum)
{
    struct winsize ws;
    char buf[64];
    int len;

    if( ioctl(STDIN_FILENO, TIOCGWINSZ, (void *)&ws) < 0)
        return;

    sprintf(buf, "%ccws %d %d\n", SLOGIN_BUILDIN_CHAR, ws.ws_row, ws.ws_col);
    len = strlen(buf);
    if(write(g_sock, &buf, len) != len)
    {
        perror("change window size fail");
    }
}

void *slogin_tx_thread(void *unused)
{
    char data;
    int n;

    signal(SIGWINCH, change_window_size);
    kill(getpid(), SIGWINCH);

    while(1)
    {
        if( (n=read(STDIN_FILENO, &data, 1)) <= 0)
        {
            if(n < 0)
                perror("tx read");
            break;
        }
        if(write(g_sock, &data, n) <= 0)
        {
            perror("tx read");
            break;
        }
    }
    pthread_cancel(g_tid_tx);
    pthread_exit(NULL);
}

void restore_tty()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_term);
}

int set_terminal(struct config_details *conf)
{
    int len;
    int i;
    int sendlen;
    char hasenv;
    struct stat buf;
    struct termios tio;

    if(conf->datasrc == DATASRC_INPUT)
    {
        hasenv = 'E';
        len = 1;
        if( (sendlen=send(g_sock, &hasenv, 1, 0))!=len)
        {
            perror("send");
            return -1;
        }

        for(i=0;i<g_conf.env_num;i++)
        {
            g_conf.env_strings[i].more = 1;
        }
        if(i>0)
            g_conf.env_strings[i-1].more = 0;
        len = sizeof(struct envstring)*g_conf.env_num;
        if(len == 0)
        {
            len = sizeof(struct envstring);
            g_conf.env_strings[0].more = -1;
        }
        if( (sendlen=send(g_sock, g_conf.env_strings, len, 0))!=len)
        {
            perror("send");
            return -1;
        }

        if(fstat(STDIN_FILENO, &buf) < 0)
        {
            perror("fstat");
            return -1;
        }

        if(S_ISREG(buf.st_mode))
            return 0;

        if(tcgetattr(STDIN_FILENO, &g_term) < 0)
        {
            perror("tcgetattr");
            return -1;
        }
        atexit(restore_tty);

        tio = g_term;
        tio.c_iflag &= ~IXON;
        tio.c_lflag &= ~(ICANON|ISIG|ECHO|ECHOE|ECHOK|ECHOKE|ECHOCTL|ECHONL|ECHOPRT);
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio) < 0)
        {
            perror("tcsetattr");
            return -1;
        }

        return 0;

    }
    else if(conf->datasrc == DATASRC_MEM)
    {
        hasenv = 'C';
        len = 1;
        if( (sendlen=send(g_sock, &hasenv, 1, 0))!=len)
        {
            perror("send");
            return -1;
        }
        return 0;
    }
    return -1;
}

int get_cmd_res(struct config_details *conf, char *buf, int len)
{
    int r_len;
    int cmd_len;

    cmd_len = MAX_CMD_LEN;
    if( send(conf->sock, conf->cmd, cmd_len, 0) != cmd_len)
    {
        perror("get_cmd_res send");
        close(conf->sock);
        return -1;
    }
    for(;;)
    {
        r_len = recv(conf->sock, buf, len, 0);
        if(r_len<0 && errno==EAGAIN)
            continue;
        if(r_len < 0)
        {
            close(conf->sock);
            return -1;
        }
        if(r_len == 0)
            break;
        buf[r_len] = 0;
        printf(buf);
    }
    close(conf->sock);
    return 0;
}

int do_send_cmd(struct config_details conf, char *buf, int buf_len)
{
    g_sock = conf.sock;

    if(set_terminal(&conf) < 0)
    {
        return -1;
    }
    if(conf.datasrc == DATASRC_INPUT)
    {
        if(pthread_create(&g_tid_rx, NULL, slogin_rx_thread, NULL) < 0)
        {
            exit(-1);
        }
        if(pthread_create(&g_tid_tx, NULL, slogin_tx_thread, NULL) < 0)
        {
            exit(-1);
        }

        pthread_join(g_tid_rx, NULL);
        pthread_join(g_tid_tx, NULL);
    }
    else
    {
        get_cmd_res(&conf, buf, buf_len);
    }

    return 0;
}

int send_work_mode(int fd, int work_mode, int test_len)
{
    char work_mode_char, work_mode_ret;
    int work_test_len = 0;

    if(work_mode == WORK_MODE_CMD)
        work_mode_char = 'C';
    else if(work_mode == WORK_MODE_TEST_ACK)
        work_mode_char = 'A';
    else if(work_mode == WORK_MODE_TEST_NOACK)
        work_mode_char = 'N';
    else if(work_mode == WORK_MODE_TRANS_SAVE)
        work_mode_char = 'E';
    else if(work_mode == WORK_MODE_TRANS_SEND)
        work_mode_char = 'D';
    else
        return -1;
    if(send(fd, (void *)&work_mode_char, sizeof(work_mode_char), 0) < 0)
    {
        perror("send work mode\n");
        return -1;
    }
    if(recv(fd, (void *)&work_mode_ret, sizeof(work_mode_ret), 0) < 0)
    {
        perror("recv work mode ack\n");
        return -1;
    }
    if ((work_mode == WORK_MODE_TEST_ACK) || (work_mode == WORK_MODE_TEST_NOACK))
    {
        work_test_len = test_len;
        work_test_len = htonl(work_test_len);
        if(send(fd, (void *)&work_test_len, sizeof(int), 0) < 0)
        {
            perror("send work mode\n");
            return -1;
        }
        if(recv(fd, (void *)&work_mode_ret, sizeof(work_mode_ret), 0) < 0)
        {
            perror("recv work mode ack\n");
            return -1;
        }
    }
    return 0;
}

int do_test(struct config_details conf, char *pathname)
{
    long len,m_len,readlen;
    int r_len,r_mlen;
    FILE *file;
    struct timeval tv1, tv2;
    __u64 val;

    gettimeofday(&tv1, NULL);

    if(conf.datasrc == DATASRC_MEM)
    {
        recvtotal = sendtotal =  0;
        readlen = FILE_DATA_BUF_LEN*conf.fast_count;
        pkts = 0;
        m_len = 0;
        while(m_len < readlen)
        {
            len=send(conf.sock, buf, conf.pktlen, 0);
            if(len<0 && errno==EAGAIN)
                continue;
            if(len < 0)
            {
                close(conf.sock);
                return -1;
            }
            m_len += len;
            pkts++;
            if(conf.recv_flag == 1)
            {
                r_mlen = 0;
                while(r_mlen < len)
                {
                    r_len = recv(conf.sock, buf, len-r_mlen, 0);
                    if(r_len<0 && errno==EAGAIN)
                        continue;
                    if(r_len < 0)
                    {
                        close(conf.sock);
                        return -1;
                    }
                    r_mlen += r_len;
                }
            }
        }
        recvtotal += readlen;
        sendtotal += readlen;
    }
    else if(conf.datasrc == DATASRC_FILE)
    {
        char *basename;

        if( (file = fopen(pathname,"r")) == NULL)
        {
            perror("fopen");
            close(conf.sock);
            return -1;
        }
        basename = strrchr(pathname, '/');
        if(basename==NULL)
            basename = pathname;
        else
            basename++;
        for(;;)
        {

            if( (readlen=fread(buf, 1, FILE_DATA_BUF_LEN, file))<=0)
            {
                fclose(file);
                break;
            }
            m_len = 0;
            while(m_len < readlen)
            {
                len=send(conf.sock, buf+m_len, readlen-m_len>conf.pktlen?conf.pktlen:readlen-m_len, 0);
                if(len<0 && errno==EAGAIN)
                    continue;
                if(len < 0)
                {
                    fclose(file);
                    close(conf.sock);
                    return -1;
                }
                m_len += len;
                pkts++;
                if(conf.recv_flag == 1)
                {
                    r_mlen = 0;
                    while(r_mlen < len)
                    {
                        r_len = recv(conf.sock, buf+r_mlen, len-r_mlen, 0);
                        if(r_len<0 && errno==EAGAIN)
                            continue;
                        if(r_len < 0)
                        {
                            fclose(file);
                            close(conf.sock);
                            return -1;
                        }
                        r_mlen += r_len;
                    }
                }
            }
            sendtotal += readlen;
            recvtotal += readlen;
        }
    }
    else
    {
        return -1;
    }
    gettimeofday(&tv2, NULL);
    val = tv2.tv_sec*1000000 + tv2.tv_usec - tv1.tv_sec*1000000 - tv1.tv_usec;

    if(conf.quiet == 0)
    {
        printf("send %d data,\t%d packets, \t%ld s, \t%ld pps, \tpacket lengh %d.\n", 
                sendtotal, pkts, (long)(val/1000000), (long)((long)pkts*1000000/val), conf.pktlen);
        if(conf.recv_flag == 1)
            printf("recv %d data.\n", recvtotal);
    }

    return 0;
}

int do_trans_save(struct config_details conf, char *pathname, char *root, char *savedir)
{
    int ret;
    struct file_update_head head;
    char * name;

    strcpy(head.dir, savedir);
    if(send(conf.sock, &head, sizeof(head), 0) != sizeof(head))
    {
        perror("send");
        return -1;
    }

    if(conf.datasrc == DATASRC_DIR)
    {
        ret = trans_dir(conf.sock, pathname, root, 1, conf.quiet);
    }
    else if(conf.datasrc == DATASRC_FILE)
    {
        ret = 0;
        while(1)
        {
            int len;
            FILE *file;

            if( (file=fopen(pathname, "r")) == NULL)
            {
                ret = -1;
                break;
            }
            head.dir[0] = 0;
            name = strrchr(pathname, '/');
            if(name != NULL)
            {
                name++;
                strcpy(head.name, name);
            }
            else
            {
                strcpy(head.name, pathname);
            }
            head.more = 0;
            fseek(file, 0, SEEK_END);
            len = ftell(file);
            fseek(file, 0, SEEK_SET);
            len = len - ftell(file);
            head.len = len;
            fclose(file);
            H2N_FILE_UPDATE_HEAD(&head);

            if(trans_file(conf.sock, pathname, &head, conf.quiet) < 0)
            {
                fprintf(stderr, "translate file '%s' fail\n", pathname);
                ret = -1;
            }
            break;
        }
    }
    else
    {
        ret = -1;
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int opt;
    int work_mode;
    struct config_details conf;
    struct sockaddr_in srvaddr;
    char dst_ip[INET_ADDRSTRLEN];
    char pathname[FILENAME_MAX];
    char root[FILENAME_MAX];
    char remnode[FILENAME_MAX];

    work_mode = WORK_MODE_CMD;
    conf.sock = 0;
    conf.nodelay = 0;
    conf.affinity = -1;
    conf.noblock = 0;
    conf.recv_flag = 0;
    conf.performance = 0;
    conf.fast_count = 1;
    conf.pktlen = DEFAULT_TRANS_DATA_LEN;
    conf.trans_port = DEFAULT_TRANS_PORT;
    conf.datasrc = DATASRC_MEM;
    conf.quiet = DATASRC_MEM;
    conf.env_num = 0;
    conf.env_strings = g_env_strings;
    memset(conf.env_strings, 0, sizeof(*conf.env_strings)*MAX_ENV_NUM);
    memset(conf.cmd, 0, MAX_CMD_LEN);
    while( (opt=getopt(argc, argv, optstring)) > 0)
    {
        switch(opt)
        {
            case 'l':
                conf.pktlen = atoi(optarg);
                if(conf.pktlen<=0 || conf.pktlen>MAX_TRANS_DATA_LEN)
                {
                    usage();
                    return 0;
                }
                break;
            case 'u':
                conf.fast_count = atoi(optarg);
                break;
            case 'e':
                conf.performance = 1;
                break;
            case 'r':
                conf.recv_flag = 1;
                break;
            case 'h':
                usage();
                return 0;
            case 'n':
                conf.noblock = 1;
                break;
            case 'a':
                conf.affinity = atoi(optarg);
                break;
            case 'p':
                conf.trans_port = atoi(optarg);
                break;
            case 'v':
                {
                    char key_value[FILENAME_MAX];
                    char *key, *value;

                    strcpy(key_value,optarg);
                    key=key_value;
                    if( (value=strchr(key_value, '=')) == NULL)
                    {
                        usage();
                        return -1;
                    }
                    *value++ = 0;
                    strncpy(conf.env_strings[conf.env_num].key, key, MAX_ENV_LEN-1);
                    strncpy(conf.env_strings[conf.env_num].value, value, MAX_ENV_LEN-1);
                    conf.env_num ++;
                    if(conf.env_num > MAX_ENV_NUM)
                    {
                        usage();
                        return -1;
                    }
                }
                break;
            case 'f':
                strcpy(pathname, optarg);
                conf.datasrc = DATASRC_FILE;
                break;
            case 'm':
                work_mode = get_work_mode(optarg);
                if(work_mode < 0)
                {
                    usage();
                    return -1;
                }
                break;
            case 'q':
                conf.quiet = 1;
                break;
            case 'c':
                strcpy(conf.cmd, optarg);
                break;
            default:
                usage();
                return -1;
        }
    }

    if(conf.affinity != -1)
    {
        if(app_set_affinity(conf.affinity) < 0)
        {
            usage();
            return -1;
        }
    }

    if(work_mode == WORK_MODE_TEST)
    {
        conf.nodelay = 1;
        if(conf.recv_flag)
            work_mode = WORK_MODE_TEST_ACK;
        else
            work_mode = WORK_MODE_TEST_NOACK;
        if(argv[optind] == NULL)
        {
            usage();
            return -1;
        }
        strcpy(dst_ip, argv[optind]);
    }
    else if(work_mode == WORK_MODE_CMD)
    {
        conf.nodelay = 1;
        if(argv[optind] == NULL)
        {
            usage();
            return -1;
        }
        strcpy(dst_ip, argv[optind]);
        conf.datasrc = DATASRC_INPUT;
        if(strlen(conf.cmd))
        {
            conf.datasrc = DATASRC_MEM;
        }
    }
    else if(work_mode==WORK_MODE_TRANS)
    {
        char *src_node;
        char *dst_node;
        struct stat st;

        if(argv[optind]==NULL || argv[optind+1]==NULL)
        {
            usage();
            return -1;
        }

        if( (dst_node=strchr(argv[optind], ':')) == NULL)
        {
            src_node = argv[optind];
            if(src_node == NULL)
            {
                usage();
                return -1;
            }
            if( (dst_node=strchr(argv[optind+1], ':')) == NULL)
            {
                usage();
                return -1;
            }
            else
            {
                work_mode=WORK_MODE_TRANS_SAVE;
                *dst_node++ = 0;
                strcpy(dst_ip, argv[optind+1]);
            }
        }
        else
        {
            work_mode=WORK_MODE_TRANS_SEND;
            src_node = argv[optind+1];
            if(src_node == NULL)
            {
                usage();
                return -1;
            }
            *dst_node++ = 0;
            strcpy(dst_ip, argv[optind]);
        }

        strcpy(remnode, dst_node);
        strcpy(pathname, src_node);
        if( (stat(pathname, &st)) < 0)
        {
            perror("open");
            return -1;
        }
        if(S_ISDIR(st.st_mode))
        {
            conf.datasrc =DATASRC_DIR;
            if(get_dir_root(root, pathname) < 0)
            {
                printf("No such file or directory.\n");
                return -1;
            }
        }
        else if(S_ISREG(st.st_mode))
        {
            conf.datasrc =DATASRC_FILE;
        }
        else
        {
            fprintf(stdout, "%s is not a file or directory\n", pathname);
            return -1;
        }
    }
    else
    {
        usage();
        return -1;
    }

    if( (buf=malloc(FILE_DATA_BUF_LEN)) == NULL)
    {
        perror("malloc");
        return -1;
    }

    srvaddr.sin_family = PF_INET;
    srvaddr.sin_addr.s_addr = inet_addr(dst_ip);
    srvaddr.sin_port = htons(conf.trans_port);
    if( (conf.sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }
    if( connect(conf.sock, (void *)&srvaddr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("connect");
        return -1;
    }

    if(send_work_mode(conf.sock, work_mode, conf.pktlen) < 0)
    {
        return -1;
    }
    if(setsockopt(conf.sock, IPPROTO_TCP, TCP_NODELAY, &conf.nodelay, sizeof(int)) < 0)
    {
        perror("setsockopt");
        return -1;
    }
    if(ioctl(conf.sock, FIONBIO, &conf.noblock) < 0)
    {
        perror("ioctl");
        return -1;
    }

    g_conf = conf;
    switch(work_mode)
    {
        case WORK_MODE_TEST_ACK:
            conf.recv_flag = 1;
            do_test(conf, pathname);
            break;
        case WORK_MODE_TEST_NOACK:
            conf.recv_flag = 0;
            do_test(conf, pathname);
            break;
        case WORK_MODE_TRANS_SAVE:
            conf.recv_flag = 0;
            do_trans_save(conf, pathname, root, remnode);
            if(recv(conf.sock, buf, MAX_ERROR_LEN, 0) > 0)
            {
                printf("error: %s\n", buf);
                free(buf);
                close(conf.sock);
                return -1;
            }
            break;
        case WORK_MODE_TRANS_SEND:
            conf.recv_flag = 0;
            do_trans_send(conf, pathname, remnode);
            if(recv(conf.sock, buf, MAX_ERROR_LEN, 0) > 0)
            {
                printf("error: %s\n", buf);
                free(buf);
                close(conf.sock);
                return -1;
            }
            break;
        case WORK_MODE_CMD:
            do_send_cmd(conf, buf, FILE_DATA_BUF_LEN);
            break;
        default:
            free(buf);
            close(conf.sock);
            return -1;
    }

    free(buf);
    close(conf.sock);
    return 0;
}
