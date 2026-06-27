/*
 * user_prog.c -- Module 24 Threads demo (standalone ring-3 ELF)
 *
 * Creates a worker thread in the same address space.  Worker increments a
 * shared counter 5 times, yielding between each increment.  Main thread
 * yields to let worker run, then reads the shared counter.
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

#define SYS_EXIT          1
#define SYS_WRITE         2
#define SYS_YIELD         4
#define SYS_THREAD_CREATE 17
#define SYS_THREAD_EXIT   18

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

/* Static user stack for the worker thread (in shared address space) */
static char worker_stack[4096];
static volatile int counter = 0;

static void worker(void) {
    int i;
    for (i = 0; i < 5; i++) {
        counter++;
        syscall(SYS_YIELD, 0, 0, 0);
    }
    uputs("[thread] worker done. counter=");
    uputi(counter);
    uputs("\n");
    syscall(SYS_THREAD_EXIT, 0, 0, 0);
}

void user_main(void) {
    int tid;

    uputs("[thread] Creating worker thread...\n");
    tid = syscall(SYS_THREAD_CREATE,
                  (int)worker,
                  (int)(worker_stack + sizeof(worker_stack)),
                  0);
    uputs("[thread] Worker TID=");
    uputi(tid);
    uputs(". Yielding for it to run:\n");
    syscall(SYS_YIELD, 0, 0, 0);
    syscall(SYS_YIELD, 0, 0, 0);
    syscall(SYS_YIELD, 0, 0, 0);
    syscall(SYS_YIELD, 0, 0, 0);
    syscall(SYS_YIELD, 0, 0, 0);
    uputs("[main] counter=");
    uputi(counter);
    uputs(" (shared with worker, no fork)\n");
    uputs("[thread] Shared address space threads work!\n");
    syscall(SYS_EXIT, 0, 0, 0);
}
