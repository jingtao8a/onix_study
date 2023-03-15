#include "../include/assert.h"
#include "../include/debug.h"
#include "../include/interrupt.h"
#include "../include/syscall.h"
#include "../include/tasks.h"
#include "../include/console.h"
#include "../include/memory.h"
#include "../include/clock.h"
#include "../include/ide.h"
#include "../include/string.h"
#include "../include/device.h"
#include "../include/buffer.h"
#include "../include/system.h"
#include "../include/fs.h"
#include "../include/execve.h"

#define SYSCALL_SIZE 256
handler_t syscall_table[SYSCALL_SIZE];//系统调用函数表

void syscall_check(uint32 nr) {
    if (nr >= SYSCALL_SIZE) {
        panic("syscall nr error");
    }
}

static void sys_default() {
    panic("syscall not implement");
}

static uint32 sys_test() {
    char ch;
    device_t *device;
    device_t *serial = device_find(DEV_SERIAL, 0);
    assert(serial);

    device_t *keyboard = device_find(DEV_KEYBOARD, 0);
    assert(keyboard);

    device_t *console = device_find(DEV_CONSOLE, 0);
    assert(console);

    // device_read(keyboard->dev, &ch, 1, 0, 0);//从键盘读数据
    device_read(serial->dev, &ch, 1, 0, 0);//从串口读数据
    device_write(serial->dev, &ch, 1, 0, 0);//从串口写数据
    device_write(console->dev, &ch, 1, 0, 0);//往控制台写数据
    return 255;
}

void syscall_init() {
    for (size_t i = 0; i < SYSCALL_SIZE; ++i) {
        syscall_table[i] = sys_default;
    }
    syscall_table[SYS_NR_TEST] = sys_test;
    syscall_table[SYS_NR_TIME] = sys_time;
    syscall_table[SYS_NR_READ] = sys_read;
    syscall_table[SYS_NR_WRITE] = sys_write;
    syscall_table[SYS_NR_GETPID] = sys_getpid;
    syscall_table[SYS_NR_BRK] = sys_brk;
    syscall_table[SYS_NR_GETPPID] = sys_getppid;
    syscall_table[SYS_NR_SLEEP] = task_sleep;
    syscall_table[SYS_NR_YIELD] = task_yield;
    syscall_table[SYS_NR_FORK] = task_fork;
    syscall_table[SYS_NR_EXIT] = task_exit;
    syscall_table[SYS_NR_WAITPID] = task_waitpid;
    syscall_table[SYS_NR_UMASK] = sys_umask;
    syscall_table[SYS_NR_MKDIR] = sys_mkdir;
    syscall_table[SYS_NR_RMDIR] = sys_rmdir;
    syscall_table[SYS_NR_LINK] = sys_link;
    syscall_table[SYS_NR_UNLINK] = sys_unlink;
    syscall_table[SYS_NR_OPEN] = sys_open;
    syscall_table[SYS_NR_CREAT] = sys_creat;
    syscall_table[SYS_NR_CLOSE] = sys_close;
    syscall_table[SYS_NR_LSEEK] = sys_lseek;
    syscall_table[SYS_NR_CHDIR] = sys_chdir;
    syscall_table[SYS_NR_CHROOT] = sys_chroot;
    syscall_table[SYS_NR_GETCWD] = sys_getcwd;
    syscall_table[SYS_NR_READDIR] = sys_readdir;
    syscall_table[SYS_NR_CLEAR] = console_clear;
    syscall_table[SYS_NR_STAT] = sys_stat;
    syscall_table[SYS_NR_FSTAT] = sys_fstat;
    syscall_table[SYS_NR_MKNOD] = sys_mknod;
    syscall_table[SYS_NR_MOUNT] = sys_mount;
    syscall_table[SYS_NR_UMOUNT] = sys_umount;
    syscall_table[SYS_NR_MKFS] = sys_mkfs;
    syscall_table[SYS_NR_MMAP] = sys_mmap;
    syscall_table[SYS_NR_MUNMAP] = sys_munmap;
    syscall_table[SYS_NR_EXECVE] = sys_execve;
    syscall_table[SYS_NR_DUP] = sys_dup;
    syscall_table[SYS_NR_DUP2] = sys_dup2;
    syscall_table[SYS_NR_PIPE] = sys_pipe;
}

