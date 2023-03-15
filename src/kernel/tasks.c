#include "../include/tasks.h"
#include "../include/printk.h"
#include "../include/memory.h"
#include "../include/assert.h"
#include "../include/string.h"
#include "../include/interrupt.h"
#include "../include/stdlib.h"
#include "../include/syscall.h"
#include "../include/thread.h"
#include "../include/clock.h"
#include "../include/mutex.h"
#include "../include/global.h"
#include "../include/arena.h"
#include "../include/debug.h"
#include "../include/fs.h"
#include "../include/execve.h"

extern void task_switch(task_t *);
extern file_t file_table[];

#define NR_TASKS 64
static task_t *task_table[NR_TASKS];//进程控制块PCB指针数组
static list_t block_list; //进程默认阻塞链表
static list_t sleep_list; //进程默认睡眠链表
static task_t *idle_task;//指向task_table中一个idle_task

//在PCB数组中查找某种状态的PCB块
static task_t* task_search(task_state_t state) {
    task_t *task = NULL;
    task_t *current = running_task();

    for (size_t i = 0; i < NR_TASKS; ++i) {
        task_t *ptr = task_table[i];
        if (ptr == NULL || ptr->state != state || ptr == current) {
            continue;
        }
        if (task == NULL || task->ticks < ptr->ticks || task->jiffies > ptr->jiffies) {//task->jiffies的设置用于防止饥饿
            task = ptr;
        }
    }
    if (task == NULL && state == TASK_READY) {//如果查找一遍PCB数组，不存在TASK_READY的进程，就准备调度idle进程
        task = idle_task;//idle_task是作为备选的
    }
    return task;
}

task_t *running_task() {//返回当前运行PCB块指针
    asm volatile(
        "movl %esp, %eax\n"
        "andl $0xfffff000, %eax");
}

void task_yield() {
    schedule();
}

void task_block(task_t *task, list_t *blist, task_state_t state) {//将某个PCB块push进block_list,并且修改它的state
    assert(!get_interrupt_state());//已经关闭中断
    assert(task->node.next == NULL);
    assert(task->node.prev == NULL);
    if (blist == NULL) {
        blist = &block_list;//如果没有阻塞队列，使用默认的阻塞队列
    }
    list_push(blist, &task->node);
    assert(state != TASK_READY && state != TASK_RUNNING);
    task->state = state;

    task_t *current = running_task();
    if (current == task) {//阻塞的是自己
        schedule();//切换到别的进程执行
    }
}

void task_unblock(task_t *task) {//将某个PCB块从block_list中删除，并且修改它的state为READY
    assert(!get_interrupt_state());//已经关闭中断
    list_remove(&task->node);
    assert(task->node.next == NULL && task->node.prev == NULL);
    task->state = TASK_READY;
}

void task_sleep(uint32 ms) {
    assert(!get_interrupt_state()); //已经关闭中断

    uint32 ticks = ms / jiffy;        // 需要睡眠的时间片
    ticks = ticks > 0 ? ticks : 1; // 至少休眠一个时间片
 
    // 记录目标全局时间片，在那个时刻需要唤醒任务
    task_t *current = running_task();
    current->ticks = jiffies + ticks;

    // 从睡眠链表找到第一个比当前任务唤醒时间点更晚的任务，进行插入排序
    //offset为task_t结构体类型中ticks成员和node成员的偏移
    list_insert_sort(&sleep_list, &current->node, element_node_offset(task_t, node, ticks));

    // 阻塞状态是睡眠
    current->state = TASK_SLEEPING;

    // 调度执行其他任务
    schedule();
}

void task_weakup() {
    assert(!get_interrupt_state()); // 已经关闭中断
    // 从睡眠链表中找到 ticks 小于等于 jiffies 的任务，恢复执行
    list_t *list = &sleep_list;
    //将睡眠时间结束得进程唤醒
    for (list_node_t *ptr = list->head.next; ptr != &list->tail;)
    {
        task_t *task = element_entry(task_t, node, ptr);
        if (task->ticks > jiffies)
        {
            break;
        }

        // unblock 会将指针清空
        ptr = ptr->next;

        task->ticks = task->priority;
        task_unblock(task);
    }
}

static void task_activate(task_t *task) {
    assert(task->magic == ONIX_MAGIC);
    if (task->pde != get_cr3()) {//页目录需要更换
        set_cr3(task->pde);
    }
    if (task->uid != KERNEL_USER) {//如果要切换的下一个任务是用户级的,那么设置tss的esp0字段
        tss.esp0 = (uint32)task + PAGE_SIZE; 
    }
}

void schedule() {
    assert(!get_interrupt_state());//已经关闭了中断
    task_t *current = running_task();
    task_t *next = task_search(TASK_READY);//找到一个处于就绪态的PCB块
    assert(next != NULL);
    assert(next->magic == ONIX_MAGIC);
    if (current->ticks == 0) {
        current->ticks = current->priority;
    }

    if (current->state == TASK_RUNNING) {
        current->state = TASK_READY;
    }
    if (next->state != TASK_READY) {
        if (next->state == TASK_SLEEPING) {
            next->ticks = next->priority;
        }
        task_unblock(next);
    }
    next->state = TASK_RUNNING;
    task_activate(next);
    task_switch(next);
}

