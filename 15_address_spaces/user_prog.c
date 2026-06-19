/*
 * user_prog.c — ring-3 user-mode demo program
 *
 * Module 15 additions:
 *   After demonstrating normal syscall use, user_main() deliberately reads
 *   from physical address 0x90000 — the kernel stack region.  That page is
 *   supervisor-only (U/S=0) in the ring-3 page directory, so the MMU raises
 *   a #PF.  The kernel's user_fault_hook prints a message and calls
 *   process_exit() to kill this process gracefully, proving that memory
 *   isolation is enforced.
 */

#include "syscall.h"
#include "user_prog.h"

static void uputs(const char *s) {
    int n = 0;
    while (s[n]) n++;
    sys_write(1, s, n);
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
    volatile uint32_t *forbidden;

    uputs("[ring 3] user_main() started\n");

    pid = sys_getpid();
    uputs("[ring 3] sys_getpid() returned PID=");
    uputi(pid);
    uputs("\n");

    uputs("[ring 3] calling sys_yield() 3 times...\n");
    for (i = 0; i < 3; i++) {
        sys_yield();
        uputs("[ring 3] resumed after yield\n");
    }

    uputs("[ring 3] writing a few lines:\n");
    uputs("  line 1\n");
    uputs("  line 2\n");
    uputs("  line 3\n");

    /* ----------------------------------------------------------------
     * Isolation test: try to read 0x90000 (kernel stack — supervisor-only).
     * This should fault immediately, kill this process, and switch to the
     * next ready process.  The lines after this point are never executed.
     * ---------------------------------------------------------------- */
    uputs("[ring 3] ISOLATION TEST: reading 0x90000 (supervisor-only)...\n");
    forbidden = (volatile uint32_t *)0x90000u;
    (void)*forbidden;   /* ← triggers #PF here */

    /* Never reached */
    uputs("[ring 3] ERROR: should have been killed by #PF handler\n");
    sys_exit(1);
    for (;;) {}
}
