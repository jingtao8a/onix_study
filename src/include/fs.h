#ifndef _FS_H_
#define _FS_H_
#include "list.h"
#include "mutex.h"
#include "stat.h"

#define BLOCK_SIZE 1024 // 块大小
#define SECTOR_SIZE 512 // 扇区大小

#define MINIX1_MAGIC 0x137F // 文件系统魔数
#define NAME_LEN 14         // 文件名长度

#define IMAP_NR 8 // inode 位图块，最大值
#define ZMAP_NR 8 // 块位图块，最大值

#define BLOCK_BITS (BLOCK_SIZE * 8)  //块位图大小
#define BLOCK_INODES (BLOCK_SIZE / sizeof(inode_desc_t)) //块 inode 数量
#define BLOCK_DENTRIES (BLOCK_SIZE / sizeof(dentry_t)) // 块 dentry 数量
#define BLOCK_INDEXES (BLOCK_SIZE / sizeof(uint16)) //块索引数量

#define DIRECT_BLOCK (7)  //直接块数量
#define INDIRECT1_BLOCK BLOCK_INDEXES //一级间接块数量
#define INDIRECT2_BLOCK (INDIRECT1_BLOCK * INDIRECT1_BLOCK) //二级间接块数量
#define TOTAL_BLOCK (DIRECT_BLOCK + INDIRECT1_BLOCK + INDIRECT2_BLOCK) //全部块数量，也是一个inode指向的文件的可以存放的最大的块的数量

#define ACC_MODE(x) ("\004\002\006\377"[(x) & O_ACCMODE])

//打开inode时的flag
enum file_flag
{
    O_RDONLY = 00,      // 只读方式
    O_WRONLY = 01,      // 只写方式
    O_RDWR = 02,        // 读写方式
    O_ACCMODE = 03,     // 文件访问模式屏蔽码
    O_CREAT = 00100,    // 如果文件不存在就创建
    O_EXCL = 00200,     // 独占使用文件标志
    O_NOCTTY = 00400,   // 不分配控制终端
    O_TRUNC = 01000,    // 若文件已存在且是写操作，则长度截为 0
    O_APPEND = 02000,   // 以添加方式打开，文件指针置为文件尾
    O_NONBLOCK = 04000, // 非阻塞方式打开和操作文件
};

//32byte 磁盘中的存储格式
typedef struct inode_desc_t
{
    uint16 mode;    // 文件类型和属性(rwx 位)
    uint16 uid;     // 用户id（文件拥有者标识符）
    uint32 size;    // 文件大小（字节数）
    uint32 mtime;   // 修改时间戳 这个时间戳应该用 UTC 时间，不然有瑕疵
    uint8 gid;      // 组id(文件拥有者所在的组)
    uint8 nlinks;   // 链接数（多少个文件目录项指向该i 节点）
    uint16 zone[9]; // 直接 (0-6)、间接(7)或双重间接 (8) 逻辑块号
} inode_desc_t;

struct buffer_t;
struct task_t;

typedef struct inode_t
{
    inode_desc_t *desc;   // inode 描述符
    struct buffer_t *buf; // inode 描述符对应 buffer
    int32 dev;            // 设备号
    uint32 nr;             // i 节点号
    uint32 count;            // 引用计数
    time_t atime;         // 访问时间
    time_t ctime;         // 修改时间
    struct reentrantlock_t lock;       // 锁
    list_node_t node;     // 链表结点
    int32 mount;          // 安装设备
    struct task_t *rxwaiter;//读等待进程
    struct task_t *txwaiter;//写等待进程
    bool pipe;//管道标志
} inode_t;

//18byte 磁盘中的存储格式
typedef struct super_desc_t
{
    uint16 inodes;        // 节点数
    uint16 zones;         // 逻辑块数
    uint16 imap_blocks;   // i node位图所占用的数据块数
    uint16 zmap_blocks;   // 逻辑块位图所占用的数据块数
    uint16 firstdatazone; // 第一个数据逻辑块号
    uint16 log_zone_size; // log2(每逻辑块数据块数)
    uint32 max_size;      // 文件最大长度
    uint16 magic;         // 文件系统魔数
} super_desc_t;


typedef struct super_block_t
{
    super_desc_t *desc;              // 超级块描述符
    struct buffer_t *buf;            // 超级块描述符 buffer
    struct buffer_t *imaps[IMAP_NR]; // inode 位图缓冲
    struct buffer_t *zmaps[ZMAP_NR]; // 块位图缓冲
    int32 dev;                       // 设备号
    uint32 count;                    // 引用计数
    list_t inode_list;               // 使用中 inode 链表
    inode_t *iroot;                  // 文件系统根目录 inode
    inode_t *imount;                 // 安装到的 inode
} super_block_t;


