#ifndef _TASKS_H_
#define _TASKS_H_
#include "types.h"
#include "stdlib.h"
#include "list.h"

#define KERNEL_USER 0
#define NORMAL_USER 1000

#define TASK_NAME_LEN 16
#define TASK_FILE_NR 16//一个进程最多可以打开16个文件描述符表

typedef void *target_t;

//进程状态
typedef enum task_state_t {
    TASK_INIT, //初始化
    TASK_READY, //就绪
    TASK_RUNNING, //执行
    TASK_BLOCKED, //阻塞
    TASK_SLEEPING, //睡眠
    TASK_WAITING, //等待
    TASK_DEAD,//死亡
} task_state_t;

struct inode_t;
struct file_t;
//进程控制块
typedef struct task_t {
    uint32 *stack; //进程(线程)内核栈
    list_node_t node;//进程(线程)阻塞节点
    task_state_t state; //进程(线程)状态
    uint32 priority; //进程(线程)优先级
    uint32 ticks; //进程(线程)剩余时间片
    uint32 jiffies; //进程(线程)上次执行时的全局时间片
    char name[TASK_NAME_LEN];//进程(线程)名
    uint32 uid; //进程用户id KERNERL_USER OR NORMAL_USER
    uint32 gid;//进程用户组id
    pid_t pid; //进程id
    pid_t ppid; //父进程id
    uint32 pde; //进程页目录物理地址
    struct bitmap_t *vmap;//进程虚拟内存位图
    uint32 text; //代码段地址
    uint32 data; //数据段地址
    uint32 end; //程序结束地址
    uint32 brk;//进程堆内最高地址
    int status; //进程exit系统调用的形参存放
    pid_t waitpid; //进程等待的pid
    char *pwd;//进程当前目录名字
    struct inode_t *ipwd;//进程当前目录inode
    struct inode_t *iroot; //进程根目录 inode
    struct inode_t *iexec; //程序文件 inode
    uint16 umask; //进程用户权限
    struct file_t *files[TASK_FILE_NR];//进程文件描述符表项指针数组
    uint32 magic; //内核魔术，用于检测栈溢出
} task_t;


typedef struct task_frame_t {
    uint32 edi;
    uint32 esi;
    uint32 ebx;
    uint32 ebp;
    void (*eip)(void);
} task_frame_t;


// 中断帧
typedef struct intr_frame_t
{
    uint32 vector;

    uint32 edi;
    uint32 esi;
    uint32 ebp;
    // 虽然 pushad 把 esp 也压入，但 esp 是不断变化的，所以会被 popad 忽略
    uint32 esp_dummy;

    uint32 ebx;
    uint32 edx;
    uint32 ecx;
    uint32 eax;

    uint32 gs;
    uint32 fs;
    uint32 es;
    uint32 ds;

    uint32 vector0;

    uint32 error;

    uint32 eip;
    uint32 cs;
    uint32 eflags;
    uint32 esp;
    uint32 ss;
} intr_frame_t;

task_t *running_task();

void schedule();

void task_init();

void task_to_user_mode();



void task_block(task_t *task, list_t *blist, task_state_t state);

void task_unblock(task_t *task);

void task_weakup();



pid_t sys_getpid();//getpid系统调用处理函数

pid_t sys_getppid();//getppid系统调用处理函数

void task_sleep(uint32 ms);//slepp系统调用处理函数

void task_yield();//yield系统调用处理函数

pid_t task_fork();//fork系统调用处理函数

void task_exit(int status);//exit系统调用处理函数

pid_t task_waitpid(pid_t pid, int32 *status);

fd_t task_get_fd(task_t *task);

void task_put_fd(task_t *task, fd_t fd);

#endif