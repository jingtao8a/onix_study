#include "../include/memory.h"
#include "../include/debug.h"
#include "../include/assert.h"
#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/multiboot2.h"
#include "../include/tasks.h"
#include "../include/syscall.h"
#include "../include/fs.h"
#include "../include/printk.h"

//ards type
#define ZONE_VALID 1 //ards可用内存区域
#define ZONE_RESERVED 2 //ards 不可用区域

#define IDX(addr) ((uint32)addr >> 12) //获取addr的页索引
#define DIDX(addr) (((uint32)addr >> 22) & 0x3ff)//获取线性地址addr的页目录项索引
#define TIDX(addr) (((uint32)addr >> 12) & 0x3ff)//获取线性地址addr的页表项索引
#define PAGE(idx) ((uint32)idx << 12) //获取页索引idx对应的页的开始位置
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)


#define PDE_MASK 0xFFC00000
// 内核页目录起始位置
#define KERNEL_PAGE_DIR 0x1000
// 内核页表的起始地址
static uint32 KERNEL_PAGE_TABLE[] = {
    0x2000,
    0x3000,
    0x4000,
    0x5000
};

#define KERNEL_MAP_BITS 0x6000 //位图的buf起始地址
bitmap_t kernel_map;

typedef struct ards_t {
    uint64 base;//内存基地址
    uint64 size;//内存长度
    uint32 type;//类型
}_packed ards_t;

static uint32 memory_base = 0; //可用内存基地址(byte)
static uint32 memory_size = 0; //可用内存大小(byte)
static uint32 total_pages = 0;//所有内存页数
static uint32 free_pages = 0;//空闲内存页数

#define used_pages (total_pages - free_pages) //已用页数

void memory_init(uint32 magic, uint32 addr) {
    uint32 count;
    ards_t *ptr;
    // 如果是 onix loader 进入的内核
    if (magic == ONIX_MAGIC)
    {
        count = *(uint32 *)addr;
        ards_t *ptr = (ards_t *)(addr + 4);

        for (size_t i = 0; i < count; i++, ptr++)
        {
            LOGK("Memory base 0x%p size 0x%p type %d\n",
                 (uint32)ptr->base, (uint32)ptr->size, (uint32)ptr->type);
            if (ptr->type == ZONE_VALID && ptr->size > memory_size)
            {
                memory_base = (uint32)ptr->base;
                memory_size = (uint32)ptr->size;
            }
        }
    }
    else if (magic == MULTIBOOT2_MAGIC)//由grub引导kernel
    {
        uint32 size = *(unsigned int *)addr;
        multi_tag_t *tag = (multi_tag_t *)(addr + 8);

        LOGK("Announced mbi size 0x%x\n", size);
        while (tag->type != MULTIBOOT_TAG_TYPE_END)
        {
            if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
                break;
            // 下一个 tag 对齐到了 8 字节
            tag = (multi_tag_t *)((uint32)tag + ((tag->size + 7) & ~7));
        }

        multi_tag_mmap_t *mtag = (multi_tag_mmap_t *)tag;
        multi_mmap_entry_t *entry = mtag->entries;
        while ((uint32)entry < (uint32)tag + tag->size)
        {
            LOGK("Memory base 0x%p size 0x%p type %d\n",
                 (uint32)entry->addr, (uint32)entry->len, (uint32)entry->type);
            count++;
            if (entry->type == ZONE_VALID && entry->len > memory_size)
            {
                memory_base = (uint32)entry->addr;
                memory_size = (uint32)entry->len;
            }
            entry = (multi_mmap_entry_t *)((uint32)entry + mtag->entry_size);
        }
    }
    else
    {
        panic("Memory init magic unknown 0x%p\n", magic);
    }

    LOGK("ARDS count %d\n", count);
    LOGK("Memory base 0x%p\n", (uint32)memory_base);
    LOGK("Memory size 0x%p\n", (uint32)memory_size);

    assert(memory_base == MEMORY_BASE); // 内存开始的位置为 1M
    assert((memory_size & 0xfff) == 0); // 要求按页对齐

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);
    free_pages = IDX(memory_size);

    LOGK("Total pages %d\n", total_pages);
    LOGK("Free pages %d\n", free_pages);

    if (memory_size < KERNEL_MEMORY_SIZE)
    {
        panic("System memory is %dM too small, at least %dM needed\n",
              memory_size / MEMORY_BASE, KERNEL_MEMORY_SIZE / MEMORY_BASE);
    }
}


