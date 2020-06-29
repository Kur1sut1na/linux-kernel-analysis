#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>

#define MAX_LEN 20

struct ps_info *trav_dir(char dir[]); /* 遍历指定目录，并建立进程信息链表*/
int read_info(char d_name[],struct ps_info *p1); /* 读取进程相关信息 */
void print_ps(struct ps_info *head); /* 打印进程信息 */
int is_num(char *); /* 判断是否为数字（纯数字命名的目录即为进程）*/
void uid_to_name(); /* 由进程uid得到进程的所有者user */

/* 进程信息结构体 */
typedef struct ps_info
{
    char pname[MAX_LEN];
    char user[MAX_LEN];
    int  pid;
    int  ppid;
    char state;
    char* cpus;
    struct ps_info *next;
}mps;

int main() {
    /* 存储输入的命令 */
    char cmd[256];
    /* 存储命令拆解后的各部分，以空指针结尾 */
    char *args[128];
    while (1) {
        /* 提示符 */
        printf("# ");
        fflush(stdin);
        fgets(cmd, 256, stdin);
        /* 清理结尾的换行符 */
        int i;
        for (i = 0; cmd[i] != '\n'; i++)
            ;
        cmd[i] = '\0';
        /* 拆解命令行 */
        args[0] = cmd;
        for (i = 0; *args[i]; i++){
            for (args[i+1] = args[i] + 1; *args[i+1]; args[i+1]++)
                if (*args[i+1] == ' ') {
                    *args[i+1] = '\0';
                    args[i+1]++;
                    break;
                }
	}
        args[i] = NULL;
	int last = i;

        /* 没有输入命令 */
        if (!args[0])
            continue;

	if (strcmp(args[0], "run") == 0) {
	/* 运行外部程序 */
	    int start = 1;
	    pid_t pid = fork();
	    if (pid == 0) {
            /* 子进程 */
	        if(strcmp(args[1], "-c") == 0){ /* 指定运行的cpu */
		    start = 3;
		    cpu_set_t mask;
    		    CPU_ZERO(&mask);    /* 初始化set集，将set置为空 */
		    CPU_SET(atoi(args[2])-1, &mask); /* 将指定的cpu加入到集合，本人虚拟机分配了2个cpu */
		    /*设置cpu亲和性（affinity）*/
    		    if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
        		printf("Set CPU affinity failue, ERROR:%s, Please input a number in 1-3 (1:cpu1,2:cpu2,3:both)\n ", strerror(errno));
        		return -1;
    		    }

		}
    		//usleep(1000);
		/* 提取外部程序的命令行部分，并运行该程序 */
		char *part[128];
		int j;
		for(i = start,j=0; i< last;i++,j++){
			part[j]=args[i];
		}
		part[j]=NULL;
                execvp(part[0],part);
            }
            /* 父进程 */
            wait(NULL);
	    }

        /* 内建命令 */
        if (strcmp(args[0], "list") == 0) {  /* 列出正在运行的进程的相关信息，具体信息通过访问/proc/下相关文件获取 */
            mps *head,*link;
    	    head=trav_dir("/proc/");
            
	    if(head==NULL)
		printf("traverse dir error\n");
				
	    print_ps(head);

            while(head!=NULL){  /* 释放链表 */
		link=head;
		head=head->next;
		free(link);
	    }
            continue;
        }

	if (strcmp(args[0], "kill") == 0){  /* 杀死指定进程 */
	    if(args[1] == NULL){ /* 需要提供进程号作为参数 */
		printf("Please input a pid. \n");
		continue;
            }
	    pid_t pid = atoi(args[1]);
	    kill(pid,SIGTERM);
	    continue;
	}
		
        if (strcmp(args[0], "quit") == 0) /*退出控制台*/
            return 0;
    }
}

