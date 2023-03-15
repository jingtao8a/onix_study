#include "../include/fs.h"
#include "../include/string.h"
#include "../include/buffer.h"
#include "../include/tasks.h"
#include "../include/syscall.h"
#include "../include/assert.h"
#include "../include/stat.h"
#include "../include/debug.h"
#include "../include/memory.h"

#define P_EXEC IXOTH
#define P_READ IROTH
#define P_WRITE IWOTH

bool permission(inode_t *inode, uint16 mask)//mask为想要获得的权限
{
    uint16 mode = inode->desc->mode;

    if (!inode->desc->nlinks)//当前inode的硬链接数为0
        return false;

    task_t *task = running_task();
    if (task->uid == KERNEL_USER)//如果是内核用户，直接返回true
        return true;

    if (task->uid == inode->desc->uid)//如果当前进程的就是该inode的创建用户
        mode >>= 6;
    else if (task->gid == inode->desc->gid)//如果当前进程和该inode的创建用户属于一个组
        mode >>= 3;

    if ((mode & mask & 0b111) == mask)//只验证最后三位分别是r w x权限
        return true;
    return false;
}


//*next指向name匹配entry_name后剩余的字符串(返回值为true时)
static bool match_name(const char *name, const char *entry_name, char **next) {
    char *lhs = (char *)name;
    char *rhs = (char *)entry_name;

    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS) {
        lhs++;
        rhs++;
    }
    if (*rhs) {
        return false;
    }
    if (*lhs && !IS_SEPARATOR(*lhs)) {
        return false;
    }
    if (IS_SEPARATOR(*lhs))
        lhs++;
    *next = lhs;
    return true;
}
//查找某个inode指向的目录下的子目录名，返回值为该子目录所在的buffer
static buffer_t *find_entry(inode_t **dir, const char *name, char **next, dentry_t **result) {
    //保证dir是目录
    assert(ISDIR(((*dir)->desc->mode)));
    if (match_name(name, "..", next) && (*dir)->nr == 1) {
        super_block_t *sb = get_super((*dir)->dev);
        inode_t *inode = *dir;
        (*dir) = sb->imount;
        (*dir)->count++;
        iput(inode);
    }

    //dir目录最多的子目录数量
    uint32 entries = (*dir)->desc->size / sizeof(dentry_t);
    
    uint32 block = 0;
    buffer_t *buf = NULL;
    dentry_t *entry = NULL;
    uint32 nr = EOF;
    for (uint32 i = 0; i < entries; ++i) {
        if (!buf || (uint32)entry >= (uint32)buf->data + BLOCK_SIZE) {
            brelease(buf);
            block = bmap((*dir), i / BLOCK_DENTRIES, false);//返回*dir所指向的文件第i / BLOCK_ENTRIES个块 在设备中的块号
            buf = bread((*dir)->dev, block);
        }
        entry = &((dentry_t *)buf->data)[i % BLOCK_DENTRIES];
        if (match_name(name, entry->name, next) && entry->nr) {
            *result = entry;
            return buf;
        }
    }
    brelease(buf);
    return NULL;
}

//在inode指向的目录文件中添加目录项
static buffer_t *add_entry(inode_t *dir, const char *name, dentry_t **result) {
    char *next = NULL;

    buffer_t *buf = find_entry(&dir, name, &next, result);
    if (buf) {//已经存在不需要添加
        return buf;
    }

    //name中不能有分隔符
    for (size_t i = 0; i < strlen(name); ++i) {
        assert(!IS_SEPARATOR(name[i]));
    }

    uint32 block = 0;
    dentry_t *entry = NULL;

    for (uint32 i = 0; true; ++i, ++entry) {
        if (!buf || (uint32)entry >= (uint32)buf->data + BLOCK_SIZE) {
            brelease(buf);
            block = bmap(dir, i / BLOCK_DENTRIES, true);//返回dir所指向的文件第i / BLOCK_ENTRIES个块 在设备中的块号
            assert(block);
            buf = bread(dir->dev, block);
            entry = (dentry_t *)buf->data;
        }

        if (i * sizeof(dentry_t) >= dir->desc->size) {
            dir->desc->size = (i + 1) * sizeof(dentry_t);
            dir->desc->mtime = time();
            // dir->buf->dirty = true;
            bwrite(dir->buf);
            
            entry->nr = 0;

            strncpy(entry->name, name, NAME_LEN);
            buf->dirty = true;
            
            *result = entry;
            return buf;
        }
    }
}

