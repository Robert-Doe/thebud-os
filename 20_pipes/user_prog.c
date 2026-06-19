/*
 * user_prog.c -- Module 20 Pipes & IPC demo (standalone ring-3 ELF)
 *
 * Forks.  Child writes "hello via pipe!" to the write end.
 * Parent reads from the read end and prints it.
 * No libc.  No kernel headers.  Only int $0x80.
 */

static inline int syscall(int num, int a, int b, int c) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (num), "b" (a), "c" (b), "d" (c)
        : "memory"
    );
    return ret;
}

#define SYS_EXIT  1
#define SYS_WRITE 2
#define SYS_FORK  6
#define SYS_WAIT  7
#define SYS_CLOSE 11
#define SYS_READ  12
#define SYS_PIPE  13

static void uputs(const char *s) {
    int n = 0;
    while (s[n]) n++;
    syscall(SYS_WRITE, 1, (int)s, n);
}

void user_main(void) {
    int fds[2];
    int cpid, n;
    char buf[32];
    char msg[] = "hello via pipe!";

    uputs("[pipe] Creating pipe...\n");
    syscall(SYS_PIPE, (int)fds, 0, 0);

    cpid = syscall(SYS_FORK, 0, 0, 0);
    if (cpid == 0) {
        /* child: write to pipe */
        syscall(SYS_CLOSE, fds[0], 0, 0);   /* close read end */
        syscall(SYS_WRITE, fds[1], (int)msg, 15);
        syscall(SYS_CLOSE, fds[1], 0, 0);
        syscall(SYS_EXIT, 0, 0, 0);
    } else {
        /* parent: read from pipe, print */
        syscall(SYS_CLOSE, fds[1], 0, 0);   /* close write end */
        n = syscall(SYS_READ, fds[0], (int)buf, 31);
        buf[n] = '\0';
        uputs("[pipe] Read from child: ");
        uputs(buf);
        uputs("\n");
        syscall(SYS_WAIT, 0, 0, 0);
        uputs("[pipe] Pipes work!\n");
        syscall(SYS_EXIT, 0, 0, 0);
    }
}