mps *trav_dir(char dir[]) /* 遍历指定目录，并建立进程信息链表*/
{
    DIR *dir_ptr;
    mps *head,*p1,*p2;
    struct dirent *direntp;
    struct stat infobuf;

    if((dir_ptr=opendir(dir))==NULL)
        fprintf(stderr,"dir error %s\n",dir);
    else
    {
        head=p1=p2=(struct ps_info *)malloc(sizeof(struct ps_info)); /*建立链表*/
        while((direntp=readdir(dir_ptr)) != NULL)  /*遍历/proc目录所有进程目录*/
        {
            if((is_num(direntp->d_name))==0)   /*判断目录名字是否为纯数字*/
            {
                if(p1==NULL)
                {
                    printf("malloc error!\n");
                    exit(0);
                }

                if(read_info(direntp->d_name,p1)!=0)   /*获取进程信息*/
                {
                    printf("read_info error\n");
                    exit(0);
                }
                p2->next=p1;   /*插入新节点*/
                p2=p1;
                p1=(struct ps_info *)malloc(sizeof(struct ps_info));
            }
        }
    }
    p2->next=NULL;
    return head;
}

int read_info(char d_name[],struct ps_info *p1) /* 读取进程相关信息 */
{
    FILE *fd;
    char dir[20];
    struct stat infobuf;

    cpu_set_t get;

    int j=0;
    int cpus = sysconf(_SC_NPROCESSORS_CONF);

    sprintf(dir,"%s/%s","/proc/",d_name);
    chdir("/proc");     /*切换至/proc目录*/
    if(stat(d_name,&infobuf)==-1)  /*得到进程USER*/
        fprintf(stderr,"stat error %s\n",d_name);
    else
        uid_to_name(infobuf.st_uid, p1);
    
    chdir(dir);  /*切换到/proc/pid目录*/
    if((fd=fopen("stat","r"))<0)
    {
        printf("open the file is error!\n");
        exit(0);
    }
    while(4==fscanf(fd,"%d %s %c %d\n",&(p1->pid),p1->pname,&(p1->state),&(p1->ppid)))     /*读文件的前四个字段内容，并存放在相关的链表成员中*/
    {
        break;
    }
    fclose(fd);

    pid_t pid = p1->pid;
    CPU_ZERO(&get);
    if (sched_getaffinity(pid, sizeof(get), &get) == -1) {
        printf("get CPU affinity failue, ERROR:%s\n", strerror(errno));
        return -1;
    }
    int count = 0;
    for(int i = 0; i < cpus; i++) {
	j = i+1;
        if (CPU_ISSET(i, &get)) {  /*查看cpu i是否在get集合当中*/
	    count+=j;
        }
    }
    if(count==1)
	p1->cpus = "cpu1";
    else if(count==2)
	p1->cpus = "cpu2";
    else
	p1->cpus = "cpu1,cpu2";	
    return 0;
}

void uid_to_name(uid_t uid, struct ps_info *p1)  /* 由进程uid得到进程的所有者user */
{
    struct passwd *getpwuid(), *pw_ptr;
    static char numstr[10];

    if((pw_ptr=getpwuid(uid)) == NULL)
    {
        sprintf(numstr,"%d",uid);
        strcpy(p1->user, numstr);
    }
    else
        strcpy(p1->user, pw_ptr->pw_name);
}

int is_num(char p_name[]) /* 判断是否为数字 */
{
    int i,len;
    len=strlen(p_name);
    if(len==0) return -1;
    for(i=0;i<len;i++)
        if(p_name[i]<'0' || p_name[i]>'9')
            return -1;
    return 0;
}

void print_ps(struct ps_info *head) /* 打印进程信息 */
{
    mps *list;
    printf("USER                \t\tPID\tPPID\tSTATE\tPNAME               \tCPUs\n");
    for(list=head;list!=NULL;list=list->next)
    {
	printf("%-20s\t\t%d\t%d\t%c\t%-20s\t%s\n",list->user,list->pid,list->ppid,list->state,list->pname,list->cpus);
    }
}
