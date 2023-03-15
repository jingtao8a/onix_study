#include "../include/types.h"
#include "../include/syscall.h"
#include "../include/stdlib.h"
#include "../include/assert.h"
#include "../include/string.h"
#include "../include/fs.h"
#include "../include/stdio.h"
#include "../include/time.h"

#define MAX_CMD_LEN 256
#define MAX_ARG_NR 16
#define MAX_PATH_LEN 1024
#define BUFFLEN 1024

static char cwd[MAX_PATH_LEN];//当前路径
static char cmd[MAX_CMD_LEN];
static char *argv[MAX_ARG_NR];//解析的cmd 参数存放
static char buf[BUFFLEN];

static char *envp[] = {
    "HOME=/",
    "PATH=/bin",
    NULL,
};

static int cmd_parse(char *cmd, char *argv[]) {
    assert(cmd != NULL);

    char *next = cmd;
    int argc = 0;
    int quot = false;//引用
    while (*next && argc < MAX_ARG_NR) {
        while (*next == ' ' || (quot && *next != '"')) {
            next++;
        }
        if (*next == 0) {//到末尾
            break;
        }
        if (*next == '"') {
            quot = !quot;
            if (quot) {
                next++;
                argv[argc++] = next;
            } else {
                *next = 0;
                next++;
            }
            continue;
        }

        argv[argc++] = next;
        while (*next && *next != ' ') {
            next++;
        }

        if (*next) {
            *next = 0;
            next++;
        }
    }
    argv[argc] = NULL;
    return argc;
}

static char *basename(char *name) {
    char *ptr = strrsep(name);
    if (ptr[1]) {
        ptr++;
        return ptr;
    }
    return ptr;
}

//打印提示符
static void print_prompt() {
    getcwd(cwd, MAX_PATH_LEN);
    char *ptr = strrsep(cwd);//ptr指向cwd最后一个分隔符的位置
    if (ptr != cwd) {
        *ptr = 0;
    }
    char *base = basename(cwd);
    printf("[root %s]# ", base);
}

static void builtin_test(int argc, char *argv[]) {

}

static void builtin_clear() {
    clear();
}

static void builtin_pwd() {
    getcwd(cwd, MAX_PATH_LEN);
    printf("%s\n", cwd);
}
static void strftime(time_t stamp, char *buf)
{
    tm time;
    localtime(stamp, &time);
    sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d",
            time.tm_year + 1900,
            time.tm_mon,
            time.tm_mday,
            time.tm_hour,
            time.tm_min,
            time.tm_sec);
}

static void builtin_cd(int argc, char *argv[]) {
    chdir(argv[1]);
}

static void builtin_mkdir(int argc, char *argv[]) {
    if (argc < 2) {
        return;
    }
    mkdir(argv[1], 0755);
}

static void builtin_rmdir(int argc, char *argv[]) {
    if (argc < 2) {
        return;
    }
    rmdir(argv[1]);
}

static void builtin_rm(int argc, char *argv[]) {
    if (argc < 2) {
        return;
    }
    unlink(argv[1]);
}

static void builtin_touch(int argc, char *argv[]) {
    if (argc < 2) {
        return;
    }
    fd_t fd = creat(argv[1], 0755);
    close(fd);
}

static void builtin_date(int argc, char *argv[])
{
    strftime(time(), buf);
    printf("%s\n", buf);
}

static void builtin_mount(int argc, char *argv[]) {
    if (argc < 3) {
        return;
    }
    mount(argv[1], argv[2], 0);
}

static void builtin_umount(int argc, char *argv[]) {
    if (argc < 2) {
        return;
    }
    umount(argv[1]);
}

static void builtin_mkfs(int argc, char *argv[]) {
    if (argc < 2) {
        return;
    }
    mkfs(argv[1], 0);
}

static int dupfile(int argc, char **argv, fd_t dupfd[3])
{
    for (size_t i = 0; i < 3; i++)
    {
        dupfd[i] = EOF;
    }

    int outappend = 0;
    int errappend = 0;

    char *infile = NULL;
    char *outfile = NULL;
    char *errfile = NULL;

    for (size_t i = 0; i < argc; i++)//遍历命令行参数
    {
        if (!strcmp(argv[i], "<") && (i + 1) < argc)
        {
            infile = argv[i + 1];
            argv[i] = NULL;
            i++;
            continue;
        }
        if (!strcmp(argv[i], ">") && (i + 1) < argc)
        {
            outfile = argv[i + 1];
            argv[i] = NULL;
            i++;
            continue;
        }
        if (!strcmp(argv[i], ">>") && (i + 1) < argc)
        {
            outfile = argv[i + 1];
            argv[i] = NULL;
            outappend = O_APPEND;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "2>") && (i + 1) < argc)
        {
            errfile = argv[i + 1];
            argv[i] = NULL;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "2>>") && (i + 1) < argc)
        {
            errfile = argv[i + 1];
            argv[i] = NULL;
            errappend = O_APPEND;
            i++;
            continue;
        }
    }

    if (infile != NULL)
    {
        fd_t fd = open(infile, O_RDONLY | outappend | O_CREAT, 0755);
        if (fd == EOF)
        {
            printf("open file %s failure\n", infile);
            goto rollback;
        }
        dupfd[0] = fd;
    }
    if (outfile != NULL)
    {
        fd_t fd = open(outfile, O_WRONLY | outappend | O_CREAT, 0755);
        if (fd == EOF)
        {
            printf("open file %s failure\n", outfile);
            goto rollback;
        }
        dupfd[1] = fd;
    }
    if (errfile != NULL)
    {
        fd_t fd = open(errfile, O_WRONLY | errappend | O_CREAT, 0755);
        if (fd == EOF)
        {
            printf("open file %s failure\n", errfile);
            goto rollback;
        }
        dupfd[2] = fd;
    }
    return 0;