static uint32 start_page = 0;//可分配物理内存起始位置
static uint8 *memory_map;//物理内存数组
static uint32 memory_map_pages;//物理内存数组占用的页数

void memory_map_init() {
    memory_map = (uint8 *)memory_base;//初始化物理内存数组
    memory_map_pages = div_round_up(total_pages, PAGE_SIZE);
    LOGK("Memory map page count %d\n", memory_map_pages);
    free_pages -= memory_map_pages;
    //清空物理内存数组
    memset(memory_map, 0, memory_map_pages * PAGE_SIZE);

    //前1M的页已经被占用，物理内存数组使用的页也已经被占用
    start_page = IDX(MEMORY_BASE) + memory_map_pages;
    for (size_t i = 0; i < start_page; ++i) {
        memory_map[i] = 1;//物理内存数组置为1,表示该页被占用
    }
    LOGK("Total pages %d free pages %d", total_pages, free_pages);
    uint32 length = (IDX(KERNEL_MEMORY_SIZE) - IDX(MEMORY_BASE)) / 8;
    bitmap_init(&kernel_map, (uint8 *)KERNEL_MAP_BITS, length, IDX(MEMORY_BASE));//map->offset设置为可分配的起始页号
    bitmap_scan(&kernel_map, memory_map_pages);//memory_map数组使用的两个页已经不能用于分配了，在位图中将其对应位置1
}

static uint32 get_page() {
    for (size_t i = start_page; i < total_pages; ++i) {
        if (!memory_map[i]) {//该页没有被占用
            memory_map[i] = 1;
            --free_pages;
            assert(free_pages >= 0)
            uint32 page = PAGE(i);
            LOGK("GET page 0x%p\n", page);
            return page;
        }
    }
    panic("Out of Memory!!!");
}

static void put_page(uint32 addr) {//addr为物理地址
    ASSERT_PAGE(addr);
    uint32 idx = IDX(addr);

    assert(idx >= start_page && idx < total_pages);
    
    assert(memory_map[idx] >= 1);

    memory_map[idx]--;//物理引用计数减1

    if(!memory_map[idx]) {
        ++free_pages;
    }
    assert(free_pages >= 0 && free_pages < total_pages);
    LOGK("PUT page 0x%p\n", addr);
}
//得到cr2寄存器
uint32 get_cr2() {
    asm volatile("movl %cr2, %eax\n");
}


// 得到 cr3 寄存器
uint32 get_cr3() {
    asm volatile("movl %cr3, %eax\n");
}

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(uint32 pde) {
    ASSERT_PAGE(pde);
    asm volatile("movl %%eax, %%cr3\n" ::"a"(pde));
}

//将cr0的最高位PG置为1,开启分页
static _inline void enable_page() {//设置位内联函数
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000, %eax\n"
        "movl %eax, %cr0\n");
}

static void entry_init(page_entry_t* entry, uint32 index) {
    *(uint32 *)entry = 0;
    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = index;
}

void mapping_init() {//开启分页
    page_entry_t *pde = (page_entry_t*)KERNEL_PAGE_DIR;//pde指向内核内核的页目录首地址
    memset(pde, 0, PAGE_SIZE);//页目录占用一个页的大小
    
    uint32 index = 0;

    for (size_t didx = 0; didx < (sizeof(KERNEL_PAGE_TABLE) / 4); ++didx) {
        page_entry_t *pte = (page_entry_t *)KERNEL_PAGE_TABLE[didx];
        memset(pte, 0, PAGE_SIZE);//页表初始化

        page_entry_t *dentry = &pde[didx];
        entry_init(dentry, IDX((uint32)pte));
        dentry->user = 0;
        for (size_t tidx = 0; tidx < 1024; ++tidx, ++index) {
            if (index == 0) {//第一个页表项不使用,为造成空指针访问，缺页异常，便于排错
                continue;//实际上就是0~0x1000的内存不访问
            }
            page_entry_t *tentry = &pte[tidx];
            entry_init(tentry, index);
            tentry->user = 0;
            memory_map[index] = 1;//该页被占用
        }
    }
    free_pages -= IDX(KERNEL_MEMORY_SIZE) - start_page;

    entry_init(&pde[1023], IDX(KERNEL_PAGE_DIR));//将页目录最后一个表项指向页目录的首地址
    set_cr3((uint32)pde);//设置cr3寄存器指向页目录
    enable_page();//开启分页，cr0的PG位置为1
}


