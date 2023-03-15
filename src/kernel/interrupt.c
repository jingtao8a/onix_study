#include "../include/interrupt.h"
#include "../include/global.h"
#include "../include/printk.h"
#include "../include/types.h"
#include "../include/io.h"
#include "../include/debug.h"
#include "../include/stdlib.h"
#include "../include/tasks.h"
#include "../include/assert.h"
#include "../include/memory.h"

#define ENTRY_SIZE 0x30
extern handler_t handler_entry_table[ENTRY_SIZE];//中断描述符的偏移地址数组,用于初始化IDT表

//中断控制器8259A的端口
#define PIC_M_CTRL 0x20 //主片的控制端口
#define PIC_M_DATA 0X21 //主片的数据端口
#define PIC_S_CTRL 0xa0 //从片的控制端口
#define PIC_S_DATA 0xa1 //从片的数据端口

#define PIC_EOI 0x20 //用于通知中断控制器中断结束

static gate_t idt[IDT_SIZE];
static pointer_t idt_ptr;

handler_t handler_table[IDT_SIZE];//0~0x30可以用于挂用中断向量对应的中断处理函数

static char *messages[] = {
    "#DE Divide Error\0",
    "#DB RESERVED\0",
    "--  NMI Interrupt\0",
    "#BP Breakpoint\0",
    "#OF Overflow\0",
    "#BR BOUND Range Exceeded\0",
    "#UD Invalid Opcode (Undefined Opcode)\0",
    "#NM Device Not Available (No Math Coprocessor)\0",
    "#DF Double Fault\0",
    "    Coprocessor Segment Overrun (reserved)\0",
    "#TS Invalid TSS\0",
    "#NP Segment Not Present\0",
    "#SS Stack-Segment Fault\0",
    "#GP General Protection\0",
    "#PF Page Fault\0",
    "--  (Intel reserved. Do not use.)\0",
    "#MF x87 FPU Floating-Point Error (Math Fault)\0",
    "#AC Alignment Check\0",
    "#MC Machine Check\0",
    "#XF SIMD Floating-Point Exception\0",
    "#VE Virtualization Exception\0",
    "#CP Control Protection Exception\0",
};

void set_interrupt_handler(uint32 irq, handler_t handler) {
    assert(irq >= 0 && irq < 16);
    handler_table[IRQ_MASTER_NR + irq] = handler;
}

void set_interrupt_mask(uint32 irq, bool enable) {
    assert(irq >= 0 && irq < 16);
    uint16 port;
    if (irq < 8) {
        port = PIC_M_DATA;
    } else {
        port = PIC_S_DATA;
        irq -= 8;
    }
    if (enable) {
        outb(port, inb(port) & ~(1 << irq));
    } else {
        outb(port, inb(port) | (1 << irq));
    }
}

bool interrupt_disable() {//清除 IF 位， 返回设置之前的值
    asm volatile (
        "pushfl\n" //将当前eflags压入栈中
        "cli\n" //清除IF位，此时外中断已经被屏蔽
        "popl %eax\n"//将刚才压入的eflags弹出到eax
        "shrl $9, %eax\n"//将eax右移9位,为了获取IF位的值
        "andl $1, %eax\n"//只需eax位
    );
}

bool get_interrupt_state() {//获得 IF 位
    asm volatile (
        "pushfl \n"
        "popl %eax\n"
        "shrl $9, %eax\n"
        "andl $1, %eax\n"
    );
}

void set_interrupt_state(bool state) {//设置 IF位的值
    if (state) {
        asm volatile("sti\n");
    } else {
        asm volatile("cli\n");
    }
}

