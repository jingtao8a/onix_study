#ifndef _STDLIB_H_
#define _STDLIB_H_
#include "types.h"

#define MAX(a, b) (a < b ? b : a)
#define MIN(a, b) (a > b ? b : a)

void delay(uint32 count);
void hang();

uint8 bcd_to_bin(uint8 value);//BCD码->二进制
uint8 bin_to_bcd(uint8 value);//二进制->BCD码

uint32 div_round_up(uint32 num, uint32 size);

int atoi(const char *str);
#endif