static page_entry_t* get_pde() {//返回页目录的首地址（线性地址）
    return (page_entry_t *)(0xfffff000);
}

static page_entry_t* get_pte(uint32 vaddr, bool create) {//返回该线性地址对应的页表首地址(线性地址)
    page_entry_t *pde = get_pde();
    uint32 idx = DIDX(vaddr);//获取线性地址vaddr对应的页目录项索引
    page_entry_t *entry = &pde[idx];

    assert(create || (!create && entry->present));

    page_entry_t *table = (page_entry_t *)(PDE_MASK | (idx << 12));

    if (!entry->present) {
        LOGK("Get and create page table entry for 0x%p\n", vaddr);
        uint32 page = get_page();//获得一个未占用页的物理地址，这个地址还不能使用
        entry_init(entry, IDX(page));//只能用这个页的物理地址得到得页号 用来初始化页目录对应的页目录项
        // memset((void *)page, 0, PAGE_SIZE);bug，访问page又会触发page fault
        memset((void*)table, 0, PAGE_SIZE);
    }
    
    return table;
}

page_entry_t *get_entry(uint32 vaddr, bool create) {//返回线性地址vaddr对应的页表项的线性地址
    page_entry_t *pte = get_pte(vaddr, create);
    return &pte[TIDX(vaddr)];
}

//刷新虚拟地址 vaddr 的 快表 TLB
void flush_tlb(uint32 vaddr) {
    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}

//将vaddr 映射物理内存
void link_page(uint32 vaddr) {
    ASSERT_PAGE(vaddr);
    page_entry_t *entry = get_entry(vaddr, true);

    uint32 index = IDX(vaddr);//线性地址vaddr指向的页号
    //页面已经存在，直接返回
    if (entry->present) {
        return;
    }

    uint32 paddr = get_page();//paddr为物理地址
    entry_init(entry, IDX(paddr));
    flush_tlb(vaddr);

    LOGK("LINK from 0x%p to 0x%p\n", vaddr, paddr);
}

//去掉vaddr对应的物理内存映射
void unlink_page(uint32 vaddr) {
    ASSERT_PAGE(vaddr);

    page_entry_t *pde = get_pde();
    page_entry_t *entry = &pde[DIDX(vaddr)];
    if (!entry->present) {
        return;
    }
    
    entry = get_entry(vaddr, false);
    if (!entry->present) {
        return;
    }

    entry->present = false;

    uint32 paddr = PAGE(entry->index);
    DEBUGK("UNLINK FROM 0x%p to 0x%p", vaddr, paddr);
    put_page(paddr);
    flush_tlb(vaddr);
}

static uint32 copy_page(void *page) {
    uint32 paddr = get_page();
    uint32 vaddr = 0;

    page_entry_t *entry = get_pte(vaddr, false);
    entry_init(entry, IDX(paddr));//现在虚拟地址0 -> paddr
    flush_tlb(vaddr);//刷新虚拟地址0的映射

    memcpy((void *)0, (void *)page, PAGE_SIZE);

    entry->present = false;//重新让0号物理页没有被虚拟地址映射
    flush_tlb(vaddr);//刷新虚拟地址0的映射

    return paddr;//返回新的页表的物理地址
}