//获取pathname对应的父目录（base) inode
inode_t *named(char *pathname, char **next) {//next用于携带文件名字符串返回参数
    task_t *task = running_task();
    char *left = pathname;
    inode_t *inode = NULL;

    if (IS_SEPARATOR(left[0])) {//pathname的第一个字符是分隔符
        inode = task->iroot;//inode指向根目录
        left++;
        if (!*left) {//pathname就是一个单独的斜杠
            inode->count++;
            *next = left;
            return inode;
        }
    } else if (left[0]) {
        inode = task->ipwd;//inode指向进程当前目录
    }

    inode->count++;
    char *right = strrsep(left);//right指向pathname的最后一个分隔符
    if (!right || right < left) {//pathname除去第一个斜杠后只有文件名了
        *next = left;
        return inode;
    }
    //现在right已经指向了最后一个分隔符
    right++;//将right右移一位，指向文件名（如果存在的话）

    dentry_t *entry = NULL;
    buffer_t *buf = NULL;
    int32 dev = inode->dev;
    while (true) {
        buf = find_entry(&inode, left, next, &entry);
        if (!buf) {//子目录匹配失败
            iput(inode);
            return NULL;
        }
        iput(inode);//释放inode节点
        inode = iget(dev, entry->nr);//获得匹配到的子目录对应的inode
        brelease(buf);//释放buf

        if (!ISDIR(inode->desc->mode) || !permission(inode, P_EXEC)) {//如果inode指向的文件不是目录，或者该进程对该inode没有可执行权限
            iput(inode);
            return NULL;
        }

        if (right == *next) {
            return inode;
        }

        left = *next;
    }
}

//获取pathname文件对应对应的 inode
inode_t *namei(char *pathname) {
    char *next = NULL;//指向文件名
    inode_t *dir = named(pathname, &next);//dir指向父目录inode
    if (!dir) {//父目录不存在
        return NULL;
    }
    if (!(*next)) {//pathname中没有文件名
        iput(dir);
        return NULL;
    }
    char *name = next;
    dentry_t *entry = NULL;
    buffer_t *buf = find_entry(&dir, name, &next, &entry);//寻找name文件所在的目录项
    if (!buf) {//文件不存在
        iput(dir);
        return NULL;
    }
    inode_t *inode = iget(dir->dev, entry->nr);
    iput(dir);
    brelease(buf);
    return inode;
}

int sys_mkdir(char *pathname, int mode) {
    char *next = NULL;
    inode_t *dir = named(pathname, &next);
    if (!dir) {//父目录不存在
        return EOF;
    }
    if (!(*next)) {//要创建的目录名为空
        iput(dir);
        return EOF;
    }
    if (!permission(dir, P_WRITE)) {//当前进程对于dir没有写权限
        iput(dir);
        return EOF;
    }
    char *name = next;
    dentry_t *entry;
    buffer_t *bf = NULL;
    bf = find_entry(&dir, name, &next, &entry);
    if (bf) {//要创建的目录已经存在
        brelease(bf);
        iput(dir);
        return EOF;
    }

    bf = add_entry(dir, name, &entry);//添加目录
    bf->dirty = true;
    entry->nr = ialloc(dir->dev);//分配一个inode，返回值为inode的序号
    
    task_t *task = running_task();
    inode_t *inode = new_inode(dir->dev, entry->nr);//获得新添加目录对应的inode
    
    inode->desc->mode = (mode & 0777 & ~task->umask) | IFDIR;
    inode->desc->size = sizeof(dentry_t) * 2; //当前目录和父目录 两个目录的大小
    inode->desc->nlinks = 2;
    bwrite(inode->buf);

    //写入inode目录中的默认目录项
    buffer_t *zbuf = bread(inode->dev, bmap(inode, 0, true));
    zbuf->dirty = true;
    entry = (dentry_t *)zbuf->data;

    strcpy(entry->name, ".");
    entry->nr = inode->nr;

    entry++;
    strcpy(entry->name, "..");
    entry->nr = dir->nr;

    //父目录链接数加1
    dir->desc->nlinks++;
    bwrite(dir->buf);

    iput(inode);
    iput(dir);
    brelease(zbuf);
    brelease(bf);
    return 0;
}

