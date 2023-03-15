#include "../include/fs.h"
#include "../include/tasks.h"
#include "../include/assert.h"

static void copy_stat(inode_t *inode, stat_t *statbuf) {
    statbuf->dev = inode->dev;
    statbuf->nr = inode->nr;
    statbuf->mode = inode->desc->mode;
    statbuf->nlinks = inode->desc->nlinks;
    statbuf->uid = inode->desc->uid;
    statbuf->gid = inode->desc->gid;
    statbuf->rdev = inode->desc->zone[0];
    statbuf->size = inode->desc->size;
    statbuf->atime = inode->atime;
    statbuf->mtime = inode->desc->mtime;
    statbuf->ctime = inode->ctime;
}


int sys_stat(char *filename, stat_t *statbuf) {
    inode_t *inode = namei(filename);
    if (!inode) {
        return EOF;
    }
    copy_stat(inode, statbuf);
    iput(inode);
    return 0;
}

int sys_fstat(fd_t fd, stat_t *statbuf) {
    if (fd > TASK_FILE_NR) {
        return EOF;
    }
    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file) {
        return EOF;
    }
    inode_t *inode = file->inode;
    assert(inode);
    copy_stat(inode, statbuf);
    return 0;
}