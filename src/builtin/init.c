#include "../include/syscall.h"
#include "../include/stdio.h"

static void user_init_thread() {
    //进入ring0
    while (true) {
        uint32 status;
        pid_t pid = fork();
        if (pid) {//父进程
            pid_t child = waitpid(pid, &status);
            printf("wait pid %d status %d\n", child, status);
        } else {//子进程
            int err = execve("/bin/osh.out", NULL, NULL);
            printf("execve /bin/osh.out error %d\n", err);
            exit(err);
        }
    }
}

int main() {
    if (getpid() != 1) {
        printf("init already running ...\n");
        return 0;
    }
    user_init_thread();
    return 0;
}