//判断当前目录下是否有除了. ..之外的目录项
static bool is_empty(inode_t *inode) {
    assert(ISDIR(inode->desc->mode));

    int entries = inode->desc->size / sizeof(dentry_t);
    if (entries < 2 || !inode->desc->zone[0]) {
        LOGK("bad directory on dev %d\n", inode->dev);//这是一个坏目录
        return false;
    }
    buffer_t *buf = NULL;
    dentry_t *entry;
    int count = 0;
    uint32 block = 0;
    for (uint32 i = 0; i < entries; ++i, entry++) {
        if (!buf || (uint32) entry > (uint32)buf->data + BLOCK_SIZE) {
            brelease(buf);
            block = bmap(inode, i / BLOCK_DENTRIES, false);
            assert(block);
            buf = bread(inode->dev, block);
            entry = (dentry_t *)buf->data;
        }
        if (entry->nr) {
            count++;
        }
    }
    brelease(buf);

    if (count < 2) {
        LOGK("bad directory on dev %d\n", inode->dev);//这是一个坏目录
        return false;
    }
    return count == 2;
}

int sys_rmdir(char *pathname) {
    char *next = NULL;
    buffer_t *bf = NULL;
    inode_t *inode = NULL;
    inode_t *dir = named(pathname, &next);
    int ret = EOF;

    if (!dir) {//父目录不存在
        goto rollback;
    }
    if (!(*next)) {//要删除的目录名为空
        goto rollback;
    }
    if (!permission(dir, P_WRITE)) {//当前进程对于dir(父目录)没有写权限
        goto rollback;
    }

    char *name = next;
    dentry_t *entry;
    
    bf = find_entry(&dir, name, &next, &entry);
    if (!bf) {//要删除的目录不存在
        goto rollback;
    }

    inode = iget(dir->dev, entry->nr);
    entry->nr = 0;//该目录项失效
    bf->dirty = true;
    
    task_t *task = running_task();
    if (!ISDIR(inode->desc->mode) || (dir->desc->mode & ISVTX) && (task->uid != inode->desc->uid)) {//不是目录 或 受限删除
        goto rollback;
    }
    if (!is_empty(inode) || inode->count > 1) {//要删除的目录下还有其他目录项 或 当前的inode还正在被别的进程引用
        goto rollback;
    }
    assert(inode->desc->nlinks == 2);
    inode_truncate(inode);
    ifree(inode->dev, inode->nr);

    dir->desc->nlinks--;
    dir->ctime = dir->atime = dir->desc->mtime = time();
    // dir->desc->size -= sizeof(dentry_t);删除目录时，文件大小不变
    bwrite(dir->buf);

    ret = 0;
rollback:
    iput(inode);
    iput(dir);
    brelease(bf);
    return ret;
}

int sys_link(char *oldname, char *newname) {
    int ret = EOF;
    buffer_t *buf = NULL;
    inode_t *dir = NULL;
    inode_t *inode = namei(oldname);
    if (!inode) {
        goto rollback;
    }
    if (ISDIR(inode->desc->mode)) {//inode指向的是一个目录
        goto rollback;
    }

    char *next = NULL;
    dir = named(newname, &next);
    if (!dir) {//newname的父目录不存在
        goto rollback;
    }
    if (!(*next)) {//newname只有路径，没有文件名
        goto rollback;
    }
    if (!permission(dir, P_WRITE)) {//当前进程对该目录没有写权限
        goto rollback;
    }

    char *name = next;
    dentry_t *entry;
    buf = find_entry(&dir, name, &next, &entry);
    if (buf) {//该目录项已经存在
        goto rollback;
    }
    //添加目录项
    buf = add_entry(dir, name, &entry);
    entry->nr = inode->nr;
    buf->dirty = true;

    inode->desc->nlinks++;
    inode->ctime = time();
    bwrite(inode->buf);
    ret = 0;
rollback:
    brelease(buf);
    iput(inode);
    iput(dir);
    return ret;
}

