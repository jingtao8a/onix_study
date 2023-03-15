#ifndef _IO_H_
#define _IO_H_

#include "types.h"

extern uint8 inb(uint16 port);//输入一个字节
extern uint16 inw(uint16 port);//输入一个字

extern void outb(uint16 port, uint8 value);//输出一个字节
extern void outw(uint16 port, uint16 value);//输出一个字

#endif