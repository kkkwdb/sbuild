#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <getopt.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <fcntl.h>
#include <syslog.h>
#include <pthread.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <error.h>

#define DEFAULT_DATA_TRANS_PORT         31075
#define DATA_BUF_LEN                    4096*4
#define MAX_ERROR_LEN                   1024
#define MAX_ENV_NUM                     16
#define MAX_ENV_LEN                     256
#define DEF_PATH_MAX                    PATH_MAX
#define MAX_CMD_LEN                     512

#define WORK_MODE_TEST          0x0000
#define WORK_MODE_TEST_NOACK    0x0001
#define WORK_MODE_TEST_ACK      0x0002
#define WORK_MODE_TRANS         0x0100
#define WORK_MODE_TRANS_SAVE    0x0101
#define WORK_MODE_TRANS_SEND    0x0102
#define WORK_MODE_CMD           0x0200

#define DATASRC_FILE            1
#define DATASRC_DIR             2

#define MAX_INNER_CMD_NUM       10
#define MAX_INNER_CMD_LEN       1024
#define SL_DEF_PATH             "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:/helinux/bin:/conf"

#define SLOGIN_BUILDIN_CHAR     '_'-0100

#define PIDFILE "/var/run/slogind.pid"

struct msg_head
{
    int data_len;
    int more;
};

struct envstring
{
    int more;
    char key[MAX_ENV_LEN];
    char value[MAX_ENV_LEN];
};

