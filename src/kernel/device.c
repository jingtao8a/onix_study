#include "../include/device.h"
#include "../include/assert.h"
#include "../include/string.h"
#include "../include/debug.h"
#include "../include/tasks.h"
#include "../include/arena.h"

#define DEVICE_NR 64 //最多虚拟化64个设备

static device_t devices[DEVICE_NR]; // 设备数组 0号设备不用
//可用设备号1 ~ 63

//获取空设备
static device_t *get_null_device() {
    for (size_t i = 1; i < DEVICE_NR; ++i) {//可用设备号1 ~ 63
        device_t *device = &devices[i];
        if (device->type == DEV_NULL) {
            return device;
        }
    }
    panic("no more devices");
}

//安装(注册)设备
int32 device_install(int type, int subtype, void *ptr, char *name, int32 parent, 
                void *ioctl, void *read, void *write) {
    device_t *device = get_null_device();
    device->type = type;
    device->subtype = subtype;
    device->ptr = ptr;
    strncpy(device->name, name, NAMELEN);
    device->parent = parent;
    device->ioctl = ioctl;
    device->read = read;
    device->write = write;
    return device->dev;
}

device_t *device_find(int subtype, uint32 idx) {
    uint32 nr = 0;
    for (size_t i = 1; i < DEVICE_NR; ++i) {
        device_t *device = &devices[i];
        if (device->subtype != subtype)
            continue;
        if (nr == idx) {
            return device;
        }
        ++nr;
    }
    return NULL;
}

//根据设备号查找设备
device_t *device_get(int32 dev) {
    assert(dev < DEVICE_NR);
    device_t *device = &devices[dev];
    assert(device->type != DEV_NULL);
    return device;
}

//控制设备
int device_ioctl(int32 dev, int cmd, void *args, int flags) {
    device_t *device = device_get(dev);
    if (device->ioctl) {
        return device->ioctl(device->ptr, cmd, args, flags);
    }
    LOGK("ioctl of device %d is not implemented\n", dev);
    return EOF;
}

//读设备
int device_read(int32 dev, void *buf, size_t count, uint32 idx, int flags) {
    device_t *device = device_get(dev);
    if (device->read) {
        return device->read(device->ptr, buf, count, idx, flags);
    }
    LOGK("read of device %d is not implemented\n", dev);
    return EOF;
}

//写设备
int device_write(int32 dev, void *buf, size_t count, uint32 idx, int flags) {
    device_t *device = device_get(dev);
    if (device->write) {
        return device->write(device->ptr, buf, count, idx, flags);
    }
    LOGK("write of device %d is not implemented\n", dev);
    return EOF;
}


void device_init() {
    for (size_t i = 0; i < DEVICE_NR; ++i) {
        device_t *device = &devices[i];
        strcpy((char *)device->name, "null");//设备名称
        device->type = DEV_NULL;//设备类型
        device->subtype = DEV_NULL;//设备子类型
        device->dev = i;//设备号
        device->parent = 0;//父设备号
        device->ptr = NULL;
        device->ioctl = NULL;
        device->read = NULL;
        device->write = NULL;
        list_init(&device->request_list);
        device->direct = DIRECT_UP;
    }
}

//执行设备块请求
static void do_request(request_t *req) {
    switch (req->type) {
        case REQ_READ:
            device_read(req->dev, req->buf, req->count, req->idx, req->flags);
            break;
        case REQ_WRITE:
            device_write(req->dev, req->buf, req->count, req->idx, req->flags);
            break;
        default:
            panic("req type %d unknown!!!", req->type);
    }
}

//磁盘调度电梯算法
static request_t *request_nextreq(device_t *device, request_t *req) {
    list_t *list = &device->request_list;
    if (device->direct == DIRECT_UP && req->node.next == &list->tail) {
        device->direct = DIRECT_DOWN;
    } else if (device->direct == DIRECT_DOWN && req->node.prev == &list->head) {
        device->direct = DIRECT_UP;
    }
    void *next = NULL;
    if (device->direct == DIRECT_UP) {
        next = req->node.next;
    } else {
        next = req->node.prev;
    }

    if (next == &list->head || next == &list->tail) {
        return NULL;
    }
    return element_entry(request_t, node, next);
}

void device_request(int32 dev, void *buf, size_t count, uint32 idx, int flags, uint32 type) {
    device_t *device = device_get(dev);
    assert(device->type == DEV_BLOCK);//块设备
    uint32 offset = idx + device_ioctl(device->dev, DEV_CMD_SECTOR_START, 0, 0);;
    if (device->parent) {//说明这个设备是一个分区
        device = device_get(device->parent);//找到父设备
    }

    request_t *req = kmalloc(sizeof(request_t));
    req->dev = device->dev;
    req->buf = buf;
    req->count = count;
    req->idx = offset;
    req->flags = flags;
    req->type = type;
    req->task = NULL;

    bool empty = list_empty(&device->request_list);//这个时候的device一定是一个磁盘
    list_insert_sort(&device->request_list, &req->node, element_node_offset(request_t, node, idx));//按照idx的值从大到小排列

    //如果列表不为空,则阻塞，因为已经有请求在处理了，等待处理完成
    if (!empty) {
        req->task = running_task();
        task_block(req->task, NULL, TASK_BLOCKED);
    }

    do_request(req);
    
    request_t *nextreq = request_nextreq(device, req);
    list_remove(&req->node);
    kfree(req);

    if (nextreq) {
        assert(nextreq->task->magic == ONIX_MAGIC);
        task_unblock(nextreq->task);
    }
}