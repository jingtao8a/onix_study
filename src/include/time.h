#ifndef _TIME_H_
#define _TIME_H_

#include "types.h"

typedef struct tm {
    int tm_sec;//秒
    int tm_min;//分
    int tm_hour;//时
    int tm_mday;//日
    int tm_mon;//月
    int tm_year;//年
    int tm_wday;//星期几
    int tm_yday;//1年中的第几天
    int tm_isdst;//夏令时标志
} tm;

void time_read_bcd(tm *time);//读出当前时间bcd
void time_read(tm * time);//读出当前时间

time_t mktime(tm *time);//通过time生成对应的时间戳

void time_init();
void localtime(time_t stamp, tm *time);


extern time_t startup_time;

#endif