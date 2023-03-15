#ifndef _STRING_H_
#define _STRING_H_

#include "types.h"

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t count);
char *strcat(char *dest, const char *src);
size_t strlen(const char *str);
int strcmp(const char *lhs, const char *rhs);
char *strchr(const char *str, int ch);
char *strrchr(const char *str, int ch);

int memcmp(const void *lhs, const void *rhs, size_t count);
void *memset(void *dest, int ch, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memchr(const void *ptr, int ch, size_t count);

#define SEPARATOR1 '/'  //目录分隔符 1
#define SEPARATOR2 '\\' //目录分隔符 2
#define IS_SEPARATOR(c) (c == SEPARATOR1 || c == SEPARATOR2) //字符是否是目录分隔符

// 获取最后一个分隔符
char *strrsep(const char *str);
// 获取第一个分隔符
char *strsep(const char *str);

#endif