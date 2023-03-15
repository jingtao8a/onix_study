#ifndef _CONSOLE_H
#define _CONSOLE_H
#include "types.h"

void console_init();//控制台初始化
void console_clear();//清屏
uint32 console_write(void *dev, char *buf, uint32 count);

#endif