struct file_update_head
{
    __u32 more;                 ///<为1表示还有文件，为0表示最后一个文件
    char dir[DEF_PATH_MAX];     ///<文件的相对目录
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

struct slogin_ctl
{
    int ptm;
    char pts_name[PATH_MAX];
    int sock;
    int change_pts_stage;
    pthread_t tid_rx;
    pthread_t tid_tx;
};

struct sl_inner_cmd
{
    char name[MAX_INNER_CMD_LEN];
    int (* func)(char *cmd);
};

int daemon_enale;
int g_inner_cmd_enable;
static char buf[DATA_BUF_LEN];
static const char *optsring = "hnia:p:d";
static struct slogin_ctl g_ctl;
static struct sl_inner_cmd g_inner_cmd[MAX_INNER_CMD_NUM];
static struct envstring g_env_strings[MAX_ENV_NUM];
static int g_env_num;

int ptym_open(char *pts_name, int pts_namesz)
{
    int ptm;

    if( (ptm=posix_openpt(O_RDWR)) < 0)
    {
        return -1;
    }
    if(grantpt(ptm)<0)
    {
        close(ptm);
        return -2;
    }
    if(unlockpt(ptm) < 0)
    {
        close(ptm);
        return -3;
    }
   
    if (ptsname_r(ptm, pts_name, pts_namesz) != 0)
    {
        close(ptm);
        return -4;
    }

    return ptm;
}

int ptys_open(char *pts_name)
{
    int pts;
    if( (pts=open(pts_name, O_RDWR)) < 0)
    {
        return -5;
    }
    return pts;
}

pid_t pty_fork(int *ptrfdm, char *slave_name, int slave_namesz)
{
    int fdm,fds;
    pid_t pid;
    char pts_name[PATH_MAX];

    if( (fdm=ptym_open(pts_name, sizeof(pts_name))) < 0)
    {
        return -1;
    }
    if(slave_name)
    {
        strncpy(slave_name, pts_name, slave_namesz);
        slave_name[slave_namesz-1] = 0;
    }
    if( (pid=fork()) < 0)
    {
        return -1;
    }
    else if(pid == 0)
    {
        if (setsid() < 0)
        {
            perror("setsid");
            exit(-1);
        }
        if ( (fds=ptys_open(pts_name)) < 0)
        {
            perror("ptys_open");
            exit(-1);
        }

        close(fdm);
        if(dup2(fds, STDIN_FILENO) != STDIN_FILENO)
        {
            perror("dup2");
            exit(-1);
        }
        if(dup2(fds, STDOUT_FILENO) != STDOUT_FILENO)
        {
            perror("dup2");
            exit(-1);
        }
        if(dup2(fds, STDERR_FILENO) != STDERR_FILENO)
        {
            perror("dup2");
            exit(-1);
        }
        if( fds!=STDIN_FILENO && fds!=STDOUT_FILENO && fds!=STDERR_FILENO)
            close(fds);

        return 0;
    }

    *ptrfdm = fdm;

    return pid;
}

int sl_test_cmd(char *cmd)
{
    write(g_ctl.sock, "this is a test\n", sizeof("this is a test\n"));
    return 0;
}

int sl_change_win_size(char *cmd)
{
    int tty;
    char *row, *column;
    struct winsize ws;

    if( (row=strchr(cmd, ' ')) == NULL)
        return -1;
    row++;

    if( (column=strchr(row, ' ')) == NULL)
        return -1;
    column++;

    if( (tty=open(g_ctl.pts_name, O_RDWR)) < 0)
        return -1;

    g_ctl.change_pts_stage = 1;
    memset(&ws, 0, sizeof(ws));
    ws.ws_row = atoi(row);
    ws.ws_col = atoi(column);
    if( ioctl(tty, TIOCSWINSZ, (void *)&ws) < 0)
    {
        perror("ioctl TIOCSWINSZ");
    }
    close(tty);

    return 0;
}

void init_inner_cmd()
{
    int i;
    for(i=0;i<MAX_INNER_CMD_NUM;i++)
    {
        memset(g_inner_cmd+i, 0, sizeof(struct sl_inner_cmd));
    }

    strcpy(g_inner_cmd[0].name, "test");
    g_inner_cmd[0].func = (void *)sl_test_cmd;

    strcpy(g_inner_cmd[1].name, "cws");
    g_inner_cmd[1].func = (void *)sl_change_win_size;
}

int exec_buildin(char *cmd)
{
    int i;

    for(i=0;i<MAX_INNER_CMD_NUM;i++)
    {
        if(g_inner_cmd[i].func && strstr(cmd, g_inner_cmd[i].name))
        {
            g_inner_cmd[i].func(cmd);
            break;
        }
    }

    if(i>=MAX_INNER_CMD_NUM)
        return -1;

    return 0;
}

int filter_buildin_cmd(char *cmd, int len)
{
    int i;
    char tmpcmd[MAX_INNER_CMD_LEN];
    char data;
    char *end;
    int n;

    if(g_inner_cmd_enable==0 || *cmd != SLOGIN_BUILDIN_CHAR)
        return len;

    memcpy(tmpcmd, cmd+1, len);
    for(i=0;i<len;i++)
    {
        if(tmpcmd[i] == '\n')
        {
            end = tmpcmd+i;
            tmpcmd[i] = 0;
            end--;
            if(tmpcmd[i-1]=='\r')
            {
                tmpcmd[i-1] = 0;
                end--;
            }
            exec_buildin(tmpcmd);
            break;
        }
    }
    if(i < len)
        return 0;
    while( (n=read(g_ctl.sock, &data, 1)) > 0)
    {
        if(data==0x0D)
        {
            data = 0x0A;
            break;
        }
        tmpcmd[i] = data;
        i++;
    }
    if(n<=0)
        return -1;
    tmpcmd[i] = 0;
    if(tmpcmd[i-1] == '\r')
        tmpcmd[i-1] = 0;
    exec_buildin(tmpcmd);
    return  0;
}

void *start_login_rx_thread(void *unused)
{
    char data[100];
    int n;

    while(1)
    {
        if( (n=read(g_ctl.sock, data, 100)) <= 0)
        {
            if(n < 0)
                perror("sock read");
            break;
        }
        
        if( (n=filter_buildin_cmd(data, n)) < 0)
            break;
        if(n == 0)
            continue;

        if(write(g_ctl.ptm, data, n) <= 0)
        {
            perror("write");
            break;
        }
    }
    pthread_cancel(g_ctl.tid_tx);
    pthread_exit(NULL);
}

void *start_login_tx_thread(void *unused)
{
    char data[100];
    int n;

    while(1)
    {
        if( (n=read(g_ctl.ptm, data, 100)) <= 0)
        {
            if(g_ctl.change_pts_stage)
            {
                usleep(100000);
                g_ctl.change_pts_stage = 0;
                continue;
            }
            break;
        }
        if(write(g_ctl.sock, data, n) <= 0)
        {
            perror("write");
            break;
        }
    }
    pthread_cancel(g_ctl.tid_rx);
    pthread_exit(NULL);
}

void set_shell_env()
{
    int i;

    setenv("PATH", SL_DEF_PATH, 1);
    setenv("HOME", "/", 1);
    setenv("TERM", "xterm", 1);
    if(geteuid()==0)
        setenv("USER", "root", 1);

    for(i=0;i<g_env_num;i++)
    {
        setenv(g_env_strings[i].key, g_env_strings[i].value, 1);
    }
}

int exec_cmd(int sock, char *cmd)
{
    FILE *file;
    int n;

    if( (file=popen(cmd, "r")) == NULL)
    {
        return -1;
    }


    while( (n=fread(cmd, 1, DATA_BUF_LEN, file)) > 0)
    {
        if( send(sock, cmd, n, 0) != n)
        {
            pclose(file);
            return -1;
        }
    }

    pclose(file);
    if(n == 0)
        return 0;

    return -1;
}

int get_shell_env()
{
    int len, m_len, t_len;
    struct envstring *env_string;
    char hasenv;

    if(recv(g_ctl.sock, &hasenv, 1, 0) != 1)
    {
        return -1;
    }
    if(hasenv == 'N')
    {
        return 0;
    }
    else if(hasenv == 'C')
    {
        while(1)
        {
            m_len = 0;
            t_len = MAX_CMD_LEN;
            while( (len=recv(g_ctl.sock, buf+m_len, t_len-m_len, 0)) > 0)
            {
                m_len += len;
                if(m_len == t_len)
                    break;
            }
            if(len < 0)
                return -1;
            else
                break;
        }
        buf[m_len-1] = 0;
        exec_cmd(g_ctl.sock, buf);
        return -1;
    }


    g_env_num = 0;
    while(1)
    {
        m_len = 0;
        t_len = sizeof(struct envstring);
        while( (len=recv(g_ctl.sock, buf+m_len, t_len-m_len, 0)) > 0)
        {
            m_len += len;
            if(m_len == t_len)
                break;
        }
        if(len < 0)
            break;
        env_string = (struct envstring *)buf;
        if(env_string->more < 0)
            break;
        memcpy(g_env_strings+g_env_num, buf, sizeof(struct envstring));
        g_env_num++;
        if(env_string->more==0)
            break;
    }

    return len;
}
void sl_sig_exit(int signum)
{
    int st;

    waitpid(-1, &st, 0);
    close(g_ctl.ptm);
}

int start_login(int sock)
{
    struct sigaction act, oldact;
    pid_t pid;
    char pts_name[PATH_MAX];

    act.sa_handler = SIG_DFL;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    if(sigaction(SIGCHLD, &act, &oldact) < 0)
    {
        perror("sigaction");
        exit(-1);
    }

    g_ctl.sock = sock;
    if(get_shell_env() < 0)
    {
        exit(-1);
    }
    if ( (pid=pty_fork(&g_ctl.ptm, pts_name, 100)) < 0)
    {
        exit(-1);
    }
    else if(pid == 0)
    {
        char *shell;

        set_shell_env();
        chdir(getenv("HOME"));
        shell = getenv("SHELL");
        if(shell)
            execlp(shell, shell, "--login", NULL);
        setenv("SHELL", "bash",1);
        execlp("bash", "bash", "--login", NULL);
        setenv("SHELL", "sh", 1);
        execlp("sh", "-sh", "--login", NULL);

        perror("execl");
        exit(-1);
    }

    act.sa_handler = sl_sig_exit;
    if(sigaction(SIGCHLD, &act, NULL) < 0)
    {
        perror("sigaction");
        exit(-1);
    }

    strcpy(g_ctl.pts_name, pts_name);
    init_inner_cmd();
    if(pthread_create(&g_ctl.tid_rx, NULL, start_login_rx_thread, NULL) < 0)
    {
        exit(-1);
    }
    if(pthread_create(&g_ctl.tid_tx, NULL, start_login_tx_thread, NULL) < 0)
    {
        exit(-1);
    }

    pthread_join(g_ctl.tid_rx, NULL);
    pthread_join(g_ctl.tid_tx, NULL);

    return 0;
}
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

int daemonlize(char *pname, int facility)
{
    int pid;
    int open_max;
    int i;
    int pidfile;
    char temp[32];

    if( (pid=fork()) < 0)
        return -1;
    if(pid > 0)
    {
        exit(0);
    }

    if(setsid() < 0)
        return -1;

    if( (pid=fork()) < 0)
        return -1;
    if(pid > 0)
    {
        exit(0);
    }

    chdir("/tmp/");

    if( (open_max=sysconf(_SC_OPEN_MAX)) < 0)
    {
        open_max = NOFILE;
    }
    for(i=0;i<open_max;i++)
    {
        close(i);
    }
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);

