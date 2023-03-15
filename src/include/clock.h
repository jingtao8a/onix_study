#ifndef _CLOCK_H_
#define _CLOCK_H_

extern uint32 volatile jiffies;
extern uint32 jiffy;//一个时间片10ms

void clock_init();
void start_beep();
time_t sys_time();//time系统调用处理函数

#endif