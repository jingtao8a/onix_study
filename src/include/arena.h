#ifndef _ARENA_H_
#define _ARENA_H_

#include "list.h"

#define DESC_COUNT 7

//内存描述符
typedef struct arena_descriptor_t {
    uint32 total_block;//一页内存分成了多少块
    uint32 block_size;//块大小
    list_t free_list;//空闲列表
} arena_descriptor_t;

//一页或者多页
typedef struct arena_t {
    arena_descriptor_t *desc;// 该arena的描述符
    uint32 count;// 当large = flase时，表示该页的剩余块数;当large = true时,表示该arena管理的页数
    uint32 large;// (是不是超过了大小)为true时，desc字段为NULL
    uint32 magic;// 魔术
} arena_t;

void *kmalloc(size_t size);
void kfree(void *ptr);
void arena_init();

#endif