    openlog(pname, LOG_PID, facility);

    if ( (pidfile=open(PIDFILE, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
    {
        syslog(LOG_CRIT, "open %s error: %m", PIDFILE);
        return -1;
    }

    if( lockf(pidfile, F_TLOCK, 0) < 0)
    {
        syslog(LOG_CRIT, "lock %s error, mybe one instance already exists", PIDFILE);
        return -1;
    }
    sprintf(temp, "%d", getpid());
    write(pidfile, temp, strlen(temp));

    return 0;
}

///@brief 取得工作模式
///@param fd socket句柄
///@return -1：错误：其他：工作模式
int get_work_mod(int fd, int *work_test_len)
{
    char work_mod_char;
    char ack;
    int tmp_work_test_len = 0;

    ack = 0;

    if(recv(fd, &work_mod_char, sizeof(work_mod_char), 0) < 0)
    {
        perror("recv");
        return -1;
    }
    if(send(fd, &ack, sizeof(ack), 0) != sizeof(ack))
    {
        perror("send");
        return -1;
    }
    if(work_mod_char  == 'C')
    {
        return WORK_MODE_CMD;
    }
    else if(work_mod_char == 'A')
    {
        if(recv(fd, &tmp_work_test_len, sizeof(int), 0) < 0)
        {
            perror("recv");
            return -1;
        }
        tmp_work_test_len = ntohl(tmp_work_test_len);
        *work_test_len = tmp_work_test_len;
        if(send(fd, &ack, sizeof(ack), 0) != sizeof(ack))
        {
            perror("send");
            return -1;
        }
        return WORK_MODE_TEST_ACK;
    }
    else if(work_mod_char == 'N')
    {
        if(recv(fd, &tmp_work_test_len, sizeof(int), 0) < 0)
        {
            perror("recv");
            return -1;
        }
        tmp_work_test_len = ntohl(tmp_work_test_len);
        *work_test_len = tmp_work_test_len;
        if(send(fd, &ack, sizeof(ack), 0) != sizeof(ack))
        {
            perror("send");
            return -1;
        }
        return WORK_MODE_TEST_NOACK;
    }
    else if(work_mod_char == 'E')
    {
        return WORK_MODE_TRANS_SAVE;
    }
    else if(work_mod_char == 'D')
    {
        return WORK_MODE_TRANS_SEND;
    }

    return -1;
}

int do_save_files(int fd, int trans_flag)
{
    int len;
    __u64 m_len,total_len,recvcount;
    struct timeval tv1, tv2;
    __u64 val;
    struct file_update_head head;
    FILE *file;
    __u64 pkts;
    int more;
    char filename[PATH_MAX];
    char cmd[PATH_MAX];
    long writecount;
    char savedir[PATH_MAX];

    recvcount = 0;
    writecount = 0;
    gettimeofday(&tv1, NULL);
    pkts = 0;
    memset(&head, 0, sizeof(struct file_update_head));
    more = 1;

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
            strncpy(buf, strerror(errno), MAX_ERROR_LEN);
            buf[MAX_ERROR_LEN-1] = 0;
            fprintf(stderr, "recv: %s\n", buf);
            return -1;
        }
        m_len += len;
        pkts++;
    }
    memcpy(&head, buf, sizeof(struct file_update_head));;
    N2H_FILE_UPDATE_HEAD(&head);
    recvcount += total_len;
    strcpy(savedir, head.dir);
    len = strlen(savedir);
    if(savedir[len-1] != '/')
    {
        savedir[len] = '/';
        savedir[len+1] = 0;
    }

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
                strncpy(buf, strerror(errno), MAX_ERROR_LEN);
                buf[MAX_ERROR_LEN-1] = 0;
                fprintf(stderr, "head recv: %s\n", buf);
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

        strcat(filename, head.name);
        if( (file = fopen(filename, "w")) == NULL)
        {
            strncpy(buf, strerror(errno), MAX_ERROR_LEN);
            buf[MAX_ERROR_LEN-1] = 0;
            fprintf(stderr, "fopen: %s, %s\n", buf, filename);
            return -1;
        }

        printf("receive file: %s\n", head.name);
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
                strncpy(buf, strerror(errno), MAX_ERROR_LEN);
                buf[MAX_ERROR_LEN-1] = 0;
                fprintf(stderr, "data recv: %s\n", buf);
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
    if(daemon_enale)
    {
        syslog(LOG_INFO, "received %ld packets, %ld bytes, %ld pps, %ld Bps,"
                "write %ld bytes, spend %ld microseconds.\n"
                , (long)pkts, (long)recvcount, (long)(pkts*1000000/val),
                (long)(recvcount*1000000/val), (long)writecount, (long)val);
    }
    else
    {
        printf("received %ld packets, %ld bytes, %ld pps, %ld Bps,"
                "write %ld bytes, spend %ld microseconds.\n"
                , (long)pkts, (long)recvcount, (long)(pkts*1000000/val),
                (long)(recvcount*1000000/val), (long)writecount, (long)val);
    }
    printf("\n");
    return 0;
}

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

int trans_file(int sock, char *pathname, struct file_update_head *head)
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
            strncpy(buf, strerror(errno), MAX_ERROR_LEN);
            buf[MAX_ERROR_LEN-1] = 0;
            fprintf(stderr, "head send: %s\n", buf);
            return -1;
        }
        m_len += len;
    }
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
                strncpy(buf, strerror(errno), MAX_ERROR_LEN);
                buf[MAX_ERROR_LEN-1] = 0;
                fprintf(stderr, "data send: %s\n", buf);
                fclose(file);
                return -1;
            }
            m_len += len;
        }
    }

    fclose(file);

    return 0;
}