static task_t *get_free_task() {
    for (size_t i = 0; i < NR_TASKS; ++i) {
        if (task_table[i] == NULL) {//找到一个没有使用的PCB块
            task_t *task = (task_t *)alloc_kpage(1);
            memset(task, 0, PAGE_SIZE);
            task->pid = i;
            // task->ppid = -1;
            task_table[i] = task;
            return task_table[i];
        }
    }
    panic("No more PCBs\n");
}

static task_t* task_create(target_t target, const char* name, uint32 priority, uint32 uid) {
    task_t *task = get_free_task();
    uint32 stack = (uint32)task + PAGE_SIZE - sizeof(task_frame_t);
    
    strcpy(task->name, name);
    task->stack = (uint32 *)stack;//指向该进程的内核栈
    task->priority = priority;//优先级
    task->ticks = task->priority;//时间片
    task->jiffies = 0;
    task->state = TASK_READY;//就绪
    task->uid = uid;//用于区分进程的特权级
    task->gid = 0;
    task->vmap = &kernel_map;//使用内核的位图
    task->pde = 0x1000;//使用内核的页目录页表
    // task->brk = KERNEL_MEMORY_SIZE;//堆的最高地址为16M
    task->brk = USER_EXEC_ADDR;
    task->text = USER_EXEC_ADDR;
    task->data = USER_EXEC_ADDR;
    task->end = USER_EXEC_ADDR;
    task->iroot = get_root_inode();
    task->ipwd = get_root_inode();
    task->iroot->count += 2;
    task->pwd = (void *)alloc_kpage(1);
    strcpy(task->pwd, "/");
    
    task->umask = 0022; //对应0755
    
    task->files[STDIN_FILENO] = &file_table[STDIN_FILENO];
    task->files[STDOUT_FILENO] = &file_table[STDOUT_FILENO];
    task->files[STDERR_FILENO] = &file_table[STDERR_FILENO];
    task->files[STDIN_FILENO]->count++;
    task->files[STDOUT_FILENO]->count++;
    task->files[STDERR_FILENO]->count++;

    task->magic = ONIX_MAGIC;
    
    task_frame_t *frame = (task_frame_t *)stack;
    frame->ebx = 0x11111111;
    frame->esi = 0x22222222;
    frame->edi = 0x33333333;
    frame->ebp = 0x44444444;
    frame->eip = (void *)target;

    return task;
}

static void task_setup() {
    task_t *task = running_task();
    task->magic = ONIX_MAGIC;
    task->ticks = 1;//必须设置为1，这样在时钟中断的时候才能够触发schedule
    task->state = TASK_RUNNING;
    memset(task_table, 0, sizeof(task_table));
}

void task_init() {
    list_init(&block_list);
    list_init(&sleep_list);
    task_setup();
    idle_task = task_create(idle_thread, "idle_thread", 1, KERNEL_USER);
    task_create(init_thread, "init_thread", 5, NORMAL_USER);
    task_create(test_thread, "test_thread", 5, KERNEL_USER);
}

void task_to_user_mode()
{
    task_t *task = running_task();    
    //创建用户进程的虚拟内存位图
    task->vmap = kmalloc(sizeof(bitmap_t));
    void *buf = (void *)alloc_kpage(1);
    bitmap_init(task->vmap, buf, USER_MMAP_SIZE / PAGE_SIZE / 8, USER_MMAP_ADDR / PAGE_SIZE);
    //创建用户进程页表
    task->pde = (uint32)copy_pde();
    set_cr3(task->pde);//更换页目录

    uint32 addr = (uint32)task + PAGE_SIZE - sizeof(intr_frame_t);
    intr_frame_t *iframe = (intr_frame_t *)(addr);

    iframe->vector = 0x20;
    iframe->edi = 1;
    iframe->esi = 2;
    iframe->ebp = 3;
    iframe->esp_dummy = 4;
    iframe->ebx = 5;
    iframe->edx = 6;
    iframe->ecx = 7;
    iframe->eax = 8;

    iframe->gs = 0;
    iframe->ds = USER_DATA_SELECTOR;
    iframe->es = USER_DATA_SELECTOR;
    iframe->fs = USER_DATA_SELECTOR;
    iframe->ss = USER_DATA_SELECTOR;
    iframe->cs = USER_CODE_SELECTOR;

    iframe->error = ONIX_MAGIC;

    iframe->eip = 0;
    iframe->eflags = (0 << 12 | 0b10 | 1 << 9);//用户态的中断需要打开IF = 1， IOPL = 0 (IO Privilege Level)
    iframe->esp = USER_STACK_TOP;
    
    int err = sys_execve("bin/init.out", NULL, NULL);
    panic("exec /bin/init.out failure");
}

pid_t sys_getpid() {
    assert(!get_interrupt_state());//已经关闭了中断
    task_t *current = running_task();
    return current->pid;
}

pid_t sys_getppid() {
    assert(!get_interrupt_state());//已经关闭了中断
    task_t *current = running_task();
    return current->ppid;
}

