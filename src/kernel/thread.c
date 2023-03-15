#include "../include/interrupt.h"
#include "../include/debug.h"
#include "../include/syscall.h"
#include "../include/mutex.h"
#include "../include/clock.h"
#include "../include/stdio.h"
#include "../include/tasks.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/osh.h"

static uint32 count = 0;

void idle_thread() {
    set_interrupt_state(true);//开中断
    while (true) {
        asm volatile(
            "hlt\n"//让cpu占停一会，等待中断唤醒cpu
        );
    }
}

void init_thread() {
    char temp[100];//必须让esp指针往低地址移动一定空间，为intr_frame_t预留出足够的空间
    dev_init();//创建设备文件
    task_to_user_mode();
}

void test_thread() {
    set_interrupt_state(true);//开中断
    while (true) {
    }
}