int trans_dir(int sock, char *dirname, char *root, int last)
{
    FILE *file;
    DIR *dir;
    struct dirent *entry, *entry_n;
    char pathname[PATH_MAX];
    struct file_update_head head;
    int len;
    struct stat st;

    entry = NULL;
    dir = NULL;

    if( (dir=opendir(dirname)) == NULL)
    {
        strncpy(buf, strerror(errno), MAX_ERROR_LEN);
        buf[MAX_ERROR_LEN-1] = 0;
        fprintf(stderr, "opendir: %s, %s\n", buf, dirname);
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
        if(trans_file(sock, NULL, &head) < 0)
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
            if(trans_dir(sock, pathname, root, end) < 0)
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

        if(trans_file(sock, pathname, &head) < 0)
        {
            return -1;
        }

        entry = entry_n;
        if(entry == NULL)
            break;
    }

    return 0;
}


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

int send_files(int fd, int trans_flag, int datasrc, char *pathname)
{
    int ret;
    struct file_update_head head;
    char root[FILENAME_MAX];
    char * name;

    if(datasrc == DATASRC_FILE)
    {
        ret = 0;
        while(1)
        {
            int len;
            FILE *file;

            if( (file=fopen(pathname, "r")) == NULL)
            {
                return -1;
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

            if(trans_file(fd, pathname, &head) < 0)
            {
                fprintf(stderr, "translate file '%s' fail\n", pathname);
                return -1;
            }
            break;
        }
    }
    else if(datasrc == DATASRC_DIR)
    {
        if(get_dir_root(root, pathname) < 0)
        {
            return -1;
        }
        printf("root: %s\npathname: %s\n", root, pathname);
        ret = trans_dir(fd, pathname, root, 1);
    }

    return 0;
}

int do_send_files(int fd, int trans_flag)
{
    __u64 m_len,total_len;
    int len;
    int datasrc;
    struct stat st;
    struct file_update_head head;
    char pathname[PATH_MAX];
    char root[PATH_MAX];

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
            strncpy(buf, strerror(errno), MAX_ERROR_LEN);
            buf[MAX_ERROR_LEN-1] = 0;
            fprintf(stderr, "head recv: %s\n", buf);
            return -1;
        }
        m_len += len;
    }
    memcpy(&head, buf, sizeof(struct file_update_head));;
    N2H_FILE_UPDATE_HEAD(&head);
    strcpy(pathname, head.dir);

    if( (stat(pathname, &st)) < 0)
    {
        strncpy(buf, strerror(errno), MAX_ERROR_LEN);
        buf[MAX_ERROR_LEN-1] = 0;
        fprintf(stderr, "open: %s, %s\n", buf, pathname);
        return -1;
    }
    if(S_ISDIR(st.st_mode))
    {
        datasrc =DATASRC_DIR;
        if(get_dir_root(root, pathname) < 0)
        {
            printf("No such file or directory.\n");
            return -1;
        }
    }
    else if(S_ISREG(st.st_mode))
    {
        datasrc =DATASRC_FILE;
    }
    else
    {
        fprintf(stdout, "%s is not a file or directory\n", pathname);
        return -1;
    }

    return send_files(fd, trans_flag, datasrc, pathname);
}