page_entry_t *copy_pde() {
    task_t *task = running_task();
    page_entry_t *pde = (page_entry_t *)alloc_kpage(1);
    memcpy(pde, (void *)task->pde, PAGE_SIZE);
    //将最后一个页目录项设置为页目录所在的页号
    page_entry_t *entry = &pde[1023];
    entry_init(entry, IDX(pde));

    page_entry_t *dentry;
    for (size_t didx = (sizeof(KERNEL_PAGE_TABLE) / 4); didx < 1023; didx++) {
        dentry = &pde[didx];
        if (!dentry->present) {
            continue;
        }
        page_entry_t *pte = (page_entry_t *)(PDE_MASK | didx << 12);//bug
        for (size_t tidx = 0; tidx < 1024; ++tidx) {
            entry = &pte[tidx];
            if (!entry->present) {
                continue;
            }
            assert(memory_map[entry->index] > 0);//该页表项指向的物理页已经被占用了
            if (!entry->shared) {
                entry->write = false;//如果过不是共享页，设置为只读
            }
            memory_map[entry->index]++;//该物理页的引用计数加1
            assert(memory_map[entry->index] < 255);
        }
        uint32 paddr = copy_page(pte);//拷贝一张页表
        dentry->index = IDX(paddr);
    }
    set_cr3(task->pde);
    return pde;
}

typedef struct page_error_code_t
{
    uint8 present : 1;
    uint8 write : 1;
    uint8 user : 1;
    uint8 reserved0 : 1;
    uint8 fetch : 1;
    uint8 protection : 1;
    uint8 shadow : 1;
    uint16 reserved1 : 8;
    uint8 sgx : 1;
    uint16 reserved2;
} _packed page_error_code_t;

void page_fault(uint32 vector, uint32 edi, uint32 esi, uint32 ebp, uint32 esp, 
                        uint32 ebx, uint32 edx, uint32 ecx, uint32 eax,
                        uint32 gs, uint32 fs, uint32 es, uint32 ds,
                        uint32 vector0, uint32 error, uint32 eip, uint32 cs, uint32 eflags) {
    assert(vector == 0xe);
    uint32 vaddr = get_cr2();
    LOGK("fault address 0x%p\n", vaddr);

    page_error_code_t *code = (page_error_code_t *)&error;
    task_t *task = running_task();

    // assert(KERNEL_MEMORY_SIZE <= vaddr && vaddr < USER_STACK_TOP);
    if (vaddr < USER_EXEC_ADDR || vaddr >= USER_STACK_TOP) {
        assert(task->uid);
        printk("Segmentation Fault!!!\n");
        task_exit(-1);
    }
    if (code->present) {
        assert(code->write);//写操作造成的缺页异常
        page_entry_t *entry = get_entry(vaddr, false); 
        
        assert(entry->present);//该页表项指向的物理页存在
        assert(memory_map[entry->index] > 0);
        assert(!entry->shared);
        assert(!entry->readonly);//只读内存页不应该被写(程序员要求的)
        assert(memory_map[entry->index] > 0)
        if (memory_map[entry->index] == 1) {//该物理页引用计数为1
            entry->write = true;
            LOGK("WRITE page for 0x%p\n", vaddr);
        } else {//引用计数为2 copy on write 写时复制
            void *page = (void *)PAGE(IDX(vaddr));
            uint32 paddr = copy_page(page);//拷贝一页物理页
            memory_map[entry->index]--;//该物理页的引用计数减1
            entry_init(entry, IDX(paddr));//将当前的页表项指向新拷贝的物理页
            flush_tlb(vaddr);//刷新快表
            LOGK("COPY page for 0x%p\n", vaddr);
        }
        return;
    }

    if (!code->present && (vaddr < task->brk || vaddr >= USER_STACK_BOTTOM)) {//vaddr是在合理的堆地址或者栈地址，就进行内存映射
        uint32 page = PAGE(IDX(vaddr));
        link_page(page);
        return;
    }
    
    panic("page fault");
}

uint32 sys_brk(void *addr) {
    LOGK("task brk 0x%p\n", addr);
    uint32 brk = (uint32)addr;
    ASSERT_PAGE((uint32)addr);

    task_t *task = running_task();
    assert(task->uid != KERNEL_USER);

    assert(task->end <= brk && brk < USER_MMAP_ADDR);

    uint32 old_brk = task->brk;
    if (old_brk > brk) {
        uint32 page = brk;
        while (page < old_brk) {
            unlink_page(page);
            page += PAGE_SIZE;
        }
    } else if (IDX(brk - old_brk) > free_pages) {//内存不够用了
        return -1;
    }

    task->brk = brk;
    return 0;
}

