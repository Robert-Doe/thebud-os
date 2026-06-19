/*
 * user_prog.c — Module 18 signal demo (standalone ring-3 ELF)
 *
 * Part A — SIGUSR1 (asynchronous user signal):
 *   1. Register SIGUSR1 handler.
 *   2. sys_kill(my_pid, SIGUSR1) — sends to self.
 *   3. sys_yield — scheduler_tick delivers SIGUSR1 before returning ring-3.
 *   4. Execution resumes after yield; handler message was printed.
 *
 * Part B — SIGSEGV recovery via user handler:
 *   Fork.  Child registers a SIGSEGV handler and touches address 0x1 (unmapped).
 *   The kernel's process_fault_handler delivers SIGSEGV to the user handler
 *   instead of killing the process outright.  Handler calls exit(0).
 *   Parent waits and confirms child exited cleanly.
 *
 * Part C — baseline fork/wait to confirm nothing regressed.
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
#define SYS_SIGNAL 8
#define SYS_KILL   9

#define SIGUSR1 10
#define SIGSEGV 11

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

static void sigusr1_handler(int sig) {
    (void)sig;
    uputs("  [handler] SIGUSR1 caught at ring 3 -- signal delivery works!\n");
}

static void sigsegv_handler(int sig) {
    (void)sig;
    uputs("  [handler] SIGSEGV caught -- process_fault_handler delivered the signal.\n");
    uputs("  [handler] Can't safely return (would re-fault), calling exit(0).\n");
    syscall(SYS_EXIT, 0, 0, 0);
}

void user_main(void) {
    int pid = syscall(SYS_GETPID, 0, 0, 0);
    int cpid, status;

    uputs("\n[demo] PID=");
    uputi(pid);
    uputs("\n");

    /* ── Part A: SIGUSR1 ─────────────────────────────────────────────── */

    uputs("\n--- Part A: SIGUSR1 ---\n");
    syscall(SYS_SIGNAL, SIGUSR1, (int)sigusr1_handler, 0);
    uputs("[demo] SIGUSR1 handler registered.\n");

    syscall(SYS_KILL, pid, SIGUSR1, 0);
    uputs("[demo] Sent SIGUSR1 to self.  Yielding for delivery:\n");
    syscall(SYS_YIELD, 0, 0, 0);
    /* handler fired during the yield, printed its line, returned */
    uputs("[demo] Resumed after yield -- handler returned cleanly.\n");

    /* ── Part B: SIGSEGV recovery ────────────────────────────────────── */

    uputs("\n--- Part B: SIGSEGV recovery ---\n");
    cpid = syscall(SYS_FORK, 0, 0, 0);

    if (cpid == 0) {
        syscall(SYS_SIGNAL, SIGSEGV, (int)sigsegv_handler, 0);
        uputs("[child] SIGSEGV handler registered.  Touching 0x1...\n");
        __asm__ volatile ("movl (1), %%eax" : : : "eax");  /* faults here */
        uputs("[child] ERROR: should not reach this line.\n");
        syscall(SYS_EXIT, 1, 0, 0);

    } else if (cpid > 0) {
        uputs("[parent] Waiting for SIGSEGV child...\n");
        status = syscall(SYS_WAIT, 0, 0, 0);
        uputs("[parent] Child exited with code ");
        uputi(status);
        uputs(" (expect 0)\n");
    } else {
        uputs("fork failed\n");
        syscall(SYS_EXIT, 1, 0, 0);
    }

    /* ── Part C: baseline fork / wait ───────────────────────────────── */

    uputs("\n--- Part C: baseline fork/wait ---\n");
    cpid = syscall(SYS_FORK, 0, 0, 0);

    if (cpid == 0) {
        uputs("[child] exiting with 77\n");
        syscall(SYS_EXIT, 77, 0, 0);
    } else if (cpid > 0) {
        status = syscall(SYS_WAIT, 0, 0, 0);
        uputs("[parent] child exited with code ");
        uputi(status);
        uputs(" (expect 77)\n");
    }

    uputs("\n[demo] All signals work.  Exiting.\n");
    syscall(SYS_EXIT, 0, 0, 0);
}