static void exception_handler(uint32 vector, uint32 edi, uint32 esi, uint32 ebp, uint32 esp, 
                        uint32 ebx, uint32 edx, uint32 ecx, uint32 eax,
                        uint32 gs, uint32 fs, uint32 es, uint32 ds,
                        uint32 vector0, uint32 error, uint32 eip, uint32 cs, uint32 eflags) {
    char *message = NULL;
    if (vector < 22) {
        message = messages[vector];
    } else {
        message = messages[15];
    }
    printk("\nEXCEPTION : %s \n", message);
    printk(" VECTOR : 0x%02X\n", vector);
    printk("  ERROR : 0x%08X\n", error);
    printk(" EFLAGS : 0x%08X\n", eflags);
    printk("     CS : 0x%02X\n", cs);
    printk("    EIP : 0x%08X\n", eip);
    printk("    ESP : %x%08X\n", esp);
    hang();
}

void send_eoi(int vector) {
    if (vector >= 0x20 && vector < 0x28) {
        //主片触发的中断
        outb(PIC_M_CTRL, PIC_EOI);
    } else if (vector >= 0x28 && vector < 0x30) {
        //从片触发的中断
        outb(PIC_M_CTRL, PIC_EOI);
        outb(PIC_S_CTRL, PIC_EOI);
    }
}

static void default_handler(int vector) {
    send_eoi(vector);//表示该中断服务程序已经结束，清除ISR寄存器的对应位
    DEBUGK("[%d] default interrupt called ...\n", vector);
}

//初始化idt表
static void idt_init() {
    idt_ptr.base = (uint32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    for (size_t i = 0; i < ENTRY_SIZE; ++i) {
        gate_t *gate = &idt[i];
        handler_t handler = handler_entry_table[i];

        gate->offset0 = (uint32) handler & 0xffff;
        gate->offset1 = ((uint32) handler >> 16) & 0xffff;
        gate->selector = KERNEL_CODE_SELECTOR;
        gate->reserved = 0;
        gate->type = 0b1110;//中断门
        gate->segment = 0;
        gate->DPL = 0;//内核态
        gate->present = 1;
    }

    for (size_t i = 0; i < 0x20; ++i) {
        handler_table[i] = exception_handler;
    }
    handler_table[0xe] = page_fault;
    for (size_t i = 0x20; i < ENTRY_SIZE; ++i) {
        handler_table[i] = default_handler;
    }
    //初始化系统调用中断描述符
    extern void syscall_handler();
    gate_t *gate = &idt[0x80];
    gate->offset0 = (uint32) syscall_handler & 0xffff;
    gate->offset1 = ((uint32) syscall_handler >> 16) & 0xffff;
    gate->selector = KERNEL_CODE_SELECTOR;
    gate->reserved = 0;
    gate->type = 0b1110;//中断门
    gate->segment = 0;
    gate->DPL = 3;//用户态的程序是可以调用的
    gate->present = 1;

    asm volatile("lidt idt_ptr");
}

//初始化中断控制器
static void pic_init() {
    outb(PIC_M_CTRL, 0b00010001); // ICW1: 边沿触发, 级联 8259, 需要ICW4.
    outb(PIC_M_DATA, 0x20);       // ICW2: 起始中断向量号 0x20
    outb(PIC_M_DATA, 0b00000100); // ICW3: IR2接从片.
    outb(PIC_M_DATA, 0b00000001); // ICW4: 8086模式, 正常EOI

    outb(PIC_S_CTRL, 0b00010001); // ICW1: 边沿触发, 级联 8259, 需要ICW4.
    outb(PIC_S_DATA, 0x28);       // ICW2: 起始中断向量号 0x28
    outb(PIC_S_DATA, 2);          // ICW3: 设置从片连接到主片的 IR2 引脚
    outb(PIC_S_DATA, 0b00000001); // ICW4: 8086模式, 正常EOI

    outb(PIC_M_DATA, 0b11111111); // 关闭所有中断(除了时钟中断)
    outb(PIC_S_DATA, 0b11111111); // 关闭所有中断
}

void interrupt_init() {
    idt_init();
    pic_init(); 
}