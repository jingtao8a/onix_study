#include "../include/memory.h"
#include "../include/string.h"
#include "../include/assert.h"
#include "../include/debug.h"
#include "../include/device.h"
#include "../include/stdio.h"

#define SECTOR_SIZE 512

#define RAMDISK_NR 4

typedef struct ramdisk_t {
    uint8 *start;//内存开始位置
    uint32 size;//占用内存的大小
} ramdisk_t;

static ramdisk_t ramdisks[RAMDISK_NR];

static int ramdisk_ioctl(ramdisk_t *disk, int cmd, void *args, int flags) {
    switch (cmd) {
        case DEV_CMD_SECTOR_START:
            return 0;
        case DEV_CMD_SECTOR_COUNT:
            return disk->size / SECTOR_SIZE;
        default:
            panic("device command %d can't recognize !!!", cmd);
    }
}

static int ramdisk_read(ramdisk_t *disk, void *buf, uint8 count, uint32 lba)
{
    void *addr = disk->start + lba * SECTOR_SIZE;
    uint32 len = count * SECTOR_SIZE;
    assert(((uint32)addr + len) < (KERNEL_RAMDISK_MEM + KERNEL_RAMDISK_SIZE));
    memcpy(buf, addr, len);
    return count;
}

static int ramdisk_write(ramdisk_t *disk, void *buf, uint8 count, uint32 lba) {
    void *addr = disk->start + lba * SECTOR_SIZE;
    uint32 len = count * SECTOR_SIZE;
    assert(((uint32)addr + len) < (KERNEL_RAMDISK_MEM + KERNEL_RAMDISK_SIZE));
    memcpy(addr, buf, len);
    return count;
}

void ramdisk_init() {
    LOGK("ramdisk init ...");
    uint32 size = KERNEL_RAMDISK_SIZE / RAMDISK_NR;//每个虚拟磁盘的大小,这里为1M
    assert(size % SECTOR_SIZE == 0);
    
    char name[32];
    for (size_t i = 0; i < RAMDISK_NR; ++i) {
        ramdisk_t *ramdisk = &ramdisks[i];
        ramdisk->start = (uint8 *)(KERNEL_RAMDISK_MEM + size * i);
        ramdisk->size = size;
        sprintf(name, "md%c", i + 'a');
        device_install(DEV_BLOCK, DEV_RAMDISK, ramdisk, name, 0, ramdisk_ioctl, ramdisk_read, ramdisk_write);
    }

}