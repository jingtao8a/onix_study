#ifndef _BITMAP_H_
#define _BITMAP_H_

#include "types.h"

typedef struct bitmap_t
{
    uint8 *bits;//位图缓冲区
    uint32 length;//位图缓冲区长度
    uint32 offset; //位图开始的偏移
} bitmap_t;

//构造位图，但是不清空bits指向的内容
void bitmap_make(bitmap_t *map, char *bits, uint32 length, uint32 offset);

//初始化位图
void bitmap_init(bitmap_t *map, char *bits, uint32 length, uint32 offset);

//测试位图的某一位是否为1
bool bitmap_test(bitmap_t *map, uint32 index);

//设置位图中某位的值
void bitmap_set(bitmap_t *map, uint32 index, bool value);

//从位图中得到连续的count位
int bitmap_scan(bitmap_t *map, uint32 count);

#endif