///@brief 接收(发送)数据
///@param fd socket句柄
///@param work_mode WORK_MODE_TEST_NOACK:不发送数据，WORK_MODE_TEST_ACK:发送数据，WORK_MODE_TRANS_SAVE:写数据到文件
///@param trans_flag 数据传输方式
///@return -1：失败，0：成功
int start_work(int fd, int work_mode, int work_test_len, int trans_flag)
{
    int len, tmp_len_per;
    __u64 recvcount,sendcount;
    struct timeval tv1, tv2;
    __u64 val;
    __u64 pkts;

    tmp_len_per = 0;
    switch(work_mode)
    {
        case WORK_MODE_TEST_NOACK: case WORK_MODE_TEST_ACK:
            {
                recvcount = sendcount = 0;
                gettimeofday(&tv1,NULL);
                pkts = 0;
                for(;;)
                {
                    len=recv(fd, buf, DATA_BUF_LEN, trans_flag);
                    if(len == 0)
                        break;
                    if(len<0 && errno==EAGAIN)
                    {
                        continue;
                    }
                    if(len < 0)
                    {
                        strncpy(buf, strerror(errno), MAX_ERROR_LEN);
                        buf[MAX_ERROR_LEN-1] = 0;
                        fprintf(stderr, "recv: %s\n", buf);
                        return -1;
                    }
                    tmp_len_per += len;
                    if (tmp_len_per < work_test_len)
                    {
                        continue;
                    }
                    if(work_mode == WORK_MODE_TEST_ACK)
                    {
                        int slen,alen;
                        slen = 0;
                        while(slen<work_test_len)
                        {
                            alen = send(fd, buf+slen, work_test_len-slen, trans_flag);
                            if(alen<0 && errno==EAGAIN)
                            {
                                continue;
                            }
                            if(alen < 0)
                            {
                                close(fd);
                                return -1;
                            }
                            slen += alen;
                        }
                        sendcount += len;
                    }
                    pkts++;
                    recvcount += tmp_len_per;
                    tmp_len_per = 0;
                }
                gettimeofday(&tv2, NULL);
                val = tv2.tv_sec*1000000 + tv2.tv_usec - tv1.tv_sec*1000000 - tv1.tv_usec;
                if(work_mode == WORK_MODE_TEST_ACK)
                {
                    if(daemon_enale)
                    {
                        syslog(LOG_INFO, "received %ld packets, %ld bytes, %ld pps, %ld Bps,"
                                "send %ld bytes, spend %ld microseconds.\n",
                                (long)pkts, (long)recvcount, (long)(pkts*1000000/val),
                                (long)(recvcount*1000000/val), (long)sendcount, (long)val);
                    }
                    else
                    {
                        printf("received %ld packets, %ld bytes, %ld pps, %ld Bps,"
                                "send %ld bytes, spend %ld microseconds.\n",
                                (long)pkts, (long)recvcount, (long)(pkts*1000000/val),
                                (long)(recvcount*1000000/val), (long)sendcount, (long)val);
                    }
                }
                else
                {
                    if(daemon_enale)
                    {
                        syslog(LOG_INFO, "received %ld packets, %ld bytes, %ld pps, %ld Bps,"
                                "spend %ld microseconds.\n", 
                                (long)pkts, (long)recvcount, (long)(pkts*1000000/val),
                                (long)(recvcount*1000000/val), (long)val);
                    }
                    else
                    {
                        printf("received %ld packets, %ld bytes, %ld pps, %ld Bps,"
                                "spend %ld microseconds.\n", 
                                (long)pkts, (long)recvcount, (long)(pkts*1000000/val),
                                (long)(recvcount*1000000/val), (long)val);
                    }
                }
                close(fd);
            }
            printf("\n");
            break;
        case WORK_MODE_TRANS_SAVE:
            if(do_save_files(fd, trans_flag) < 0)
            {
                send(fd, buf, strlen(buf)+1, 0);
                return -1;
            }
            break;
        case WORK_MODE_TRANS_SEND:
            if(do_send_files(fd, trans_flag) < 0)
            {
                send(fd, buf, strlen(buf)+1, 0);
                return -1;
            }
            break;
        case WORK_MODE_CMD:
            {
                start_login(fd);
            }
            break;
    }

    return 0;
}

