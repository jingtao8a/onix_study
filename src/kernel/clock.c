#include "../include/assert.h"
#include "../include/types.h"
#include "../include/debug.h"
#include "../include/io.h"
#include "../include/interrupt.h"
#include "../include/tasks.h"
#include "../include/time.h"

#define PIT_CHAN0_REG 0x40
#define PIT_CHAN1_REG 0x41
#define PIT_CHAN2_REG 0x42
#define PIT_CTRL_REG 0x43

#define kHZ 100
#define OSCILLATOR 1193182
#define CLOCK_COUNTER (OSCILLATOR / kHZ)
#define JIFFY (1000 / kHZ)


#define SPEAKER_REG 0X61
#define BEEP_KHZ 440
#define BEEP_COUNTER (OSCILLATOR / BEEP_KHZ)
//时间片计数器
uint32 volatile jiffies = 0;
uint32 jiffy = JIFFY;//一个时间片10ms

static uint32 volatile beeping = 0;//初始为0，开始beep后，被设置为应该结束的那个时间片

void start_beep() {
    if (!beeping) {
        outb(SPEAKER_REG, inb(SPEAKER_REG) | 0b11);
    }
    beeping = jiffies + 5;//表示将再经过5个时间片，就关掉蜂鸣器（前提是时钟中断打开了)
}

static void stop_beep() {
    if (beeping && jiffies > beeping) {
        outb(SPEAKER_REG, inb(SPEAKER_REG) & 0Xfc);
        beeping = 0;
    }
}

static void clock_handler(int vector) {
    assert(vector == 0x20);
    send_eoi(vector);
    stop_beep();//如果蜂鸣器的截至时间到了，就关闭它
    task_weakup();//唤醒睡眠的任务
    jiffies++;
    //DEBUGK("clock jiffies %d ...\n", jiffies);
    task_t *task = running_task();
    assert(task->magic == ONIX_MAGIC);//确保PCB的信息没有被破坏
    task->jiffies = jiffies;
    task->ticks--;
    if (task->ticks == 0) {//时间片已经耗尽
        schedule();
    }
}

time_t sys_time() {
    return startup_time + (jiffies * JIFFY) / 1000;
}

//8253芯片初始化
static void pit_init() {
    //配置计数器0 时钟
    outb(PIT_CTRL_REG, 0b00110100);//选择通道0，先读/写计数器低字节，后读/写计数器高字节,方式2:比率发生器(分频)
    outb(PIT_CHAN0_REG, CLOCK_COUNTER & 0xff);
    outb(PIT_CHAN0_REG, (CLOCK_COUNTER >> 8) & 0xff);

    //配置计数器2 蜂鸣器
    outb(PIT_CTRL_REG, 0b10110110);
    outb(PIT_CHAN2_REG, BEEP_COUNTER & 0Xff);
    outb(PIT_CHAN2_REG, (BEEP_COUNTER >> 8) & 0xff);
}

void clock_init() {
    pit_init();
    set_interrupt_handler(IRQ_CLOCK, clock_handler);
    set_interrupt_mask(IRQ_CLOCK, true);
}