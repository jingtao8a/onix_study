#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "list.h"

#define NAMELEN 16

//设备类型
enum device_type_t {
    DEV_NULL = 0, // 空设备
    DEV_CHAR = 1, //字符设备
    DEV_BLOCK = 2, //块设备
};
//设备子类型
enum device_subtype_t {
    DEV_CONSOLE = 1, //控制台
    DEV_KEYBOARD, //键盘
    DEV_SERIAL,//串口
    DEV_IDE_DISK, //IDE磁盘
    DEV_IDE_PART, //IDE分区
    DEV_RAMDISK, //虚拟磁盘
};

//设备控制命令
enum device_cmd_t {
    DEV_CMD_SECTOR_START = 1,//获得设备扇区的开始位置 lba
    DEV_CMD_SECTOR_COUNT = 2,//获得设备扇区数量
};

//块设备读写请求
#define REQ_READ 0 //块设备读
#define REQ_WRITE 1 //块设备写

#define DIRECT_UP 0 //上楼
#define DIRECT_DOWN 1 //下楼

//块设备请求消息结构
typedef struct request_t {
    int32 dev; //设备号
    uint32 type;//请求类型
    uint32 idx; //扇区位置
    uint32 count; //扇区数量s
    int flags; //特殊标记
    uint8 *buf;//缓冲区
    struct task_t *task;//请求的进程
    list_node_t node;//列表结点
} request_t;

typedef struct device_t {
    char name[NAMELEN];//设备名称
    int type;//设备类型
    int subtype;//设备子类型
    int32 dev;//设备号
    int32 parent;//父设备号
    void *ptr;//设备数据结构指针
    list_t request_list;//每个设备有一个请求链表挂着不同进程的请求消息
    bool direct; //磁盘寻道方向
    //设备控制
    int (*ioctl)(void *dev, int cmd, void *args, int flags);
    //读设备
    int (*read)(void *dev, void *buf, size_t count, uint32 idx, int flags);
    //写设备
    int (*write)(void *dev, void *buf, size_t count, uint32 idx, int flags);
} device_t;

//安装设备
int32 device_install(int type, int subtype, void *ptr, char *name, int32 parent, 
                void *ioctl, void *read, void *write);

//根据设备号查找设备
device_t *device_get(int32 dev);

//根据设备子类型和idx查找设备
device_t *device_find(int subtype, uint32 idx);

//控制设备
int device_ioctl(int32 dev, int cmd, void *args, int flags);

//读设备
int device_read(int32 dev, void *buf, size_t count, uint32 idx, int flags);

//写设备
int device_write(int32 dev, void *buf, size_t count, uint32 idx, int flags);

//块设备请求
void device_request(int32 dev, void *buf, size_t count, uint32 idx, int flags, uint32 type);
#endif