// 文件目录项结构 磁盘中的存储格式
typedef struct dentry_t
{
    uint16 nr;              // i 节点
    char name[NAME_LEN]; // 文件名
} dentry_t;


//文件描述符表项结构
typedef struct file_t
{
    inode_t *inode; // 文件 inode
    uint32 count;      // 引用计数(多少个进程使用了这个文件描述符)
    off_t offset;   // 文件偏移
    int flags;      // 文件标记
    int mode;       // 文件模式
} file_t;

typedef enum whence_t {
    SEEK_SET = 1,//直接设置偏移
    SEEK_CUR,//当前位置偏移
    SEEK_END,//结束位置偏移
}whence_t;

/**************/
/*super.c*/
/**************/
void super_init();
super_block_t *get_super(int32 dev);
super_block_t *read_super(int32 dev); // 读取 dev 对应的超级块
//挂载设备
int sys_mount(char *devname, char *dirname, int flags);
//卸载设备
int sys_umount(char *target);
//格式化文件系统 系统调用
int sys_mkfs(char *devname, int icount);
//格式化文件系统 非系统调用
int devmkfs(int32 dev, uint32 icount);

/**************/
/*bmap.c*/
/**************/
uint32 balloc(int32 dev);          // 分配一个文件块
void bfree(int32 dev, uint32 idx); // 释放一个文件块
uint32 ialloc(int32 dev);          // 分配一个文件系统 inode块
void ifree(int32 dev, uint32 idx); // 释放一个文件系统 inode块
uint32 bmap(inode_t *inode, uint32 block, bool create);//获取inode第block块的索引值，如果不存在，且creat为true，则创建



/**************/
/*inode.c*/
/**************/
void inode_init();
inode_t *get_root_inode(); //获取根目录的inode
inode_t *iget(int32 dev, uint32 nr);//获得设备dev的nr inode（线程安全）
void iput(inode_t *inode); //释放inode
//从inode的offset处，读len个字节到buf
int inode_read(inode_t *inode, char *buf, uint32 len, off_t offset);
//从inode的offset处，将buf个字节写入磁盘
int inode_write(inode_t *inode, char *buf, uint32 len, off_t offset);
//释放inode所有文件块
void inode_truncate(inode_t *inode);
//创建新的inode
inode_t *new_inode(int32 dev, uint32 nr);

inode_t *get_pipe_inode();

/**************/
/*namei.c*/
/**************/
//获取pathname对应的父目录 inode
inode_t *named(char *pathname, char **next);
//获取pathname对应的 inode
inode_t *namei(char *pathname);
//打开inode
inode_t *inode_open(char *pathname, int flag, int mode);

//系统调用处理函数mkdir
int sys_mkdir(char *pathname, int mode);
//系统调用处理函数rmdir
int sys_rmdir(char *pathname);
//系统调用处理函数link
int sys_link(char *oldname, char *newname);
//系统调用处理函数unlink
int sys_unlink(char *filename);
//获取当前路径
char *sys_getcwd(char *buf, size_t size);
//切换当前目录
int sys_chdir(char *pathname);
//切换根目录
int sys_chroot(char *pathname);

//创建设备文件系统调用
int sys_mknod(char *filename, int mode, int dev);

#define P_EXEC IXOTH
#define P_READ IROTH
#define P_WRITE IWOTH
//判断当前进程对该inode指向的文件是否有相应权限
bool permission(inode_t *inode, uint16 mask);//mask为想要获得的权限


/**************/
/*file.c*/
/**************/
//初始化
void file_init();
//从文件描述符表中获得一个空闲的(引用计数为0)文件描述符表项
file_t *get_file();
//释放文件描述符表项
void put_file(file_t *file);
//系统调用处理函数open
fd_t sys_open(char *filename, int flags, int mode);
//系统调用处理函数creat
fd_t sys_creat(char *filename, int mode);
//系统调用处理函数close
void sys_close(fd_t fd);
//系统调用处理函数read
int sys_read(fd_t fd, char *buf, int len);
//系统调用处理函数write
int sys_write(fd_t fd, char *buf, int len);
//系统调用处理函数lseek
int sys_lseek(fd_t fd, off_t offset, int whence);
//系统调用处理函数readdir
int sys_readdir(fd_t fd, dentry_t *dir, uint32 count);

fd_t sys_dup(fd_t oldfd);

fd_t sys_dup2(fd_t oldfd, fd_t newfd);

/**************/
/*stat.c*/
/**************/
int sys_stat(char *filename, stat_t *statbuf);

int sys_fstat(fd_t fd, stat_t *statbuf);

/**************/
/*dev.c*/
/**************/
void dev_init();

/**************/
/*pipe.c*/
/**************/
int pipe_read(inode_t *inode, char *buf, int len);
int pipe_write(inode_t *inode, char *buf, int len);
int sys_pipe(fd_t pipefd[2]);
#endif