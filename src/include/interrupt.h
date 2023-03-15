#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_
#include "types.h"

#define IDT_SIZE 256

#define IRQ_CLOCK 0      // 时钟
#define IRQ_KEYBOARD 1   // 键盘
#define IRQ_CASCADE 2    // 8259 从片控制器
#define IRQ_SERIAL_2 3   // 串口 2
#define IRQ_SERIAL_1 4   // 串口 1
#define IRQ_PARALLEL_2 5 // 并口 2
#define IRQ_FLOPPY 6     // 软盘控制器
#define IRQ_PARALLEL_1 7 // 并口 1
#define IRQ_RTC 8        // 实时时钟
#define IRQ_REDIRECT 9   // 重定向 IRQ2
#define IRQ_MOUSE 12     // 鼠标
#define IRQ_MATH 13      // 协处理器 x87
#define IRQ_HARDDISK 14  // ATA 硬盘第一通道
#define IRQ_HARDDISK2 15 // ATA 硬盘第二通道


#define IRQ_MASTER_NR 0x20 //主片起始向量号
#define IRQ_SLABE_NR 0x28 //从片起始向量号

typedef struct gate_t {
    uint16 offset0;  //段内偏移 0 ~ 15位
    uint16 selector; //代码段选择子
    uint8 reserved;//保留不用
    uint8 type: 4;//任务门 / 中断门/ 陷阱门
    uint8 segment: 1;//segment = 0 表示系统段
    uint8 DPL: 2;//使用int指令访问的最低权限
    uint8 present: 1; //是否有效
    uint16 offset1; // 段内偏移 16 ~ 31位
}_packed gate_t;

typedef void* handler_t; // 中断处理函数指针

void interrupt_init();

void send_eoi(int vector);//清除ISR寄存器中对应的标志位
void set_interrupt_handler(uint32 irq, handler_t handler);//设置irq对应的中断处理函数
void set_interrupt_mask(uint32 irq, bool enable);//屏蔽某个irq的信号

bool interrupt_disable();//清除 IF 位， 返回设置之前的值
bool get_interrupt_state();//获得 IF 位
void set_interrupt_state(bool state);//设置 IF位的值
#endif