int sys_unlink(char *filename) {
    int ret = EOF;
    char *next = NULL;
    inode_t *inode = NULL;
    buffer_t *buf = NULL;
    inode_t *dir = named(filename, &next);
    if (!dir) {//没有父目录
        goto rollback;
    }
    if (!(*next)) {//只有路径没有文件名
        goto rollback;
    }
    char *name = next;
    dentry_t *entry;
    buf = find_entry(&dir, name, &next, &entry);
    if (!buf) {//目录项不存在
        goto rollback;
    }
    inode = iget(dir->dev, entry->nr);
    if (ISDIR(inode->desc->mode)) {//这个文件是一个目录
        goto rollback;
    }

    task_t *task = running_task();
    if ((inode->desc->mode & ISVTX) && task->uid != inode->desc->uid) {
        goto rollback;
    }

    entry->nr = 0;
    buf->dirty = true;

    inode->desc->nlinks--;
    // inode->buf->dirty = true;
    bwrite(inode->buf);
    if (inode->desc->nlinks == 0) {
        inode_truncate(inode);
        ifree(inode->dev, inode->nr);
    }

    dir->ctime = dir->atime = dir->desc->mtime = time();
    // dir->desc->size -= sizeof(dentry_t);删除一个目录项，目录大小不变
    bwrite(dir->buf);
    ret = 0;
rollback:
    brelease(buf);
    iput(inode);
    iput(dir);
    return ret;
}

inode_t *inode_open(char *pathname, int flag, int mode) {
    inode_t *dir = NULL;//指向父目录
    inode_t *inode = NULL;
    buffer_t *buf = NULL;
    dentry_t *entry = NULL;
    char *next = NULL;//指向文件名
    dir = named(pathname, &next);
    if (!dir) {//没有父目录
        goto rollback;
    }

    if (!*next) {//没有文件名,说明打开的inode是一个目录
        return dir;
    }

    if ((flag & O_TRUNC) && ((flag & O_ACCMODE) == O_RDONLY)) {//以只读方式打开inode时，且flag包含O_TRUNC选项
        flag |= O_RDWR;//将flag改为读写选项
    }

    char *name = next;//name指向文件名
    buf = find_entry(&dir, name, &next, &entry);//在dir目录下查找该文件的目录项
    if (buf) {
        inode = iget(dir->dev, entry->nr);//获得该文件的inode
        goto makeup;
    }

    //该文件不存在时
    if (!(flag & O_CREAT) || !permission(dir, P_WRITE)) {//flag没有O_CREAT选项，并且没有对dir的写权限
        goto rollback;
    }

    buf = add_entry(dir, name, &entry);//在dir下添加目录
    buf->dirty = true;
    entry->nr = ialloc(dir->dev);//分配一个inode块
    inode = new_inode(dir->dev, entry->nr);//获取新建文件的inode

    task_t *task = running_task();
    //准备创建文件
    mode &= (0777 & ~task->umask);//创建的文件的属性
    mode |= IFREG; //文件类型

    inode->desc->mode = mode;

    bwrite(inode->buf);
makeup:
    if (!permission(inode, ACC_MODE(flag && O_ACCMODE))) {
        goto rollback;
    }
    //打开的如果是目录必须以只读方式打开
    if (ISDIR(inode->desc->mode) && ((flag & O_ACCMODE) != O_RDONLY)) {
        goto rollback;
    }
    inode->atime = time();

    if (flag & O_TRUNC) {
        inode_truncate(inode);
    }
    brelease(buf);
    iput(dir);
    return inode;
rollback:
    brelease(buf);
    iput(dir);
    iput(inode);
    return NULL;
}