fd_t task_get_fd(task_t *task) {
    fd_t i;
    for (i = 3; i < TASK_FILE_NR; ++i) {
        if (!task->files[i]) {
            break;
        }
    }
    if (i == TASK_FILE_NR) {//超过进程一次最多打开的文件数量
        panic("Exceed task max open files.");
    }
    return i;
}

void task_put_fd(task_t *task, fd_t fd) {
    assert(fd < TASK_FILE_NR);
    task->files[fd] = NULL;
}

static void task_build_stack(task_t *task) {
    uint32 addr = (uint32)task + PAGE_SIZE - sizeof(intr_frame_t);
    intr_frame_t *iframe = (intr_frame_t *)addr;
    iframe->eax = 0;//子进程退出fork中断时的返回值为0

    addr -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)addr;
    frame->ebp = 0xaa55aa55;
    frame->ebx = 0xaa55aa55;
    frame->edi = 0xaa55aa55;
    frame->esi = 0xaa55aa55;
    extern void interrupt_exit();
    frame->eip = interrupt_exit;
    task->stack = (uint32 *)frame;
}

pid_t task_fork() {
    task_t *task = running_task();

    assert(task->node.next == NULL && task->node.prev == NULL && task->state == TASK_RUNNING);

    task_t *child = get_free_task();
    pid_t pid = child->pid;//子进程ID
    memcpy(child, task, PAGE_SIZE);

    child->pid = pid;
    child->ppid = task->pid;
    child->state = TASK_READY;
    child->ticks = child->priority;

    //分配子进程的虚拟内存位图
    child->vmap = kmalloc(sizeof(bitmap_t));
    memcpy(child->vmap, task->vmap, sizeof(bitmap_t));//并将父进程的vmap拷贝一份
    void *buf = (void *)alloc_kpage(1);
    memcpy(buf, task->vmap->bits, PAGE_SIZE);
    child->vmap->bits = buf;

    //拷贝页目录
    child->pde = (uint32)copy_pde();

    //拷贝pwd
    child->pwd = (char *)alloc_kpage(1);
    strncpy(child->pwd, task->pwd, PAGE_SIZE);

    //工作目录引用加1
    task->ipwd->count++;
    task->iroot->count++;
    if (task->iexec) {
        task->iexec->count++;
    }
    //文件描述符引用计数加1
    for (size_t i = 0; i < TASK_FILE_NR; ++i) {
        file_t *file = child->files[i];
        if (file) {//这个fd指针使用了
            file->count++;
        }
    }

    task_build_stack(child);

    return child->pid;
}

void task_exit(int status) {//exit系统调用处理函数
    task_t *task = running_task();

    assert(task->node.next == NULL && task->node.prev == NULL && task->state == TASK_RUNNING);

    task->state = TASK_DEAD;
    task->status = status;

    free_pde();//释放当前进程的页目录，页表，物理页
    free_kpage((uint32)task->vmap->bits, 1);//释放虚拟位图缓冲区
    kfree(task->vmap);

    free_kpage((uint32)task->pwd, 1);
    iput(task->ipwd);
    iput(task->iroot);
    iput(task->iexec);

    //文件描述符的引用计数减1
    for (size_t i = 0; i < TASK_FILE_NR; ++i) {
        file_t *file = task->files[i];
        if (file) {
            close(i);
        }
    }

    for (size_t i = 0; i < NR_TASKS; ++i) {//将自己的子进程的父进程赋值为自己的父进程
        task_t *child = task_table[i];
        if (!child) {
            continue;
        }

        if (child->ppid != task->pid) {
            continue;
        }
        child->ppid = task->ppid;
    }
    LOGK("task %s 0x%p exit....\n", task->name, task);
    task_t *parent = task_table[task->ppid];
    if (parent->state == TASK_WAITING && (parent->waitpid == task->pid || parent->waitpid == -1)) {
        task_unblock(parent);
    }
    schedule();//调度执行别的进程
}


pid_t task_waitpid(pid_t pid, int32 *status) {//pid为-1时，等待所有子进程结束
    task_t *task = running_task();
    task_t *child = NULL;

    while (true) {
        bool has_child =false;
        for (size_t i = 2; i < NR_TASKS; ++i) {
            task_t *ptr = task_table[i];
            if (!ptr) {
                continue;
            }
            if (ptr->ppid != task->pid) {
                continue;
            }
            if (pid != ptr->pid && pid != -1) {
                continue;
            }

            if (ptr->state == TASK_DEAD) {//等待的这个子进程已经exit
                child = ptr;
                task_table[i] = NULL;
                goto rollback;
            }
            has_child = true;
            //接下来继续的循环只在pid为-1时有效果
        }
        if (has_child) {//子进程还没有exit
            task->waitpid = pid;//可能为-1，或者其中一个子进程的pid
            task_block(task, NULL, TASK_WAITING);//将自己阻塞
            continue;
        }
        break;
    }
    return -1;//没有找到符合条件的子进程

rollback:
    *status = child->status;
    uint32 ret = child->pid;
    free_kpage((uint32)child, 1);//释放PCB块
    return ret;//返回释放的子进程pid
}