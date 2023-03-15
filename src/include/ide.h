#ifndef _IDE_H_
#define _IDE_H_

#include "mutex.h"

#define SECTOR_SIZE 512 // 扇区大小

#define IDE_CTRL_NR 2 // 控制器数量，固定为 2
#define IDE_DISK_NR 2 // 每个控制器可挂磁盘数量，固定为 2
#define IDE_PART_NR 4 //每个磁盘的分区数量，只支持主分区，MBR分区

//分区的文件系统类型
typedef enum PART_FS
{
    PART_FS_FAT12 = 1,    // FAT12
    PART_FS_EXTENDED = 5, // 扩展分区
    PART_FS_MINIX = 0x80, // minux
    PART_FS_LINUX = 0x83, // linux
} PART_FS;

//每个分区的信息(在磁盘中一个分区表项16个字节)
typedef struct part_entry_t
{
    uint8 bootable;             // 引导标志
    uint8 start_head;           // 分区起始磁头号
    uint8 start_sector : 6;     // 分区起始扇区号
    uint16 start_cylinder : 10; // 分区起始柱面号
    uint8 system;               // 分区类型字节
    uint8 end_head;             // 分区的结束磁头号
    uint8 end_sector : 6;       // 分区结束扇区号
    uint16 end_cylinder : 10;   // 分区结束柱面号
    uint32 start;               // 分区起始物理扇区号 LBA
    uint32 count;               // 分区占用的扇区数
} _packed part_entry_t;

//主引导扇区，总共512个字节
typedef struct boot_sector_t
{
    uint8 code[446];
    part_entry_t entry[4];
    uint16 signature;
} _packed boot_sector_t;






struct ide_disk_t;
//从每个分区表项提取出有效的信息，并且，将disk指针指向对应的ide_disk_t对象
typedef struct ide_part_t
{
    char name[8];            // 分区名称
    struct ide_disk_t *disk; // 磁盘指针
    uint32 system;              // 分区类型
    uint32 start;               // 分区起始物理扇区号 LBA
    uint32 count;               // 分区占用的扇区数
} ide_part_t;



struct ide_ctrl_t;
// IDE 磁盘
typedef struct ide_disk_t
{
    char name[8];                  // 磁盘名称
    struct ide_ctrl_t *ctrl;       // 控制器指针
    uint8 selector;                   // 磁盘选择
    bool master;                   // 主盘
    uint32 total_lba; //可用扇区数量
    uint32 cylinders; //柱面数
    uint32 heads; //磁头数
    uint32 sectors; //扇区数
    ide_part_t parts[IDE_PART_NR]; //磁盘分区
} ide_disk_t;

// IDE 控制器
typedef struct ide_ctrl_t
{
    char name[8];                  // 控制器名称
    reentrantlock_t lock;                   // 控制器锁
    uint16 iobase;                    // IO 寄存器基址
    ide_disk_t disks[IDE_DISK_NR]; // 磁盘
    ide_disk_t *active;            // 当前选择的磁盘
    uint8 control;
    task_t *waiter; //等待控制器的进程
} ide_ctrl_t;

extern ide_ctrl_t controllers[IDE_CTRL_NR];

/*
1.通过disk找到对应ctrl,获得iobase等信息
2.向对应的通道驱动器发送命令字选择对应的disk
3.向对应的通道驱动器发送命令字选择应该写或者读的扇区数，以及在磁盘的开始扇区号
4.向对应的通道驱动器发送命令字（写或者读）
5.向对应的通道驱动器读数据或者写数据
*/
//读磁盘
int ide_pio_read(ide_disk_t *disk, void *buf, uint8 count, uint32 lba);
//写磁盘
int ide_pio_write(ide_disk_t *disk, void *buf, uint8 count, uint32 lba);

//读分区
int ide_pio_part_read(ide_part_t *part, void *buf, uint8 count, uint32 lba);
//写分区
int ide_pio_part_write(ide_part_t *part, void *buf, uint8 count, uint32 lba);

void ide_init();

#endif