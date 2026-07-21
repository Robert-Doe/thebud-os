/*
 * user_prog.c — standalone ring-3 user program (Module 16)
 *
 * This file is compiled as a SEPARATE ELF binary (user_prog.elf), written to
 * disk.img as "hello.elf" by the kernel on first boot, and loaded at runtime
 * via SYS_EXEC.  It is no longer compiled into the kernel image at all.
 *
 * Linked to load at 0x400000 (USER_BASE) by user_linker.ld.
 * Entry point: user_start (defined in user_entry.asm).
 *
 * System call ABI (same int 0x80 gate):
 *   EAX = syscall number, EBX/ECX/EDX = args, return in EAX.
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
    while (v > 0 && i >= 0) {
        buf[i--] = (char)('0' + (v % 10));
        v /= 10;
    }
    uputs(buf + i + 1);
}

void user_main(void) {
    int pid;
    int i;

    uputs("[ELF] hello from a disk-loaded program!\n");
    uputs("[ELF] this binary was read from BobFS at runtime.\n");

    pid = syscall(SYS_GETPID, 0, 0, 0);
    uputs("[ELF] my PID = ");
    uputi(pid);
    uputs("\n");

    uputs("[ELF] yielding 3 times...\n");
    for (i = 0; i < 3; i++) {
        syscall(SYS_YIELD, 0, 0, 0);
        uputs("[ELF] resumed\n");
    }

    uputs("[ELF] calling sys_exit(0) -- goodbye.\n");
    syscall(SYS_EXIT, 0, 0, 0);

    for (;;) {}
}
