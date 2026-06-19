/*
 * user_prog.c — ring-3 user-mode demo program
 *
 * This code runs at CPU privilege level 3 (ring 3).  It cannot execute
 * privileged instructions (like `cli`, `hlt`, `lgdt`) or access I/O ports
 * directly — the CPU will raise a #GP if it tries.  The only way to request
 * kernel services is via the syscall gate (int 0x80).
 *
 * This is the first code in BobOS that genuinely runs with reduced privileges.
 */

#include "syscall.h"
#include "user_prog.h"

/* Helper: write a null-terminated string to stdout (fd 1). */
static void uputs(const char *s) {
    int n = 0;
    while (s[n]) n++;
    sys_write(1, s, n);
}

/* Helper: write a small non-negative integer as decimal digits. */
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

    uputs("[ring 3] user_main() started\n");

    /* Demonstrate SYS_GETPID works from ring 3. */
    pid = sys_getpid();
    uputs("[ring 3] sys_getpid() returned PID=");
    uputi(pid);
    uputs("\n");

    /* Demonstrate sys_yield() works — cooperate with the idle process. */
    uputs("[ring 3] calling sys_yield() 3 times...\n");
    for (i = 0; i < 3; i++) {
        sys_yield();
        uputs("[ring 3] resumed after yield\n");
    }

    /* Demonstrate multiple SYS_WRITE calls. */
    uputs("[ring 3] writing a few lines:\n");
    uputs("  line 1\n");
    uputs("  line 2\n");
    uputs("  line 3\n");

    uputs("[ring 3] calling sys_exit(0)...\n");
    sys_exit(0);

    /* Should never reach here — guard loop. */
    for (;;) {}
}
