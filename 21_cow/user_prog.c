/*
 * user_prog.c -- Module 21 Copy-on-Write demo (standalone ring-3 ELF)
 *
 * Forks.  Child writes to shared_val -- triggers a CoW #PF.
 * Parent then reads shared_val -- should still see the original value (42).
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

static void uputs(const char *s) {
    int n = 0;
    while (s[n]) n++;
    syscall(SYS_WRITE, 1, (int)s, n);
}

static void uputi(int v) {
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    if (v == 0) { uputs("0"); return; }
    if (v < 0)  { uputs("-"); v = -v; }
    while (v > 0 && i >= 0) {
        buf[i--] = (char)('0' + (v % 10));
        v /= 10;
    }
    uputs(buf + i + 1);
}

static int shared_val = 42;

void user_main(void) {
    int cpid;

    uputs("[cow] shared_val before fork = 42\n");
    cpid = syscall(SYS_FORK, 0, 0, 0);
    if (cpid == 0) {
        shared_val = 99;   /* write triggers CoW fault */
        uputs("[child] wrote 99 to shared_val\n");
        syscall(SYS_EXIT, 0, 0, 0);
    } else {
        syscall(SYS_WAIT, 0, 0, 0);
        /* Parent should still see 42 */
        uputs("[parent] shared_val = ");
        uputi(shared_val);
        uputs(" (expect 42 -- CoW worked!)\n");
        syscall(SYS_EXIT, 0, 0, 0);
    }
}
