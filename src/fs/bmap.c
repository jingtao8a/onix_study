#include "../include/fs.h"
#include "../include/bitmap.h"
#include "../include/assert.h"
#include "../include/buffer.h"

//分配一个文件块
uint32 balloc(int32 dev) {
    super_block_t *sb = get_super(dev);//获得该设备的超级块内存对象
    assert(sb);//确保不为null

    buffer_t *buf = NULL;
    uint32 bit = EOF;
    bitmap_t map;
    
    for (size_t i = 0; i < ZMAP_NR; ++i) {
        buf = sb->zmaps[i];
        assert(buf);
        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS + sb->desc->firstdatazone - 1);

        bit = bitmap_scan(&map, 1);
        if (bit != EOF) {
            bwrite(buf);
            break;
        }
    }
    return bit;//分配的文件块在该设备中的块号
}

//释放一个文件块
void bfree(int32 dev, uint32 idx) {//释放该设备中的第idx号文件块
    super_block_t *sb = get_super(dev);
    assert(sb);
    bitmap_t map;
    for (size_t i = 0; i < ZMAP_NR; ++i) {
        if ((i * BLOCK_BITS + sb->desc->firstdatazone - 1) <= idx && idx < ((i + 1) * BLOCK_BITS + sb->desc->firstdatazone - 1)) {
            buffer_t *buf = sb->zmaps[i];
            assert(buf);
            bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS + sb->desc->firstdatazone - 1);
            assert(bitmap_test(&map, idx));
            bitmap_set(&map, idx, false);
            
            bwrite(buf);
            break;
        }
    }
}

//分配一个 inode块
uint32 ialloc(int32 dev) {
    super_block_t *sb = get_super(dev);
    assert(sb);

    buffer_t *buf = NULL;
    uint32 bit = EOF;
    bitmap_t map;
    
    for (size_t i = 0; i < IMAP_NR; ++i) {
        buf = sb->imaps[i];
        assert(buf);
        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS);

        bit = bitmap_scan(&map, 1);
        if (bit != EOF) {
            bwrite(buf);
            break;
        }
    }
    return bit;//分配的是第几个inode
}

//释放一个inode
void ifree(int32 dev, uint32 idx) {//释放该设备中的第idx号块
    super_block_t *sb = get_super(dev);
    assert(sb);
    bitmap_t map;
    for (size_t i = 0; i < IMAP_NR; ++i) {
        if ((i * BLOCK_BITS) <= idx && idx < ((i + 1) * BLOCK_BITS)) {
            buffer_t *buf = sb->imaps[i];
            assert(buf);
            bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS);
            assert(bitmap_test(&map, idx));
            bitmap_set(&map, idx, false);

            bwrite(buf);
            break;
        }
    }
}


static uint32 reckon(inode_t *inode, uint32 block, bool create, int level) {
    uint16 *array = inode->desc->zone;
    int index;
    int divider;
    switch (level) {
        case 0:
            index = block;
            break;
        case 1:
            index = DIRECT_BLOCK;
            divider = 1;
            break;
        case 2:
            index = DIRECT_BLOCK + 1;
            divider = BLOCK_INDEXES;
            break;
    }

    buffer_t *buf = inode->buf;
    buf->count++;

    while (level >= 0) {
        if (!array[index]) {
            if (!create) {
                return 0;
            }
            array[index] = balloc(inode->dev);
            buf->dirty = true;
        }
        brelease(buf);

        if (level == 0) {
            if (inode->buf->dirty) {
                inode->buf->dirty = false;
                bwrite(inode->buf);//磁盘和内存的内容保持一致性
            }
            return array[index];
        }
        
        buf = bread(inode->dev, array[index]);
        array = (uint16 *)(buf->data);
        index = block / divider;
        block = block % divider;
        divider /= BLOCK_INDEXES;
        
        level--;
    }
}

//获取inode指向的文件的第block块的索引值，如果不存在，且creat为true，则创建
uint32 bmap(inode_t *inode, uint32 block, bool create) {
    assert(block >= 0 && block < TOTAL_BLOCK);
    int level;
    if (block < DIRECT_BLOCK) {//直接块
        level = 0;
    } else if (block < DIRECT_BLOCK + INDIRECT1_BLOCK) {//使用了一级间接块
        level = 1;
        block -= DIRECT_BLOCK;
    } else {//使用了二级间接块
        level = 2;
        block -= INDIRECT1_BLOCK;
    }
    return reckon(inode, block, create, level);
}