//获取当前路径
char *sys_getcwd(char *buf, size_t size) {
    task_t *task = running_task();
    strncpy(buf, task->pwd, size);
    return buf;
}

// 计算 当前路径 pwd 和新路径 pathname, 存入 pwd
static void abspath(char *pwd, const char *pathname)
{
    char *cur = NULL;
    char *ptr = NULL;
    if (IS_SEPARATOR(pathname[0]))
    {
        cur = pwd + 1;
        *cur = 0;
        pathname++;
    }
    else
    {
        cur = strrsep(pwd) + 1;
        *cur = 0;
    }

    while (pathname[0])
    {
        ptr = strsep(pathname);
        if (!ptr)
        {
            break;
        }

        int len = (ptr - pathname) + 1;
        *ptr = '/';
        if (!memcmp(pathname, "./", 2))
        {
            /* code */
        }
        else if (!memcmp(pathname, "../", 3))
        {
            if (cur - 1 != pwd)
            {
                *(cur - 1) = 0;
                cur = strrsep(pwd) + 1;
                *cur = 0;
            }
        }
        else
        {
            strncpy(cur, pathname, len + 1);
            cur += len;
        }
        pathname += len;
    }

    if (!pathname[0])
        return;

    if (!strcmp(pathname, "."))
        return;

    if (strcmp(pathname, ".."))
    {
        strcpy(cur, pathname);
        cur += strlen(pathname);
        *cur = '/';
        *(cur + 1) = '\0';
        return;
    }
    if (cur - 1 != pwd)
    {
        *(cur - 1) = 0;
        cur = strrsep(pwd) + 1;
        *cur = 0;
    }
}

//切换当前目录
int sys_chdir(char *pathname) {
    task_t *task = running_task();
    inode_t *inode = namei(pathname);
    if (!inode) {//找不到inode
        goto rollback;
    }
    if (!ISDIR(inode->desc->mode) || inode == task->ipwd) {//该inode不是目录 或者 和当前目录pwd一样
        goto rollback;
    }
    abspath(task->pwd, pathname);//更新task->pwd

    iput(task->ipwd);
    task->ipwd = inode;//更新inode
    return 0;

rollback:
    iput(inode);
    return EOF;
}

//切换根目录
int sys_chroot(char *pathname) {
    task_t *task = running_task();
    inode_t *inode = namei(pathname);
    if (!inode) {//找不到inode
        goto rollback;
    }
    if (!ISDIR(inode->desc->mode) || inode == task->iroot) {//该inode不是目录 或者 和当前根目录iroot一样
        goto rollback;
    }

    iput(task->iroot);
    task->iroot = inode;
    return 0;
rollback:
    iput(inode);
    return EOF;
}

//创建设备文件
int sys_mknod(char *filename, int mode, int dev) {
    char *next = NULL;
    inode_t *dir = NULL;
    buffer_t *buf = NULL;
    inode_t *inode = NULL;
    int ret = EOF;
    
    dir = named(filename, &next);
    if (!dir) {
        goto rollback;
    }

    if (!(*next)) {
        goto rollback;
    }

    if (!permission(dir, P_WRITE)) {//当前进程对此目录没有写权限
        goto rollback;
    }

    char *name = next;
    dentry_t *entry;
    buf = find_entry(&dir, name, &next, &entry);
    if (buf) {//该目录项存在
        goto rollback;
    }
    buf = add_entry(dir, name, &entry);//添加目录
    buf->dirty = true;
    entry->nr = ialloc(dir->dev);
    inode = new_inode(dir->dev, entry->nr);

    inode->desc->mode = mode;
    if (ISBLK(mode) || ISCHR(mode)) {//字符设备或者块设备文件
        inode->desc->zone[0] = dev;
    }
    bwrite(inode->buf);

    ret = 0;
rollback:
    brelease(buf);
    iput(inode);
    iput(dir);

    return ret;
}