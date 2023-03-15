#include "../include/device.h"
#include "../include/syscall.h"
#include "../include/stat.h"
#include "../include/stdio.h"
#include "../include/fs.h"
#include "../include/assert.h"

extern file_t file_table[];

void dev_init() {
    mkdir("/dev", 0755);//创建目录dev

    device_t *device = NULL;
    //格式化后三个内存虚拟磁盘
    for (int i = 1; i < 4; ++i) {
        device = device_find(DEV_RAMDISK, i);
        assert(device);
        devmkfs(device->dev, 0);
    }

    //第一个虚拟磁盘文件系统 挂载载 /dev目录上 start
    device = device_find(DEV_RAMDISK, 0);
    assert(device);
    devmkfs(device->dev, 0);//格式化文件系统

    super_block_t *sb = read_super(device->dev);
    sb->iroot = iget(device->dev, 1);
    sb->imount = namei("/dev");
    sb->imount->mount = device->dev;

    //第一个虚拟磁盘文件系统 挂载载 /dev目录上 end

    device = device_find(DEV_CONSOLE, 0);
    mknod("/dev/console", IFCHR | 0200, device->dev);//第二个参数为创建文件的mode

    device = device_find(DEV_KEYBOARD, 0);
    mknod("/dev/keyboard", IFCHR | 0400, device->dev);

    char name[32];
    size_t i = 0;
    while (true) {
        device = device_find(DEV_IDE_DISK, i);
        if (!device) {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
        ++i;
    }

    i = 0;
    while (true) {
        device = device_find(DEV_IDE_PART, i);
        if (!device) {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
        ++i;
    }

    i = 0;
    while (true) {
        device = device_find(DEV_SERIAL, i);
        if (!device) {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFCHR | 0600, device->dev);
        ++i;
    }
    //防止误操作，去掉mda虚拟磁盘文件，保护dev文件系统
    i = 1;
    while (true) {
        device = device_find(DEV_RAMDISK, i);
        if (!device) {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
        ++i;
    }



    //建立字符设备文件的硬连接
    link("/dev/console", "/dev/stdout");
    link("/dev/console", "/dev/stderr");
    link("/dev/keyboard", "/dev/stdin");
    
    file_t *file;
    inode_t *inode;
    file = &file_table[STDIN_FILENO];
    inode = namei("/dev/stdin");
    file->inode = inode;
    file->mode = inode->desc->mode;
    file->flags = O_RDONLY;
    file->offset = 0;

    file = &file_table[STDOUT_FILENO];
    inode = namei("/dev/stdout");
    file->inode = inode;
    file->mode = inode->desc->mode;
    file->flags = O_WRONLY;
    file->offset = 0;

    file = &file_table[STDERR_FILENO];
    inode = namei("/dev/stderr");
    file->inode = inode;
    file->mode = inode->desc->mode;
    file->flags = O_WRONLY;
    file->offset = 0;
}