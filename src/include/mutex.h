#ifndef _MUTEX_H_
#define _MUTEX_H_

#include "list.h"
#include "tasks.h"

typedef struct mutex_t
{
    bool value;     // 信号量
    list_t waiters; // 等待队列
} mutex_t;


void mutex_init(mutex_t *mutex);   // 初始化互斥量
void mutex_lock(mutex_t *mutex);   // 尝试持有互斥量
void mutex_unlock(mutex_t *mutex); // 释放互斥量

typedef struct reentrantlock_t {
    task_t *holder;//持有者
    mutex_t mutex;//互斥量
    uint32 repeat;//重入次数
} reentrantlock_t;//可重入锁，一般用在递归的情况


void reentrant_init(reentrantlock_t *lock);//可重入锁初始化
void reentrant_lock(reentrantlock_t *lock);//加锁
void reentrant_unlock(reentrantlock_t *lock);//解锁

#endif