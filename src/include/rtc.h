#ifndef _RTC_H_
#define _RTC_H_

#include "types.h"

uint8 cmos_read(uint8  addr);

void rtc_init();//实时中断初始化

void set_alarm(uint32 sec);//设置下次实时中断触发的时间

#endif