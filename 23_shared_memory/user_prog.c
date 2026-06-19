/*
 * user_prog.c -- Module 23 Shared Memory demo (standalone ring-3 ELF)
 *
 * Parent creates segment (key=42), maps it at 0xD00000, writes 0xCAFE.
 * Forks.  Child attaches same segment at 0xD00000 and reads 0xCAFE, then
 * writes 0xBEEF.  Parent waits, reads 0xBEEF.
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
#define SYS_FORK   6
#define SYS_WAIT   7
#define SYS_SHMGET 15
#define SYS_SHMAT  16

typedef unsigned int uint32_t;
typedef volatile unsigned int vuint32_t;

static void uputs(const char *s) {
    int n = 0;
    while (s[n]) n++;
    syscall(SYS_WRITE, 1, (int)s, n);
}

static void uputh(uint32_t v) {
    char buf[9];
    int i;
    buf[8] = '\0';
    for (i = 7; i >= 0; i--) {
        int nibble = v & 0xF;
        buf[i] = (char)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
        v >>= 4;
    }
    uputs(buf);
}

void user_main(void) {
    int shm_id, cpid;
    vuint32_t *shared;

    uputs("[shm] Creating shared memory segment (key=42)...\n");
    shm_id = syscall(SYS_SHMGET, 42, 4096, 0);
    shared = (vuint32_t *)syscall(SYS_SHMAT, shm_id, 0xD00000, 0);

    uputs("[shm] Mapped at 0xD00000. Writing 0xCAFE...\n");
    *shared = 0xCAFEu;

    cpid = syscall(SYS_FORK, 0, 0, 0);
    if (cpid == 0) {
        /* Child: attach same segment and read/write */
        syscall(SYS_SHMAT, shm_id, 0xD00000, 0);
        uputs("[child] Read from shared: 0x");
        uputh((uint32_t)*shared);
        uputs(" (expect 0xCAFE)\n");
        *shared = 0xBEEFu;
        syscall(SYS_EXIT, 0, 0, 0);
    } else {
        syscall(SYS_WAIT, 0, 0, 0);
        uputs("[parent] After child write: 0x");
        uputh((uint32_t)*shared);
        uputs(" (expect 0xBEEF -- shared memory works!)\n");
        syscall(SYS_EXIT, 0, 0, 0);
    }
}
