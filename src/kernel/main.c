/*
 * @Author: yuxintao 1921056015@qq.com
 * @Date: 2022-10-21 11:29:42
 * @LastEditors: yuxintao 1921056015@qq.com
 * @LastEditTime: 2022-10-21 12:12:22
 * @FilePath: /onix_study/src/kernel/main.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "../include/interrupt.h"
#include "../include/clock.h"
#include "../include/time.h"
#include "../include/rtc.h"
#include "../include/memory.h"
#include "../include/gate.h"
#include "../include/syscall.h"
#include "../include/keyboard.h"
#include "../include/tasks.h"
#include "../include/arena.h"
#include "../include/ide.h"
#include "../include/buffer.h"
#include "../include/fs.h"
#include "../include/ramdisk.h"
#include "../include/serial.h"

void kernel_init() {
    memory_map_init();//使用物理内存数组管理空闲页
    mapping_init();//开启分页机制
    arena_init();    

    interrupt_init();
    clock_init();
    keyboard_init();
    syscall_init();
    time_init();//时间不准
    serial_init();
    // rtc_init();//设置闹钟中断不会触发
    ide_init();
    ramdisk_init();
    buffer_init();
    file_init();
    inode_init();
    super_init();
    
    task_init();
    set_interrupt_state(true);
    return;
}