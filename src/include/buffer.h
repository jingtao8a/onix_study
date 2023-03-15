#ifndef _BUFFER_H_
#define _BUFFER_H_

#include "list.h"
#include "mutex.h"

#define BLOCK_SIZE 1024 //块大小 1k
#define SECTOR_SIZE 512 //扇区大小 512byte
#define BLOCK_SECS (BLOCK_SIZE / SECTOR_SIZE) //一块占 2 个扇区

typedef struct buffer_t
{
    char *data;        // 数据区
    int32 dev;         // 设备号
    uint32 block;       // 对应设备的块号
    int count;         // 引用计数
    list_node_t hnode; // 哈希表拉链节点
    list_node_t rnode; // 缓冲节点
    reentrantlock_t lock;       // 锁
    bool dirty;        // 是否与磁盘不一致
    bool valid;        // 是否有效
} buffer_t;

void bwrite(buffer_t *bf);//线程不安全
buffer_t *bread(int32 dev, uint32 block);//线程安全
void brelease(buffer_t *bf);//线程安全

void buffer_init();
#endif