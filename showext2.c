#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <time.h>
#include <string.h>

#define BLKSIZE 1024

/* 定义相关结构体别名 */
typedef struct ext2_group_desc GD;
typedef struct ext2_super_block SUPER;
typedef struct ext2_inode INODE;
typedef struct ext2_dir_entry_2 DIR;

SUPER *sp;
GD *gp;
INODE *ip;
DIR *dp;

char buf[BLKSIZE];
char inodebuf[128]; 

unsigned int fd, firstdata, inodesize, blksize, iblock; 
char *dev = "/dev/sdb1"; /* 默认设备文件 */

/* 读取block信息 */
int get_block(int fd, unsigned int blk, char *buf)
{
	lseek(fd, blk*BLKSIZE,0); 
	return read(fd, buf, BLKSIZE);
}

/* 读取inode信息 */
int get_inode(int fd, unsigned int blk, char *buf, int offset)
{
	lseek(fd, 10 * BLKSIZE + offset * 128,0); 
	return read(fd, buf, 128);
}

/* 根据inode号获取相应文件夹目录 */
int list(int inodenum){
	int offset = inodenum - 1;
	get_inode(fd, iblock, inodebuf, offset);
	ip = (INODE *)inodebuf;
	struct ext2_dir_entry_2 *dp; /* dir_entry 指针 */
	char *cp; /* char指针 */
	printf("Directory Entries information:\n");
	printf("inode#     rec_len    name_len   name \n");
	for (int i=0; i<15; i++){ /* 遍历inode对应的block */
		if (ip->i_block[i]) { /* 针对非0的block显示目录 */
			int blk = ip->i_block[i]; 
			char buf[BLKSIZE], temp[256];
			get_block(fd, blk, buf); /* 将数据block读入buf */
			dp = (struct ext2_dir_entry_2 *)buf; /* 作为dir_entry */
			cp = buf;
			while(cp < buf + BLKSIZE){
				strncpy(temp, dp->name, dp->name_len); /* 将name变成字符串 */
				temp[dp->name_len] = 0; 
				printf("%-10d %-10d %-10d %-10s\n", dp->inode, dp->rec_len, dp->name_len, temp);
				cp += dp->rec_len; /* 将cp向前移动rec_len */
				dp = (struct ext2_dir_entry_2 *)cp; /* 进入下一个entry */
			} 
		}
	}
}

/* 结合书上的例子，先显示superblock，group descriptor，root inode的一些信息，最后显示根目录结构 */
int showroot(char *dev){
	int i;
	fd = open(dev, O_RDONLY); 
	if (fd < 0){
		printf("open failed\n"); exit(1); 
	}
	get_block(fd, 1, buf); /* 得到superblock */
	sp = (SUPER *)buf;
	firstdata = sp->s_first_data_block; 
	inodesize = sp->s_inode_size;
	blksize = 1024*(1<<sp->s_log_block_size);
	printf("Superblock information:\n"); 
	printf("first_data_block=%d block_size=%d inodesize=%d\n", firstdata, blksize, inodesize);
	printf("---------------------–\n");

	printf("Group Descriptor information:\n"); 
	get_block(fd, firstdata + 1, buf); /* 得到get group descriptor */
	gp = (GD *)buf;
	printf("bmap_block=%u imap_block=%u inodes_table=%u ",gp->bg_block_bitmap, gp->bg_inode_bitmap, gp->bg_inode_table);
	printf("free_blocks=%u free_inodes=%u used_dirs=%u\n", gp->bg_free_inodes_count, gp->bg_used_dirs_count);
	iblock = gp->bg_inode_table;
	printf("---------------------–\n");

	printf("root inode information:\n"); 
	get_inode(fd, iblock, inodebuf, 1);
	ip = (INODE *)inodebuf; /* ip指向#2 inode，也就是root inode */
	printf("mode=%4x ", ip->i_mode);
	printf("uid=%d gid=%d\n", ip->i_uid, ip->i_gid); 
	printf("size=%d\n", ip->i_size); 
	time(&ip->i_ctime);
	printf("ctime=%s", ctime(&ip->i_ctime)); 
	printf("links=%d\n", ip->i_links_count);
	for (i=0; i<15; i++){ 
		if (ip->i_block[i])
			printf("i_block[%d]=%d\n", i, ip->i_block[i]); 
	}
	printf("---------------------–\n");
	list(2); /* 显示根目录结构 */
        return 1;
}


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

        /* 没有输入命令 */
        if (!args[0])
            continue;
		
        /* 列出根目录的文件和子目录 */
	if (strcmp(args[0], "list") == 0) {
	    if (args[1]!=NULL) 
	        dev = args[1]; 
	    showroot(dev);
            continue;
        }

	/* 切换到子目录，或退出子目录，回到根目录 */
	if (strcmp(args[0], "cd") == 0) {
	    if (args[1]!=NULL) {
		if(strcmp(args[1], "/") == 0){
			list(2);
			continue;
		}
		struct ext2_dir_entry_2 *dp; /* dir_entry指针 */ 
		char *cp;  /* char指针 */
		for (int i=0; i<15; i++){ /* 找到需要切换到的文件夹对应的inode号 */
			if (ip->i_block[i]) { 
				int blk = ip->i_block[i]; 
				char buf[BLKSIZE], temp[256];
				get_block(fd, blk, buf); 
				dp = (struct ext2_dir_entry_2 *)buf; 
				cp = buf;
				while(cp < buf + BLKSIZE){
					strncpy(temp, dp->name, dp->name_len); 
					temp[dp->name_len] = 0; 
					if(strcmp(temp, args[1]) == 0){
						list(dp->inode);
						break;
					}
					cp += dp->rec_len; 
					dp = (struct ext2_dir_entry_2 *)cp;
			        } 
		         }
	        }
	    }
	    continue;
         }
	
	/* 退出控制台 */	
        if (strcmp(args[0], "quit") == 0)
            return 0;
    }
}
