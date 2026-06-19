/*
 * user_prog.c -- Module 22 Demand Paging demo (standalone ring-3 ELF)
 *
 * Calls SYS_MMAP to reserve 4KB.  Writes 0xDEAD to it (triggers demand-page
 * fault -- kernel allocates the physical frame).  Reads it back.
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
#define SYS_MMAP  14

typedef unsigned int uint32_t;

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
    uint32_t addr;
    int *p;

    uputs("[mmap] Requesting 4KB anonymous mapping...\n");
    addr = (uint32_t)syscall(SYS_MMAP, 0, 4096, 3);
    uputs("[mmap] Got address 0x");
    uputh(addr);
    uputs("\n[mmap] Writing 0xDEAD (demand-page fault will fire)...\n");
    p = (int *)addr;
    *p = 0xDEAD;
    uputs("[mmap] Read back: 0x");
    uputh((uint32_t)*p);
    uputs(" (expect 0xDEAD)\n");
    uputs("[mmap] Demand paging works!\n");
    syscall(SYS_EXIT, 0, 0, 0);
}
