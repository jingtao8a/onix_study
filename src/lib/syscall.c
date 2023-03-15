#include "../include/syscall.h"
#include "../include/types.h"

static _inline uint32 _syscall0(uint32 nr) {//0个参数的系统调用
    uint32 ret;
    asm volatile(
        "int $0x80\n"//0x80软中断
        : "=a"(ret)
        : "a"(nr));//nr位系统调用号
    return ret;
}

static _inline uint32 _syscall1(uint32 nr, uint32 arg1) {
    uint32 ret;
    asm volatile(
        "int $0x80\n"//0x80软中断
        : "=a"(ret)
        : "a"(nr), "b"(arg1));
    return ret;
}

static _inline uint32 _syscall2(uint32 nr, uint32 arg1, uint32 arg2) {
    uint32 ret;
    asm volatile (
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2));
    return ret;
}

static _inline uint32 _syscall3(uint32 nr, uint32 arg1, uint32 arg2, uint32 arg3) {
    uint32 ret;
    asm volatile (
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3));
    return ret;
}

static _inline uint32 _syscall4(uint32 nr, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4) {
    uint32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
        return ret;
}

static _inline uint32 _syscall5(uint32 nr, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5) {
    uint32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5));
    return ret;
}

static _inline uint32 _syscall6(uint32 nr, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5, uint32 arg6) {
    uint32 ret;
    asm volatile(
        "pushl %%ebp\n"
        "movl %7, %%ebp\n"
        "int $0x80\n"
        "popl %%ebp\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5), "m"(arg6));
    return ret;
}

uint32 test() {
    return _syscall0(SYS_NR_TEST);
}

pid_t getpid() {
    return _syscall0(SYS_NR_GETPID);
}

uint32 brk(void *addr)  {
    return _syscall1(SYS_NR_BRK, (uint32)addr);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return (void *)_syscall6(SYS_NR_MMAP, (uint32)addr, (uint32)length, (uint32)prot, (uint32)flags, (uint32)fd, (uint32)offset);
}

int munmap(void *addr, size_t length) {
    return _syscall2(SYS_NR_MUNMAP, (uint32)addr, (uint32)length);
}


//打开文件
fd_t open(char *filename, int flags, int mode) {
    return _syscall3(SYS_NR_OPEN, (uint32)filename, (uint32)flags, (uint32)mode);
}
//创建普通文件
fd_t creat(char *filename, int mode) {
    return _syscall2(SYS_NR_CREAT, (uint32)filename, (uint32)mode);
}
//关闭文件
void close(fd_t fd) {
    _syscall1(SYS_NR_CLOSE, (uint32)fd);
}

pid_t getppid() {
    return _syscall0(SYS_NR_GETPPID);
}

void yield() {
    _syscall0(SYS_NR_YIELD);
}


void sleep(uint32 ms) {
    _syscall1(SYS_NR_SLEEP, ms);
}

int read(fd_t fd, char *buf, int len) {
    return _syscall3(SYS_NR_READ, fd, (uint32)buf, len);
}

int write(fd_t fd, char *buf, int len) {
    return _syscall3(SYS_NR_WRITE, fd, (uint32)buf, len);
}

pid_t fork() {
    return _syscall0(SYS_NR_FORK);
}

void exit(int status) {
    _syscall1(SYS_NR_EXIT, (uint32)status);
}

pid_t waitpid(pid_t pid, int32 *status) {
    _syscall2(SYS_NR_WAITPID, (uint32)pid, (uint32)status);
}

time_t time() {
    return _syscall0(SYS_NR_TIME);
}

mode_t umask(mode_t mask) {
    return _syscall1(SYS_NR_UMASK, (uint32)mask);
}

int mkdir(char *pathname, int mode) {
    return _syscall2(SYS_NR_MKDIR, (uint32)pathname, (uint32)mode);
}

int rmdir(char *pathname) {
    return _syscall1(SYS_NR_RMDIR, (uint32)pathname);
}

int link(char *oldname, char *name) {
    return _syscall2(SYS_NR_LINK, (uint32)oldname, (uint32)name);
}

int unlink(char *filename) {
    return _syscall1(SYS_NR_UNLINK, (uint32)filename);
}

int lseek(fd_t fd, off_t offset, int whence) {
    return _syscall3(SYS_NR_LSEEK, (uint32)fd, (uint32)offset, (uint32)whence);
}


//获取当前路径
char *getcwd(char *buf, size_t size) {
    return (char *)_syscall2(SYS_NR_GETCWD, (uint32)buf, (uint32)size);
}

//切换当前目录
int chdir(char *pathname) {
    return _syscall1(SYS_NR_CHDIR, (uint32)pathname);
}

//切换根目录
int chroot(char *pathname) {
    return _syscall1(SYS_NR_CHROOT, (uint32)pathname);
}

//读目录
int readdir(fd_t fd, void *dir, uint32 count) {
    return _syscall3(SYS_NR_READDIR, (uint32)fd, (uint32)dir, count);
}

//清屏系统调用
void clear() {
    _syscall0(SYS_NR_CLEAR);
}

int stat(char *filename, stat_t *statbuf) {
    return _syscall2(SYS_NR_STAT, (uint32)filename, (uint32)statbuf);
}

int fstat(fd_t fd, stat_t *statbuf) {
    return _syscall2(SYS_NR_FSTAT, fd, (uint32)statbuf);
}

int mknod(char *filename, int mode, int dev) {
    return _syscall3(SYS_NR_MKNOD, (uint32)filename, (uint32)mode, (uint32)dev);
}

//挂载设备
int mount(char *devname, char *dirname, int flags) {
    return _syscall3(SYS_NR_MOUNT, (uint32)devname, (uint32)dirname, (uint32)flags);
}

//卸载设备
int umount(char *target) {
    return _syscall1(SYS_NR_UMOUNT, (uint32)target);
}

//格式化文件系统
int mkfs(char *devname, int icount) {
    return _syscall2(SYS_NR_MKFS, (uint32)devname, (uint32)icount);
}

//执行程序
int execve(char *filename, char *argv[], char *envp[]) {
    return _syscall3(SYS_NR_EXECVE, (uint32)filename, (uint32)argv, (uint32)envp);
}


fd_t dup(fd_t oldfd) {
    return _syscall1(SYS_NR_DUP, (uint32)oldfd);
}

fd_t dup2(fd_t oldfd, fd_t newfd) {
    return _syscall2(SYS_NR_DUP2, (uint32)oldfd, (uint32)newfd);
}

int pipe(fd_t pipefd[2]) {
    return _syscall1(SYS_NR_PIPE, (uint32)pipefd);
}