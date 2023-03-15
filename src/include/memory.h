#ifndef _MEMORY_H_
#define _MEMORY_H_
#include "types.h"
#include "bitmap.h"

#define PAGE_SIZE 0x1000     // 一页的大小 4K
#define MEMORY_BASE 0x100000 // 1M，可用内存开始的位置

//内核占用的内存大小 16M 其中8~12M 为磁盘高速缓冲区
#define KERNEL_MEMORY_SIZE 0x1000000

//内核高速缓冲地址
#define KERNEL_BUFFER_MEM 0x800000

//内核高速缓冲大小
#define KERNEL_BUFFER_SIZE 0x400000

//内存虚拟磁盘地址
#define KERNEL_RAMDISK_MEM (KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE)

//内存虚拟磁盘大小
#define KERNEL_RAMDISK_SIZE 0x400000

//用户程序地址
#define USER_EXEC_ADDR KERNEL_MEMORY_SIZE

//用户映射内存开始位置 128 M
#define USER_MMAP_ADDR 0x8000000

//用户映射内存大小 128M
#define USER_MMAP_SIZE 0x8000000

//用户栈顶地址 256 M
#define USER_STACK_TOP 0x10000000

//用户栈的最大空间 2M
#define USER_STACK_SIZE 0X200000

//用户栈底地址 256 - 2M
#define USER_STACK_BOTTOM (USER_STACK_TOP - USER_STACK_SIZE)

typedef struct page_entry_t
{
    uint8 present : 1;  // 在内存中
    uint8 write : 1;    // 0 只读 1 可读可写
    uint8 user : 1;     // 1 所有人 0 超级用户 DPL < 3
    uint8 pwt : 1;      // page write through 1 直写模式，0 回写模式
    uint8 pcd : 1;      // page cache disable 禁止该页缓冲
    uint8 accessed : 1; // 被访问过，用于统计使用频率
    uint8 dirty : 1;    // 脏页，表示该页缓冲被写过
    uint8 pat : 1;      // page attribute table 页大小 4K/4M
    uint8 global : 1;   // 全局，所有进程都用到了，该页不刷新缓冲
    uint8 shared : 1;   // 共享内存页，与cpu无关
    uint8 private : 1;  // 私有内存页， 与cpu无关
    uint8 readonly : 1; // 只读内存页，与cpu无关
    uint32 index : 20;  // 页索引
} _packed page_entry_t;

extern bitmap_t kernel_map;

void memory_init(uint32 magic, uint32 addr);
void memory_map_init();

//得到cr2 寄存器
uint32 get_cr2();

//获得cr3寄存器的值
uint32 get_cr3();

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(uint32 pde);

//开启分页
void mapping_init();

//分配count个连续的内核页
uint32 alloc_kpage(uint32 count);

//释放个连续的内核页
void free_kpage(uint32 vaddr, uint32 count);

//获取页表项
page_entry_t *get_entry(uint32 vaddr, bool create);

//刷新快表
void flush_tlb(uint32 vaddr);

//将vaddr 映射物理内存
void link_page(uint32 vaddr);

//去掉vaddr对应的物理内存映射
void unlink_page(uint32 vaddr);

page_entry_t *copy_pde();

void page_fault(uint32 vector, uint32 edi, uint32 esi, uint32 ebp, uint32 esp, 
                        uint32 ebx, uint32 edx, uint32 ecx, uint32 eax,
                        uint32 gs, uint32 fs, uint32 es, uint32 ds,
                        uint32 vector0, uint32 error, uint32 eip, uint32 cs, uint32 eflags);

uint32 sys_brk(void *addr);//brk系统调用处理函数


void free_pde();//释放当前进程的页目录

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

int sys_munmap(void *addr, size_t length);

#endif