rollback:
    for (size_t i = 0; i < 3; i++)
    {
        if (dupfd[i] != EOF)
        {
            close(dupfd[i]);
        }
    }
    return EOF;
}

static pid_t builtin_command(char *filename, char *argv[], fd_t infd, fd_t outfd, fd_t errfd) {
    int status;
    pid_t pid = fork();
    if (pid) {//父进程
        if (infd != EOF) {
            close(infd);
        }
        if (outfd != EOF) {
            close(outfd);
        }
        if (errfd != EOF) {
            close(errfd);
        }
        return pid;
    }
    //子进程
    if (infd != EOF) {
        fd_t fd = dup2(infd, STDIN_FILENO);
        close(infd);
    }
    if (outfd != EOF) {
        fd_t fd = dup2(outfd, STDOUT_FILENO);
        close(outfd);
    }
    if (errfd != EOF) {
        fd_t fd = dup2(errfd, STDERR_FILENO);
        close(errfd);
    }
    int i = execve(filename, argv, envp);
    //正常情况下不执行到这里
    exit(i);
}

static void builtin_exec(int argc, char *argv[])
{
    bool p = true;
    int status;

    char **bargv = NULL;
    char *name = buf;

    fd_t dupfd[3];
    if (dupfile(argc, argv, dupfd) == EOF)
        return;

    fd_t infd = dupfd[0];
    fd_t pipefd[2];
    int count = 0;

    for (int i = 0; i < argc; i++) {
        if (!argv[i]) {
            continue;
        }
        if (!p && !strcmp(argv[i], "|")) {
            argv[i] = NULL;
            int ret = pipe(pipefd);
            builtin_command(name, bargv, infd, pipefd[1], EOF);
            count++;
            infd = pipefd[0];
            int len = strlen(name) + 1;
            name += len;
            p = true;
            continue;
        }
        if (!p) {
            continue;
        }

        stat_t statbuf;
        sprintf(name, "/bin/%s.out", argv[i]);
        if (stat(name, &statbuf) == EOF) {
            printf("osh: command not found: %s\n", argv[i]);
            return;
        }
        bargv = &argv[i + 1];
        p = false;
    }

    int pid = builtin_command(name, bargv, infd, dupfd[1], dupfd[2]);
    for (size_t i = 0; i <= count; i++) {
        pid_t child = waitpid(-1, &status);
        // printf("child %d exit\n", child);
    }
}

static void execute(int argc, char *argv[]) {
    char *line = argv[0];
    if (!strcmp(line, "test")) {
        return builtin_test(argc, argv);
    }
    if (!strcmp(line, "pwd")) {
        return builtin_pwd();
    }
    if (!strcmp(line, "clear")) {
        return builtin_clear();
    }
    if (!strcmp(line, "exit")) {
        int code = 0;
        if (argc == 2) {
            code = atoi(argv[1]);//进程exit的状态码
        }
        exit(code);
        assert("never reach");
    }
    if (!strcmp(line, "cd")) {
        return builtin_cd(argc, argv);
    }
    if (!strcmp(line, "mkdir"))
    {
        return builtin_mkdir(argc, argv);
    }
    if (!strcmp(line, "rmdir"))
    {
        return builtin_rmdir(argc, argv);
    }
    if (!strcmp(line, "rm"))
    {
        return builtin_rm(argc, argv);
    }
    if (!strcmp(line, "touch")) 
    {
        return builtin_touch(argc, argv);
    }
    if (!strcmp(line, "date"))
    {
        return builtin_date(argc, argv);
    }
    if (!strcmp(line, "mount")) 
    {
        return builtin_mount(argc, argv);
    }
    if (!strcmp(line, "umount"))
    {
        return builtin_umount(argc, argv);
    }
    if (!strcmp(line, "mkfs"))
    {
        return builtin_mkfs(argc, argv);
    }
    return builtin_exec(argc, argv);
}

static void readline(char *buf, uint32 count) {
    assert(buf != NULL);
    memset(buf, 0, count);

    char *ptr = NULL;
    uint32 idx = 0;
    char ch = 0;
    while (idx < count) {
        ptr = buf + idx;
        int ret = read(STDIN_FILENO, ptr, 1);
        if (ret == -1) {//读错误
            *ptr = 0;
            return;
        }
        switch(*ptr) {
            case '\n':
            case '\r':
                *ptr = 0;
                ch = '\n';
                write(STDOUT_FILENO, &ch, 1);//换行
                return;
            case '\b':
                if (buf[0] != '\b') {
                    idx--;
                    ch = '\b';
                    write(STDOUT_FILENO, &ch, 1);//退格
                }
                break;
            default:
                write(STDOUT_FILENO, ptr, 1);
                idx++;
                break;
        }
    }
}


int main() {
    memset(cmd, 0, sizeof(cmd));
    memset(cwd, 0, sizeof(cwd));

    builtin_clear();

    while (true) {
        print_prompt();
        readline(cmd, sizeof(cmd));
        if (cmd[0] == 0) {
            continue;
        }
        int argc = cmd_parse(cmd, argv);
        if (argc < 0 || argc >= MAX_ARG_NR) {
            continue;
        }
        execute(argc, argv);
    }
    return 0;
}