/*
 * @Author: yuxintao 1921056015@qq.com
 * @Date: 2022-10-21 12:17:45
 * @LastEditors: yuxintao 1921056015@qq.com
 * @LastEditTime: 2022-10-21 13:03:55
 * @FilePath: /onix_study/src/include/types.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef _TYPES_H_
#define _TYPES_H_

#include "onix.h"

#define EOF -1 //end of file
#define EOS 0
#define NULL (void*)0

#define CONTACT(x, y) x##y
#define RESERVED_TOKEN(x, y) CONTACT(x, y)
#define RESERVED RESERVED_TOKEN(reserved, __LINE__)

#ifndef __cplusplus
#define bool _Bool
#define true 1
#define false 0
#endif


#define weak __attribute__((__weak__))//标记弱符号
#define noreturn __attribute__((__noreturn))//标记不会返回的函数
#define _packed  __attribute__((packed))//用于定义特殊的结构体
#define _ofp __attribute__((optimize("omit-frame-pointer"))) //省略函数的栈帧
#define _inline __attribute__((always_inline)) inline

typedef unsigned int size_t;
typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef int32 pid_t;

typedef uint32 time_t;

typedef enum std_fd_t {
    STDIN_FILENO,//0
    STDOUT_FILENO,//1
    STDERR_FILENO,//2
} std_fd_t;

typedef uint32 fd_t;
typedef uint16 mode_t;
typedef uint32 off_t;
#endif