/******************************/
/*   calloc_kpage free_kpage  */
/******************************/
static uint32 scan_page(bitmap_t *kernel_map, uint32 count) {
    assert(count > 0);
    int32 ret = bitmap_scan(kernel_map, count);
    if ((int)ret == EOF) {
        panic("Scan page fail!!!");
    }
    uint32 addr = PAGE(ret);
    LOGK("Scan page 0x%p count %d\n", addr, count);
    return addr;
}

static void reset_page(bitmap_t *kernel_map, uint32 addr, uint32 count) {
    ASSERT_PAGE(addr);
    assert(count > 0);
    uint32 index = IDX(addr);
    for (size_t i = 0; i < count; i++) {
        assert(bitmap_test(kernel_map, index + i));
        bitmap_set(kernel_map, index + i, false);//置为0
    }
}

//分配count个连续的页
uint32 alloc_kpage(uint32 count) {
    assert(count > 0);
    uint32 vaddr = scan_page(&kernel_map, count);
    LOGK("ALLOC kernel pages 0x%p count %d", vaddr, count);
    return vaddr;
}

//释放个连续的内核页
void free_kpage(uint32 vaddr, uint32 count) {
    assert(count > 0);
    reset_page(&kernel_map, vaddr, count);
    LOGK("FREE kernel pages 0x%p count %d\n", vaddr, count);
}


void free_pde() {//释放当前进程的页目录
    task_t *task = running_task();
    assert(task->uid != KERNEL_USER);

    page_entry_t *pde = get_pde();

    for (size_t didx = (sizeof(KERNEL_PAGE_TABLE) / 4); didx < 1023; ++didx) {
        page_entry_t *dentry = &pde[didx];
        if (!dentry->present) {
            continue;
        }

        page_entry_t *pte = (page_entry_t *)(PDE_MASK | (didx << 12));

        for (size_t tidx = 0; tidx < 1024; tidx++) {
            page_entry_t *entry = &pte[tidx];
            if (!entry->present) {
                continue;
            }

            assert(memory_map[entry->index] > 0);
            //释放物理页
            put_page(PAGE(entry->index));
        }
        //释放物理页表
        put_page(PAGE(dentry->index));
    }
    //释放页目录
    free_kpage(task->pde, 1);
    LOGK("free pages %d\n", free_pages);
}

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    ASSERT_PAGE((uint32)addr);

    uint32 count = div_round_up(length, PAGE_SIZE);//需要映射的页的数量
    uint32 vaddr = (uint32)addr;//虚拟地址

    task_t *task = running_task();
    if (!vaddr) {
        vaddr = scan_page(task->vmap, count);
    }
    assert(vaddr >= USER_MMAP_ADDR && vaddr < USER_STACK_BOTTOM);
    for (size_t i = 0; i < count; ++i) {
        uint32 page = vaddr + PAGE_SIZE * i;
        link_page(page);//虚拟地址page --映射--> 物理地址？？？
        bitmap_set(task->vmap, IDX(page), true);//该内存映射页已经被使用

        page_entry_t *entry = get_entry(page, false);
        entry->user = true;
        entry->write = false;//映射的页默认不可写
        entry->readonly = true;

        if (prot & PROT_WRITE) {
            entry->write = true;
            entry->readonly = false;
        }
        if (flags & MAP_SHARED) {
            entry->shared = true;
        }
        if (flags & MAP_PRIVATE) {
            entry->private = true;
        }

    }

    if (fd != EOF) {//需要将文件映射到页
        lseek(fd, offset, SEEK_SET);
        read(fd, (char *)vaddr, length);
    }
    return (void *)vaddr;//映射的虚拟地址
}

int sys_munmap(void *addr, size_t length) {
    task_t *task = running_task();
    uint32 vaddr = (uint32)addr;//内存映射虚拟地址
    assert(vaddr >= USER_MMAP_ADDR && vaddr < USER_STACK_BOTTOM);

    ASSERT_PAGE(vaddr);
    uint32 count = div_round_up(length, PAGE_SIZE);

    for (size_t i = 0; i < count; ++i) {
        uint32 page = vaddr + PAGE_SIZE * i;
        unlink_page(page);
        assert(bitmap_test(task->vmap, IDX(page)));
        bitmap_set(task->vmap, IDX(page), false);
    }

    return 0;
}
