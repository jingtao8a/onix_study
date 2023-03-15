[bits 32]

section .text

global _start

extern __libc_start_main
extern _init
extern _fini
extern main

_start:
    xor ebx, ebx; 清除栈底 ebx = 0
    pop esi;栈顶参数为argc
    mov ecx, esp;其次为argv

    and esp, -16;栈对齐,需要16字节对齐,esp的后四位清0
    push eax;
    push esp;用户程序栈最大地址
    push edx;动态链接器
    push _init;libc构造函数
    push _fini;libc析构函数
    push ecx;argv
    push esi;argc
    push main; 主函数
    
    call __libc_start_main

    ud2;程序不可能走到这里，否则可能是哪里出了什么问题