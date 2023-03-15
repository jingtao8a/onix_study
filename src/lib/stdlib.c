#include "../include/stdlib.h"

void delay(uint32 count) {
    while (count--);
}

void hang() {
    while (true) {
        //do nothing
    }
}

uint8 bcd_to_bin(uint8 value) {//BCD码->二进制
    return ((value >> 4) & 0xf) * 10 + value & 0xf;
}

uint8 bin_to_bcd(uint8 value) {//二进制->BCD码
    return ((value / 10) << 4) | (value % 10); 
}

//计算num被分成size的数量 （向上取整）
uint32 div_round_up(uint32 num, uint32 size) {
    return (num + size - 1) / size;
}

int atoi(const char *str) {
    if (str == NULL) {
        return 0;
    }
    bool sign = 1;
    int result = 0;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    for (; *str; str++) {
        result = result * 10 + (*str - '0');
    }
    return result * sign;
}
