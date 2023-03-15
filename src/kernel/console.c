#include "../include/console.h"
#include "../include/io.h"
#include "../include/string.h"
#include "../include/clock.h"
#include "../include/interrupt.h"
#include "../include/device.h"

#define CRT_ADDR_REG 0x3d4 //CRT(6845)地址寄存器
#define CRT_DATA_REG 0x3d5 //CRT(6845)数据寄存器
#define CRT_CURSOR_H 0xE //光标位置 高位
#define CRT_CURSOR_L 0xF //光标位置 低位
#define CRT_START_ADDR_H 0XC //显示内存起始位置 高位
#define CRT_START_ADDR_L 0xD //显示内存起始位置 低位

#define MEM_BASE 0XB8000//显卡内存的起始位置
#define MEM_SIZE 0X4000 //显卡内存的大小8K
#define MEM_END  (MEM_BASE + MEM_SIZE) //显卡内存结束位置
#define WIDTH 80 //屏幕文本列数
#define HEIGHT 25 //屏幕文本行数
#define ROW_SIZE (WIDTH * 2) //每行字节数
#define SCR_SIZE (ROW_SIZE * HEIGHT) //屏幕字节数

#define ASCII_NUL 0X00
#define ASCII_ENQ 0x05
#define ASCII_BEL 0X07 // \a
#define ASCII_BS 0X08  // \b
#define ASCII_HT 0x09  // \t
#define ASCII_LF 0x0A  // \n
#define ASCII_VT 0x0B  // \v
#define ASCII_FF 0x0C  // \f
#define ASCII_CR 0x0D  // \r
#define ASCII_DEL 0x7F 

static uint32 screen;//当前显示器开始的内存位置

static uint32 pos;//记录当前光标的内存位置

static uint32 x, y;//记录当前光标的坐标

static uint8 attr = 7;//显示器上字符样式

static uint16 erase = 0x0720;//显示器上的一个空格字符

//获得当前显示器的位置
static void get_screen() {
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);
    screen = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);
    screen |= inb(CRT_DATA_REG);
    screen <<= 1; //screen * 2
    screen += MEM_BASE;
}

static void set_screen() {
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);
    outb(CRT_DATA_REG, ((screen - MEM_BASE) >> 9) & 0Xff);
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);
    outb(CRT_DATA_REG, (screen >> 1) & 0xff);
}

static void get_cursor() {
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    pos = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    pos |= inb(CRT_DATA_REG);
    pos <<= 1;
    pos += MEM_BASE;
}

static void set_cursor() {
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    outb(CRT_DATA_REG, ((pos - MEM_BASE) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    outb(CRT_DATA_REG, ((pos - MEM_BASE) >> 1) & 0xff);
}

static void get_x_y() {
    uint32 delta = (pos - screen) >> 1;
    x = delta % WIDTH;
    y = delta / WIDTH;
}

void console_clear() {
    screen = MEM_BASE;
    pos = MEM_BASE;
    x = y = 0;
    set_cursor();
    set_screen();
    uint16* ptr = (uint16 *)MEM_BASE;
    while (ptr < (uint16 *)MEM_END) {
        *ptr = erase;
        ptr++;
    }
}

static void command_bs() {
    if (x) {
        x--;
        pos -= 2;
        *(uint16 *)pos = erase;
    }
}

static void scroll_up() {
    if (screen + SCR_SIZE + 1 >= MEM_END) {
        memcpy((void *)MEM_BASE, (void *)screen, SCR_SIZE);
        pos -= (screen - MEM_BASE);
        screen = MEM_BASE;
    }
    uint16* ptr = (uint16 *)(screen + SCR_SIZE);
    for (size_t i = 0; i < WIDTH; ++i) {
        *ptr = erase;
        ptr++;
    }
    screen += ROW_SIZE;
    pos += ROW_SIZE;
    set_screen();
}

static void command_lf() {
    if (y + 1 < HEIGHT) {
        y++;
        pos += ROW_SIZE;
        return;
    }
    scroll_up();
}

static void command_cr() {
    pos -= (x << 1);
    x = 0;
}

static void command_del() {
    *(uint16 *)pos = erase;
}

uint32 console_write(void *dev, char *buf, uint32 count) {
    bool itr = interrupt_disable();
    char ch;
    while (count--) {
        ch = *buf;
        switch(ch) {
            case ASCII_NUL: //空字符
                break;
            case ASCII_ENQ: //请求
                break;
            case ASCII_BEL: //\a 蜂鸣
                start_beep();
                break;
            case ASCII_BS: //\b 退格
                command_bs();
                break;
            case ASCII_HT:// \t 水平制表
                break;
            case ASCII_LF:// \n 换行
                command_lf();
                command_cr();
                break;
            case ASCII_VT:// \v 垂直制表
                break;
            case ASCII_FF:// \f 换页
                command_lf();
                break;
            case ASCII_CR:// \r 光标到本行行首
                command_cr();
                break;
            case ASCII_DEL:
                command_del();
                break;
            default:
                if (x >= WIDTH) {
                    command_lf();
                    command_cr();
                }
                *(char*)pos = ch;
                pos++;
                *(char *)pos = attr;
                pos++;
                x++;
                break;
        }
        buf++;
    }
    set_cursor();
    set_interrupt_state(itr);
    return count;
}

void console_init() {
    console_clear();
    device_install(DEV_CHAR, DEV_CONSOLE, NULL, "console", 0, NULL, NULL, console_write);
}