#include "../include/fs.h"
#include "../include/debug.h"
#include "../include/syscall.h"
#include "../include/buffer.h"
#include "../include/assert.h"
#include "../include/stat.h"
#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/mutex.h"
#include "../include/memory.h"
#include "../include/fifo.h"
#include "../include/arena.h"

#define INODE_NR 64

static inode_t inode_table[INODE_NR];

//找到inode_table中没有使用的空inode
static inode_t *get_free_inode() {
    for (size_t i = 0; i < INODE_NR; ++i) {
        inode_t *inode = &inode_table[i];
        if (inode->dev == EOF) {
            return inode;
        }
    }
    panic("no more inode!!!");
}
//将某个inode重新置为空
static void put_free_inode(inode_t *inode) {
    assert(inode != inode_table);//不是第一个,根inode不可以释放
    assert(inode->count == 0);//该inode的引用计数为0
    inode->dev = EOF;
}

//获得inode_table的第一个inode
inode_t *get_root_inode() {
    return inode_table;
}

inode_t *get_pipe_inode() {
    inode_t *inode = get_free_inode();
    //区别于EOF，这里是无效设备，但是被占用了
    inode->dev = -2;
    //申请内存，表示缓冲队列
    inode->desc = (inode_desc_t *)kmalloc(sizeof(fifo_t));
    //管道缓冲区一页内存
    inode->buf = (void *)alloc_kpage(1);
    //两个文件
    inode->count = 2;
    //管道标志
    inode->pipe = true;
    //初始化输入输出设备
    fifo_init((fifo_t *)inode->desc, (char *)inode->buf, PAGE_SIZE);
    return inode;
}

static void put_pipe_inode(inode_t *inode) {
    if (!inode) {
        return;
    }
    inode->count--;
    if (inode->count) {
        return;
    }
    inode->pipe = false;
    //释放描述符 fifo
    kfree(inode->desc);
    //释放缓冲区
    free_kpage((uint32)inode->buf, 1);
    //释放inode
    put_free_inode(inode);
}

//计算第nr块inode在设备中的块号
static inline uint32 inode_block(super_block_t *sb, uint32 nr) {
    return 2 + sb->desc->imap_blocks + sb->desc->zmap_blocks + (nr - 1) / BLOCK_INODES;
}

//从inode_list中查找编号为 nr 的inode
static inode_t *find_inode(super_block_t *sb, uint32 nr) {
    list_t *list = &sb->inode_list;
    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next) {
        inode_t *inode = element_entry(inode_t, node, node);
        if (inode->nr == nr) {
            return inode;
        }
    }
    return NULL;
}

static inode_t *fit_inode(inode_t *inode) {
    if (!inode->mount) {
        return inode;
    }
    super_block_t *sb = get_super(inode->mount);
    assert(sb);
    iput(inode);
    inode = sb->iroot;
    inode->count++;
    return inode;
}

inode_t *iget(int32 dev, uint32 nr) {
    //将该inode链接进super_block_t的inode_lis中
    super_block_t *sb = get_super(dev);
    assert(sb);

    inode_t *inode = find_inode(sb, nr);
    //找到了这个inode
    if (inode) {
        reentrant_lock(&inode->lock);
        
        inode->count++;//计数加1
        inode->atime = time();//访问时间更新
        reentrant_unlock(&inode->lock);
        return fit_inode(inode);
    }
    //获得一个空闲的inode
    inode = get_free_inode();

    reentrant_lock(&inode->lock);
    
    inode->dev = dev;
    inode->nr = nr;
    inode->count++;
    inode->atime = time();
    
    list_push(&sb->inode_list, &inode->node);//放进inode_lis中

    uint32 block = inode_block(sb, inode->nr);//获得该inode在设备中的块号
    
    buffer_t *buf = bread(inode->dev, block);//读取该块到buffer
    inode->buf = buf;
    inode->desc = &((inode_desc_t *)buf->data)[((inode->nr - 1) % BLOCK_INODES)];
    inode->ctime = inode->desc->mtime;

    reentrant_unlock(&inode->lock);
    return inode;
}

void iput(inode_t *inode) {
    if (!inode) {
        return;
    }
    if (inode->pipe) {
        return put_pipe_inode(inode);
    }
    inode->count--;
    if (inode->count) {
        return;
    }

    //从超级块链表inode_list中移除
    list_remove(&inode->node);

    //释放该inode所在的缓冲区
    brelease(inode->buf);

    //放在空闲的位置
    put_free_inode(inode);
}


