[bits 32]

section .text
global inb
global inw
global outb
global outw

inb:
    push ebp
    mov ebp, esp
    xor eax, eax
    
    mov edx, [ebp + 8]
    in al, dx;将端口号dx的 8bit 输入到ax
    jmp $+2;延迟
    jmp $+2
    jmp $+2
    
    leave;恢复栈帧
    ret
    ret

inw:
    push ebp
    mov ebp, esp
    xor eax, eax

    mov edx, [ebp + 8]
    in ax, dx;将端口号dx的 16bit 输入到ax
    jmp $+2;延迟
    jmp $+2
    jmp $+2
    
    leave;恢复栈帧
    ret
    ret

outb:
    push ebp
    mov ebp, esp
    
    mov edx, [ebp + 8]
    mov eax, [ebp + 12]
    out dx, al;将al中的 8bit 输出到 端口号 dx
    jmp $+2;延迟
    jmp $+2
    jmp $+2
    
    leave;恢复栈帧
    ret

outw:
    push ebp
    mov ebp, esp

    mov edx, [ebp + 8]
    mov eax, [ebp + 12]
    out dx, ax;将al中的 16bit 输出到 端口号 dx
    jmp $+2;延迟
    jmp $+2
    jmp $+2

    leave;恢复栈帧
    ret
