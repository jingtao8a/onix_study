#include "../include/buffer.h"
#include "../include/memory.h"
#include "../include/debug.h"
#include "../include/device.h"
#include "../include/assert.h"

#define HASH_COUNT 31 //索引数 为素数 ? 有限域

static buffer_t *buffer_start = (buffer_t *)KERNEL_BUFFER_MEM;
static uint32 buffer_count = 0;

//记录当前buffer_t 结构体位置
static buffer_t *buffer_ptr = (buffer_t *)KERNEL_BUFFER_MEM;

//记录当前数据缓冲区的位置
static void *buffer_data = (void *)(KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE - BLOCK_SIZE);

static list_t free_list; //缓存链表，被释放的块
static list_t wait_list; //等待进程链表
static list_t hash_table[HASH_COUNT];//缓存哈希表

//哈希函数
static uint32 hash(int32 dev, uint32 block) {
    return (dev ^ block) % HASH_COUNT;
}

//根据设备号和块号从哈希表中获取buffer
static buffer_t *get_from_hash_table(int32 dev, uint32 block) {
    uint32 idx = hash(dev, block);
    list_t *list = &hash_table[idx];
    
    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next) {
        buffer_t *ptr = element_entry(buffer_t, hnode, node);
        if (ptr->dev == dev && ptr->block == block) {
            return ptr;
        }
    }

    return NULL;
}

//将buffer放入哈希表
static void hash_locate(buffer_t *bf) {
    uint32 idx = hash(bf->dev, bf->block);
    list_t *list = &hash_table[idx];
    assert(!list_search(list, &bf->hnode));
    list_push(list, &bf->hnode);
}

static buffer_t *get_new_buffer() {
    buffer_t *bf = NULL;
    if ((uint32)buffer_ptr + sizeof(buffer_t) < (uint32)buffer_data) {
        bf = buffer_ptr;
        bf->data = buffer_data;
        bf->dev = EOF;//设备号
        bf->block = 0;//对应设备的第几块
        bf->count = 0;
        bf->dirty = false;
        bf->valid = false;
        reentrant_init(&bf->lock);
        
        buffer_count++;
        buffer_ptr++;
        buffer_data -= BLOCK_SIZE;
        LOGK("buffer count %d\n", buffer_count);
    }
    return bf;
}

static buffer_t *get_free_buffer() {
    buffer_t *bf = NULL;
    while (true) {
        bf = get_new_buffer();//如果内存还有空间，直接开辟一个新的buffer_t
        if (bf) {
            return bf;
        }
        //内存空间不够了,那么就只能看看free_list中是否有buffer_t了
        if (!list_empty(&free_list)) {
            bf = element_entry(buffer_t, rnode, list_popback(&free_list));
            return bf;
        }
        //如果没有的话,就把自己阻塞在wait_list,等待某个缓冲释放
        task_block(running_task(), &wait_list, TASK_BLOCKED);
    }
}

//读取dev的block块
buffer_t *bread(int32 dev, uint32 block) {
    buffer_t *bf = get_from_hash_table(dev, block);//先从hash_table中找
    //hash_table中找到了buffer
    if (bf) {
        reentrant_lock(&bf->lock);
        assert(bf->valid == true);
        if (bf->count == 0) {
            reentrant_unlock(&bf->lock);
            goto getagain;
        }
        bf->count++;
        reentrant_unlock(&bf->lock);
        return bf;
    }
getagain:
    //hash_table中没有找到
    bf = get_free_buffer();//获得一个新的buffer_t
    
    reentrant_lock(&bf->lock);

    assert(bf->count == 0 && bf->dirty == false && bf->valid == false);
    bf->count = 1;//该buffer的引用计数置为1
    bf->dev = dev;//设置设备号
    bf->block = block;//设置该设备的块号
    hash_locate(bf);//将该buffer插入hash_table

    device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_READ);//对该设备请求读bf->block * BLOCK_SECS个扇区
    bf->valid = true;//将该buffer_t的valid置为true
    
    reentrant_unlock(&bf->lock);
    
    return bf;
}

//写缓冲
void bwrite(buffer_t *bf) {
    assert(bf);
    device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_WRITE);//将该buffer_t写入对应设备的对应块
}

//释放缓冲
void brelease(buffer_t *bf) {//释放某个buffer_t
    if (!bf) {
        return;
    }

    bf->count--;//将该buffer_t引用计数减1
    assert(bf->count >= 0);
    if (bf->count) {//还有进程引用,直接返回
        return;
    }
    
    list_remove(&bf->hnode);//从hash_table中删除
    
    if (bf->dirty) {//如果该buffer_t为dirty块
        bf->dirty = false;
        bwrite(bf);//写入磁盘
    }
    bf->dev = EOF;
    bf->block = 0;
    bf->valid = false;

    list_push(&free_list, &bf->rnode);//插入free_list

    if (!list_empty(&wait_list)) {//如果有task阻塞,唤醒
        task_t *task = element_entry(task_t, node, list_pop(&wait_list));
        task_unblock(task);
    }
}

void buffer_init() {
    LOGK("buffer_t size is %d\n", sizeof(buffer_t));
    //初始化空闲链表
    list_init(&free_list);
    //初始化等待进程链表
    list_init(&wait_list);

    //初始化哈希表
    for (size_t i = 0; i < HASH_COUNT; ++i) {
        list_init(&hash_table[i]);
    }
}

