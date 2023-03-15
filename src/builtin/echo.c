#ifdef ONIX
#include "../include/types.h"
#include "../include/stdio.h"
#include "../include/syscall.h"
#include "../include/string.h"
#else
#include <stdio.h>
#include <string.h>
#endif

int main(int argc, char const *argv[])
{
    for (size_t i = 1; i < argc; i++)
    {
        printf(argv[i]);
        if (i < argc - 1)
        {
            printf(" ");
        }
    }
    printf("\n");
    return 0;
}
