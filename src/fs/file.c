#include "../include/fs.h"
#include "../include/assert.h"
#include "../include/tasks.h"
#include "../include/device.h"
#include "../include/stat.h"
#include "../include/syscall.h"

#define FILE_NR 128

file_t file_table[FILE_NR];

//从文件描述符表中获得一个空闲的(引用计数为0)文件描述符表项
file_t *get_file() {
    for (size_t i = 3; i < FILE_NR; ++i) {
        file_t *file = &file_table[i];
        if (!file->count) {
            file->count++;
            return file;
        }
    }
    panic("Exceed max open files!!!");
}

//释放文件描述符表项
void put_file(file_t *file) {
    assert(file->count > 0);
    file->count--;
    if (!file->count) {//该文件描述符表项的引用计数为0
        iput(file->inode);//释放该文件描述符表项引用的inode
    }
}

void file_init() {
    for (size_t i = 3; i < FILE_NR; ++i) {
        file_t *file = &file_table[i];
        file->mode = 0;
        file->count = 0;
        file->flags = 0;
        file->offset = 0;
        file->inode = NULL;
    }
}

fd_t sys_open(char *filename, int flags, int mode) {
    inode_t *inode = inode_open(filename, flags, mode);
    if (!inode) {
        return EOF;
    }
    task_t *task = running_task();
    fd_t fd = task_get_fd(task);//获取该进程一个空闲的额文件描述符表项的指针
    file_t *file = get_file();//获取一个空闲文件描述符表项
    task->files[fd] =file;

    file->inode = inode;
    file->flags = flags;//打开时使用的flags
    file->count = 1;
    file->mode = inode->desc->mode;//文件的属性
    file->offset = 0;//文件偏移量为0 byte

    if (flags & O_APPEND) {//如果打开时使用的是追加模式
        file->offset = file->inode->desc->size;//偏移量为文件大小
    }
    return fd;
}

fd_t sys_creat(char *filename, int mode) {
    return sys_open(filename, O_CREAT | O_TRUNC, mode);
}

void sys_close(fd_t fd) {
    assert (fd < TASK_FILE_NR);
    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file) {
        return;
    }
    assert(file->inode);
    put_file(file);
    task_put_fd(task, fd);
}


//系统调用处理函数read
int sys_read(fd_t fd, char *buf, int len) {
    task_t *task = running_task();
    file_t *file = task->files[fd];
    assert(file);
    assert(len > 0);
    if ((file->flags & O_ACCMODE) == O_WRONLY) {//该文件是以只写方式打开的
        return EOF;
    }
    int ret = 0;
    inode_t *inode = file->inode;
    if (inode->pipe) {
        ret = pipe_read(inode, buf, len);
        return ret;
    } else if (ISCHR(inode->desc->mode)) {
        assert(inode->desc->zone[0]);
        ret = device_read(inode->desc->zone[0], buf, len, 0, 0);
        return ret;
    } else if (ISBLK(inode->desc->mode)) {
        assert(inode->desc->zone[0]);
        device_t *device = device_get(inode->desc->zone[0]);
        assert(file->offset % BLOCK_SIZE == 0);
        assert(len % BLOCK_SIZE == 0);
        ret = device_read(inode->desc->zone[0], buf, len / BLOCK_SIZE, file->offset / BLOCK_SIZE, 0);
        return ret;//bug??????????????????
    }
    ret = inode_read(inode, buf, len, file->offset);
    if (ret != EOF) {
        file->offset += ret;
    }
    return ret;
}

//系统调用处理函数write
int sys_write(fd_t fd, char *buf, int len) {
    task_t *task = running_task();
    file_t *file = task->files[fd];
    assert(file);
    assert(len > 0);
    if ((file->flags & O_ACCMODE) == O_RDONLY) {//该文件是以只读方式打开的
        return EOF;
    }
    int ret = 0;
    inode_t *inode = file->inode;
    if (inode->pipe) {
        ret = pipe_write(inode, buf, len);
        return ret;
    } else if (ISCHR(inode->desc->mode)) {
        assert(inode->desc->zone[0]);
        ret = device_write(inode->desc->zone[0], buf, len, 0, 0);
        return ret;
    } else if (ISBLK(inode->desc->mode)) {
        assert(inode->desc->zone[0]);
        device_t *device = device_get(inode->desc->zone[0]);
        assert(file->offset % BLOCK_SIZE == 0);
        assert(len % BLOCK_SIZE == 0);
        ret = device_write(inode->desc->zone[0], buf, len / BLOCK_SIZE, file->offset / BLOCK_SIZE, 0);
        return ret;
    }

    ret = inode_write(inode, buf, len, file->offset);
    if (ret != EOF) {
        file->offset += ret;
    }
    return ret;
}

int sys_lseek(fd_t fd, off_t offset, int whence) {
    assert(fd < TASK_FILE_NR);

    task_t *task = running_task();
    file_t *file = task->files[fd];

    assert(file);
    assert(file->inode);

    switch (whence) {
        case SEEK_SET:
            assert(offset >= 0);
            file->offset = offset;
            break;
        case SEEK_CUR:
            assert(file->offset + offset >= 0);
            file->offset += offset;
            break;
        case SEEK_END:
            assert(file->inode->desc->size + offset >= 0);
            file->offset = file->inode->desc->size + offset;
            break;
        default:
            panic("whence not defined !!!");
    }
    return file->offset;
}

int sys_readdir(fd_t fd, dentry_t *dir, uint32 count) {
    return sys_read(fd, (char *)dir, sizeof(dentry_t));
}

static fd_t dupfd(fd_t fd, fd_t arg) {
    task_t *task = running_task();
    if (fd >= TASK_FILE_NR || !task->files[fd]) {
        return EOF;
    }
    while (task->files[arg] && arg < TASK_FILE_NR) {
        arg++;
    }
    if (arg >= TASK_FILE_NR) {
        return EOF;
    }
    task->files[arg] = task->files[fd];
    task->files[arg]->count++;
    return arg;
}

fd_t sys_dup(fd_t oldfd) {
    return dupfd(oldfd, 0);
}

fd_t sys_dup2(fd_t oldfd, fd_t newfd) {
    close(newfd);
    return dupfd(oldfd, newfd);
}