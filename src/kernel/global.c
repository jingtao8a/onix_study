#include "../include/global.h"
#include "../include/debug.h"
#include "../include/string.h"

static descriptor_t gdt[GDT_SIZE]; // 内核全局描述符表
static pointer_t gdt_ptr;          // 内核全局描述符表指针
tss_t tss;                  // 任务状态段

static void descriptor_init(descriptor_t* desc, uint32 base, uint32 limit) {//初始化段描述符的基地址和段界限
    desc->base_low = base & 0xffffff;
    desc->base_high = (base >> 24) & 0xff;
    desc->limit_low = limit & 0xffff;
    desc->limit_high = (limit >> 16) & 0xf;
}

void gdt_init() {
    LOGK("init gdt!!!\n");
    memset(gdt, 0, sizeof(gdt));//清0
    descriptor_t *desc;

    desc = gdt + KERNEL_CODE_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->type = 0b1010;//代码 / 非一致性 / 可读 / 没有被cpu访问过
    desc->segment = 1;//代码段
    desc->DPL = 0;
    desc->present = 1;
    desc->long_mode = 0;
    desc->big = 1;
    desc->granularity = 1;//段界限粒度位4k

    desc = gdt + KERNEL_DATA_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->type = 0b0010;   // 数据 / 向上增长 / 可写 / 没有被访问过
    desc->segment = 1;     // 数据段
    desc->DPL = 0;         // 内核特权级
    desc->present = 1;     // 在内存中
    desc->long_mode = 0;   // 不是 64 位
    desc->big = 1;         // 32 位
    desc->granularity = 1; // 4K

    desc = gdt + USER_CODE_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 代码段
    desc->granularity = 1; // 4K
    desc->big = 1;         // 32 位
    desc->long_mode = 0;   // 不是 64 位
    desc->present = 1;     // 在内存中
    desc->DPL = 3;         // 用户特权级
    desc->type = 0b1010;   // 代码 / 非一致性 / 可读 / 没有被访问过

    desc = gdt + USER_DATA_IDX;
    descriptor_init(desc, 0, 0xFFFFF);
    desc->segment = 1;     // 数据段
    desc->granularity = 1; // 4K
    desc->big = 1;         // 32 位
    desc->long_mode = 0;   // 不是 64 位
    desc->present = 1;     // 在内存中
    desc->DPL = 3;         // 用户特权级
    desc->type = 0b0010;   // 数据 / 向上增长 / 可写 / 没有被访问过

    gdt_ptr.base = (uint32)&gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;
    asm volatile(
        "lgdt gdt_ptr\n");//加载tss段选择子到tr段寄存
}

void tss_init()
{
    memset(&tss, 0, sizeof(tss));//清0

    tss.ss0 = KERNEL_DATA_SELECTOR;//设置ring0 的栈段选择子ss0位KERNEL_DATA_SELECTOR
    tss.iobase = sizeof(tss);//io位图的基地址

    descriptor_t *desc = gdt + KERNEL_TSS_IDX;
    descriptor_init(desc, (uint32)&tss, sizeof(tss) - 1);
    desc->segment = 0;     // 系统段
    desc->granularity = 0; // 段界限粒度为字节
    desc->big = 0;         // 固定为 0
    desc->long_mode = 0;   // 固定为 0
    desc->present = 1;     // 在内存中
    desc->DPL = 0;         // 内核特权级
    desc->type = 0b1001;   // 32 位可用 tss

    // BMB;
    asm volatile(
        "ltr %%ax\n" ::"a"(KERNEL_TSS_SELECTOR));//加载tss段选择子到tr段寄存器
}