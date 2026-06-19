/*
 * user_prog.c — Module 17 fork/wait demo (standalone ring-3 ELF)
 *
 * Demonstrates the full fork/wait lifecycle:
 *
 *   1. Process calls SYS_FORK.
 *   2. Kernel deep-copies address space into a new PD.
 *   3. Both parent and child resume after the fork() call.
 *   4. Child (fork returns 0): prints, exits with code 42.
 *   5. Parent (fork returns child PID): calls SYS_WAIT, blocks until
 *      child exits, receives exit code 42 in EAX.
 *   6. Parent prints the reaped exit code and exits cleanly.
 *
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

#define SYS_EXIT   1
#define SYS_WRITE  2
#define SYS_GETPID 3
#define SYS_YIELD  4
#define SYS_FORK   6
#define SYS_WAIT   7

static void uputs(const char *s) {
    int n = 0;
    while (s[n]) n++;
    syscall(SYS_WRITE, 1, (int)s, n);
}

static void uputi(int v) {
    char buf[12];
    int  i = 10;
    buf[11] = '\0';
    if (v == 0) { uputs("0"); return; }
    if (v < 0)  { uputs("-"); v = -v; }
    while (v > 0 && i >= 0) {
        buf[i--] = (char)('0' + (v % 10));
        v /= 10;
    }
    uputs(buf + i + 1);
}

void user_main(void) {
    int pid  = syscall(SYS_GETPID, 0, 0, 0);
    int cpid;

    uputs("[user] process started, PID=");
    uputi(pid);
    uputs("\n");

    uputs("[user] calling fork()...\n");
    cpid = syscall(SYS_FORK, 0, 0, 0);

    if (cpid == 0) {
        /* ── Child ── */
        int my_pid = syscall(SYS_GETPID, 0, 0, 0);
        uputs("[child] I am the child, PID=");
        uputi(my_pid);
        uputs("\n");
        uputs("[child] doing some work...\n");
        syscall(SYS_YIELD, 0, 0, 0);
        syscall(SYS_YIELD, 0, 0, 0);
        uputs("[child] exiting with code 42\n");
        syscall(SYS_EXIT, 42, 0, 0);

    } else if (cpid > 0) {
        /* ── Parent ── */
        int status;
        uputs("[parent] forked child PID=");
        uputi(cpid);
        uputs(", calling wait()...\n");

        status = syscall(SYS_WAIT, 0, 0, 0);

        uputs("[parent] child exited with code ");
        uputi(status);
        uputs("\n");
        uputs("[parent] exiting cleanly.\n");
        syscall(SYS_EXIT, 0, 0, 0);

    } else {
        uputs("[user] fork() failed!\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }

    for (;;) {}
}