void get_child(int signum)
{
    int st;

    waitpid(-1, &st, 0);
}

void usage()
{
    printf("usage:\tslogind [-h] [-n] [-i] [-d] [-a cpu_id] [-p port]\n"
            "\t-h: print usage\n"
            "\t-n: nonblock translate mode\n"
            "\t-a: set cpu affinity\n"
            "\t-p: set listen port\n"
            "\t-i: disable inner command\n"
            "\t-d: deamonlize\n");
}


int main(int argc, char *argv[])
{
    int fd, workfd;
    struct sockaddr_in srvaddr, cliaddr;
    struct sigaction act;
    int pid;
    socklen_t len;
    short listen_port;
    int trans_flag;
    int work_mod;
    int affinity;
    int opt;
    int work_test_len;

    daemon_enale = 0;
    affinity = -1;
    trans_flag = 0;
    work_test_len = 0;
    g_inner_cmd_enable = 1;
    listen_port = DEFAULT_DATA_TRANS_PORT;
    while( (opt=getopt(argc, argv, optsring)) > 0)
    {
        switch(opt)
        {
            case 'h':
                usage();
                return 0;
            case 'n':
                trans_flag = MSG_DONTWAIT;
                break;
            case 'a':
                affinity = atoi(optarg);
                break;
            case 'p':
                listen_port = atoi(optarg);
                break;
            case 'd':
                daemon_enale = 1;
                break;
            case 'i':
                g_inner_cmd_enable = 1;
                break;
            default:
                usage();
                return 0;
        }
    }

    if(affinity != -1)
    {
        if(app_set_affinity(affinity) < 0)
        {
            usage();
            return -1;
        }
    }

    if(daemon_enale)
    {
        if(daemonlize("slogind", LOG_USER) < 0)
            return -1;
    }

    act.sa_handler = get_child;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if( sigaction(SIGCHLD, &act, NULL) < 0)
    {
        perror("sigaction");
        return -1;
    }

    srvaddr.sin_family = PF_INET;
    srvaddr.sin_addr.s_addr = INADDR_ANY;
    srvaddr.sin_port = htons(listen_port);

    if( (fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }
    if( bind(fd, (void *)&srvaddr, sizeof(srvaddr)) < 0)
    {
        perror("bind");
        return -1;
    }
    if(listen(fd, 5) < 0)
    {
        perror("listen");
        return -1;
    }
    len = sizeof(struct sockaddr_in);
    for(;;)
    {
        workfd = accept(fd, (void *)&cliaddr, &len);
        if(workfd < 0 && (errno == EINTR||errno==EAGAIN))
            continue;
        if(workfd < 0)
            break;
        pid=fork();
        if(pid == 0)
        {
            int st;

            close(fd);

            act.sa_handler = SIG_DFL;
            act.sa_flags = 0;
            sigemptyset(&act.sa_mask);
            if( sigaction(SIGCHLD, &act, NULL) < 0)
            {
                perror("sigaction");
                return -1;
            }

            if( (work_mod=get_work_mod(workfd, &work_test_len)) < 0)
            {
                exit(-1);
            }
            st= (start_work(workfd, work_mod, work_test_len, trans_flag));
            exit(st>>10);
        }
        close(workfd);
    }
    return 0;
}
