[bits 32]

magic equ 0xe85250d6
i386 equ 0
length equ header_end - header_start
section .multiboot2
header_start:
    dd magic ;魔术
    dd i386;32位保护模式
    dd length;头部长度
    dd -(magic + i386 + length);校验和
    ;结束标记
    dw 0 ;type
    dw 0 ;flags
    dd 8 ;size
header_end:
extern device_init 
extern console_init
extern memory_init
extern gdt_init
extern tss_init
extern kernel_init

code_selector equ (1 << 3)
data_selector equ (2 << 3)

section .text
global _start
_start:
    push ebx; ards_count,如果是multiboot,那么存储的是multiboot信息结构体(32位物理地址)
    push eax; ONIX_MAGIC为0x20220205,multiboot_MAGIC为0x36d76289
    
    call device_init;//虚拟化设备初始化
    call console_init
    call memory_init
    call gdt_init
    jmp dword code_selector:_next
_next:
    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax; 初始化段寄存器
    mov esp, 0x10000; 修改栈顶
    call tss_init    
    call kernel_init
    jmp $
