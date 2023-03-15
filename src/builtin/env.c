#ifdef ONIX
#include "../include/types.h"
#include "../include/stdio.h"
#include "../include/syscall.h"
#include "../include/string.h"
#else
#include <stdio.h>
#include <string.h>
#endif

int main(int argc, const char *argv[], const char *envp[]) {
    for (size_t i = 0; i < argc; ++i) {
        printf("%s\n", argv[i]);
    }
    
    for (size_t i = 0; 1; ++i) {
        if (!envp[i]) {
            break;
        }
        int len = strlen(envp[i]);
        if (len >= 1022)
            continue;
        printf("%s\n", envp[i]);
    }
    return 0;
}