void inode_init() {
    for (size_t i = 0; i < INODE_NR; ++i) {
        inode_t *inode = &inode_table[i];
        inode->dev = EOF;
        reentrant_init(&inode->lock);
        inode->rxwaiter = NULL;
        inode->txwaiter = NULL;
        inode->pipe = false;
    }
}

//从inode的offset处，读len个字节到buf
int inode_read(inode_t *inode, char *buf, uint32 len, off_t offset) {
    assert(ISFILE(inode->desc->mode) || ISDIR(inode->desc->mode));
    if (offset >= inode->desc->size) {//超过inode指向的数据大小
        return EOF;
    }
    //开始读的位置
    uint32 begin = offset;
    
    //需要读的字节数
    uint32 left = MIN(len, inode->desc->size - offset);

    while (left) {
        uint32 idx = bmap(inode, offset / BLOCK_SIZE, false);
        assert(idx);
        buffer_t *bf = bread(inode->dev, idx);

        //文件块中的偏移量
        uint32 start = offset % BLOCK_SIZE;

        //文件块中的指针
        char *ptr = bf->data + start;

        //本次需要读取的字节数
        uint32 chars = MIN(BLOCK_SIZE - start, left);
        
        //拷贝内容
        memcpy(buf, ptr, chars);
        brelease(bf);

        left -= chars;
        offset += chars;
        buf += chars;
    }

    inode->atime = time();
    return offset - begin;
}

//从inode的offset处，将buf个字节写入磁盘
int inode_write(inode_t *inode, char *buf, uint32 len, off_t offset) {
    assert(ISFILE(inode->desc->mode));//不允许写入目录
    
    //开始写的位置
    uint32 begin = offset;
    
    //需要写的字节数
    uint32 left = len;

    while (left) {
        uint32 idx = bmap(inode, offset / BLOCK_SIZE, true);//如果需要可以新增文件块
        assert(idx);
        buffer_t *bf = bread(inode->dev, idx);

        //文件块中的偏移量
        uint32 start = offset % BLOCK_SIZE;

        //文件块中的指针
        char *ptr = bf->data + start;

        //本次需要读取的字节数
        uint32 chars = MIN(BLOCK_SIZE - start, left);
        
        //拷贝内容
        memcpy(ptr, buf, chars);
        bf->dirty = true;
        brelease(bf);

        left -= chars;
        offset += chars;
        buf += chars;
    }
    
    inode->desc->size = MAX(offset, inode->desc->size);
    inode->desc->mtime = inode->atime = time();
    bwrite(inode->buf);
    
    return offset - begin;
}


static void inode_bfree(inode_t *inode, uint16 *array, int index, int level) {
    if (!array[index]) {
        return;
    }
    if (level == 0) {
        bfree(inode->dev, array[index]);
        return;
    }

    buffer_t *buf = bread(inode->dev, array[index]);
    for (size_t i = 0; i < BLOCK_INDEXES; ++i) {
        inode_bfree(inode, (uint16 *)buf->data, i, level - 1);
    }
    brelease(buf);
    bfree(inode->dev, array[index]);
}

void inode_truncate(inode_t *inode) {
    if (!ISFILE(inode->desc->mode) || !ISDIR(inode->desc->mode)) {
        return;
    }
    for (size_t i = 0; i < DIRECT_BLOCK; ++i) {
        inode_bfree(inode, inode->desc->zone, i, 0);
        inode->desc->zone[i] = 0;
    }
    //释放一级间接块
    inode_bfree(inode, inode->desc->zone, DIRECT_BLOCK, 1);
    inode->desc->zone[DIRECT_BLOCK] = 0;
    //释放二级间接块
    inode_bfree(inode, inode->desc->zone, DIRECT_BLOCK + 1, 2);
    inode->desc->zone[DIRECT_BLOCK + 1] = 0;
    
    inode->desc->size = 0;
    inode->desc->mtime = inode->atime = time();
    bwrite(inode->buf);
}

inode_t *new_inode(int32 dev, uint32 nr) {
    task_t *task = running_task();
    inode_t *inode = iget(dev, nr);

    inode->buf->dirty = true;
    inode->desc->mode = 0777 & (~task->umask);
    inode->desc->uid = task->uid;
    inode->desc->size = 0;
    inode->desc->mtime = inode->atime = time();
    inode->desc->gid = task->gid;
    inode->desc->nlinks = 1;
    
    return inode;
}

