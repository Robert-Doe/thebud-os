/*
 * user_prog.c -- Module 19 VFS demo (standalone ring-3 ELF)
 *
 * Opens hello.txt via SYS_OPEN, reads it via SYS_READ, closes via SYS_CLOSE.
 * All output goes via SYS_WRITE to fd 1 (stdout, pre-wired to vga_ops).
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
#define SYS_OPEN   10
#define SYS_CLOSE  11
#define SYS_READ   12

static void uputs(const char *s) {
    int n = 0;
    while (s[n]) n++;
    syscall(SYS_WRITE, 1, (int)s, n);
}

void user_main(void) {
    char buf[64];
    int fd, n;

    uputs("[vfs] Opening hello.txt via SYS_OPEN\n");
    fd = syscall(SYS_OPEN, (int)"hello.txt", 0, 0);
    if (fd < 0) {
        uputs("[vfs] open failed!\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }
    uputs("[vfs] Reading...\n");
    n = syscall(SYS_READ, fd, (int)buf, 63);
    if (n > 0) {
        buf[n] = '\0';
        uputs("[vfs] Content: ");
        uputs(buf);
    }
    syscall(SYS_CLOSE, fd, 0, 0);
    uputs("[vfs] Closed. VFS layer works!\n");
    syscall(SYS_EXIT, 0, 0, 0);
}
