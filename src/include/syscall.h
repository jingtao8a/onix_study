#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "stat.h"

typedef enum syscall_t {
    SYS_NR_TEST,
    SYS_NR_EXIT = 1,
    SYS_NR_FORK = 2,
    SYS_NR_READ = 3,
    SYS_NR_WRITE = 4,
    SYS_NR_OPEN = 5,
    SYS_NR_CLOSE = 6,
    SYS_NR_WAITPID = 7,
    SYS_NR_CREAT = 8,
    SYS_NR_LINK = 9,
    SYS_NR_UNLINK = 10,
    SYS_NR_EXECVE = 11,
    SYS_NR_CHDIR = 12,
    SYS_NR_TIME = 13,
    SYS_NR_MKNOD = 14,
    SYS_NR_STAT = 18,
    SYS_NR_LSEEK = 19,
    SYS_NR_GETPID = 20,
    SYS_NR_MOUNT = 21,
    SYS_NR_UMOUNT = 22,
    SYS_NR_FSTAT = 28,
    SYS_NR_MKDIR = 39,
    SYS_NR_RMDIR = 40,
    SYS_NR_DUP = 41,
    SYS_NR_PIPE = 42,
    SYS_NR_BRK = 45,
    SYS_NR_UMASK = 60,
    SYS_NR_CHROOT = 61,
    SYS_NR_DUP2 = 63,
    SYS_NR_GETPPID = 64,
    SYS_NR_READDIR = 89,
    SYS_NR_MMAP = 90,
    SYS_NR_MUNMAP = 91,
    SYS_NR_SLEEP = 158,
    SYS_NR_YIELD = 162,
    SYS_NR_GETCWD = 183,
    SYS_NR_CLEAR = 200, 
    SYS_NR_MKFS = 201,
}syscall_t;


enum mmap_type_t {
    //prot
    PROT_NONE = 0,
    PROT_READ = 1,
    PROT_WRITE = 2,
    PROT_EXEC = 4,
    //flags
    MAP_SHARED = 1,
    MAP_PRIVATE = 2,
    MAP_FIXED =  0x10,
};

uint32 test();

pid_t getpid();

uint32 brk(void *addr);
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);


//打开文件
fd_t open(char *filename, int flags, int mode);
//创建普通文件
fd_t creat(char *filename, int mode);
//关闭文件
void close(fd_t fd);

pid_t getppid();

void sleep(uint32 ms);

void yield();

pid_t fork();

void exit(int status);

pid_t waitpid(pid_t pid, int32 *status);

time_t time();

mode_t umask(mode_t mask);

int mkdir(char *pathname, int mode);

int rmdir(char *pathname);

int link(char *oldname, char *name);

int unlink(char *filename);

int read(fd_t fd, char *buf, int len);

int write(fd_t fd, char *buf, int len);

int lseek(fd_t fd, off_t offset, int whence);

//获取当前路径
char *getcwd(char *buf, size_t size);

//切换当前目录
int chdir(char *pathname);

//切换根目录
int chroot(char *pathname);

//读目录
int readdir(fd_t fd, void *dir, uint32 count);

//清屏系统调用
void clear();

//获取文件状态
int stat(char *filename, stat_t *statbuf);

int fstat(fd_t fd, stat_t *statbuf);

//创建设备文件
int mknod(char *filename, int mode, int dev);

//挂载设备
int mount(char *devname, char *dirname, int flags);

//卸载设备
int umount(char *target);

//格式化文件系统
int mkfs(char *devname, int icount);

//执行程序
int execve(char *filename, char *argv[], char *envp[]);

//复制文件描述符
fd_t dup(fd_t oldfd);
fd_t dup2(fd_t oldfd, fd_t newfd);

//创建管道
int pipe(fd_t pipefd[2]);

#endif