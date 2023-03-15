#include "../include/mutex.h"
#include "../include/interrupt.h"
#include "../include/assert.h"

void mutex_init(mutex_t *mutex) {   // 初始化互斥量
    mutex->value = false;//没有被持有
    list_init(&mutex->waiters);
}
void mutex_lock(mutex_t *mutex) {   // 尝试持有互斥量
    bool intr = interrupt_disable();
    
    task_t *current = running_task();
    while (mutex->value == true) {
        task_block(current, &mutex->waiters, TASK_BLOCKED);
    }
    assert(mutex->value == false);
    mutex->value++;
    assert(mutex->value == true);

    set_interrupt_state(intr);
}

void mutex_unlock(mutex_t *mutex) { // 释放互斥量
    bool intr = interrupt_disable();

    assert(mutex->value == true);
    mutex->value--;
    assert(mutex->value == false);

    if (!list_empty(&mutex->waiters)) {
        task_t *task = element_entry(task_t, node, mutex->waiters.tail.prev);//找到队尾的task
        assert(task->magic == ONIX_MAGIC);
        task_unblock(task);
        task_yield();//确保让出自己的执行权，用于防止饥饿
    }
    set_interrupt_state(intr);
}


void reentrant_init(reentrantlock_t *lock) {//可重入锁初始化
    lock->holder = NULL;
    lock->repeat = 0;
    mutex_init(&lock->mutex);
}

void reentrant_lock(reentrantlock_t *lock) {//加锁
    task_t *current = running_task();
    if (lock->holder != current) {
        mutex_lock(&lock->mutex);
        lock->holder = current;
        assert(lock->repeat == 0);
        lock->repeat = 1;
    } else {
        lock->repeat++;
    }
}

void reentrant_unlock(reentrantlock_t *lock) {//解锁

    task_t *current = running_task();
    assert(lock->holder == current);
    lock->repeat--;
    if (lock->repeat == 0) {
        lock->holder = NULL;
        mutex_unlock(&lock->mutex);
    }
}
