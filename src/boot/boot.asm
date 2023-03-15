[org 0x7c00]

;设置屏幕模式为文本模式，清除屏幕
mov ax, 3
int 0x10

;初始化段寄存器
mov ax, 0
mov ds, ax
mov es, ax 
mov ss, ax
mov sp, 0x7c00

mov si, booting
call print
mov edi, 0x1000;读取的目标内存
mov ecx, 2;起始扇区
mov bl, 4;扇区数量
call read_disk

cmp word[0x1000], 0x55aa
jnz error

jmp 0:0x1002

jmp $


read_disk:
    push dx
    push ax
    ;设置读写扇区的数量
    mov dx, 0x1f2
    mov al, bl
    out dx, al

    ;设置起始扇区号
    inc dx;0x1f3
    mov al, cl;起始扇区的低八位
    out dx, al

    inc dx;0x1f4
    shr ecx, 8
    mov al, cl
    out dx, al;起始扇区的中八位

    inc dx;0x1f5
    shr ecx, 8
    mov al, cl
    out dx, al;起始扇区的高八位

    inc dx;0x1f6
    shr ecx, 8;
    and cl, 0b1111;将起始扇区的最高4位取出
    mov al, 0b1110_0000
    or al, cl
    out dx, al;选择主盘-LBA模式，并且将起始扇区的最高4位设置

    inc dx;0x1f7
    mov al, 0x20;读硬盘
    out dx, al

    xor ecx, ecx;
    mov cl, bl;cl用于记录读取的扇区数量,也是.read的循环数

    .read:
        call waits;//等待数据准备完毕
        call reads;/读数据
        loop .read

    pop ax
    pop dx
    ret

waits:
    push dx
    push ax
    
    mov dx, 0x1f7
    .check:
        in al, dx
        jmp $+2;一点延迟
        jmp $+2;
        jmp $+2;
        and al, 0b1000_1000
        cmp al, 0b0000_1000
        jnz .check
    pop ax
    pop dx
    ret

reads:
    push cx
    push dx
    mov dx, 0x1f0
    mov cx, 256;读256个word = 512 byte
    .readw:
        in ax, dx
        jmp $+2;一点延迟
        jmp $+2;
        jmp $+2;
        mov [edi], ax
        add edi, 2
        loop .readw
    pop dx
    pop cx
    ret

print:
    push ax
    mov ah, 0x0e
.next:    
    mov al, [si]
    cmp al, 0
    jz .done
    int 0x10
    inc si
    jmp .next
.done:
    pop ax
    ret

error:
    mov si, msg
    call print
    jmp $


msg:
    db "Booting Error!!!", 10, 13, 0

booting:
    db "Booting Onix ...", 10, 13, 0

times 510 - ($ - $$) db 0
db 0x55, 0xaa ;主引导扇区结束字节0x55 0xaa dw 0xaa55