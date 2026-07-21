#include "wx_demo.h"
#include "jit.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* ─── W^X (Write XOR Execute) Demonstration ─────────────────────────────────
 *
 * W^X is the policy: a memory page may be writable OR executable, never both.
 *
 * Without W^X, an attacker who can write to any memory page (e.g., via a
 * buffer overflow or UAF) can immediately write shellcode and execute it.
 *
 * With W^X:
 *  - When writing machine code: page is RW (not X)
 *  - When executing machine code: page is RX (not W)
 *  - Transition between states is explicit (mprotect on Linux, VirtualProtect
 *    on Windows) — and must be guarded so only the JIT can call it.
 *
 * JIT compilers MUST violate W^X temporarily (write then execute), but they
 * do so in a controlled window, never leaving pages RWX permanently.
 *
 * Shellcode for the demo: MOV EAX, 42; RET
 *   x86-64: B8 2A 00 00 00  C3
 */

static const uint8_t shellcode[] = {
    0xB8, 0x2A, 0x00, 0x00, 0x00,  /* MOV EAX, 42 */
    0xC3                             /* RET         */
};

void wx_demo_run(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║              B8 — W^X (Write XOR Execute) Demo       ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    printf("Shellcode: MOV EAX, 42; RET\n");
    printf("Bytes: ");
    for (size_t i = 0; i < sizeof(shellcode); i++)
        printf("%02X ", shellcode[i]);
    printf("\n\n");

#ifdef __linux__
    #include <sys/mman.h>
    #include <unistd.h>

    long page_size = sysconf(_SC_PAGESIZE);
    size_t alloc = (size_t)page_size;

    /* ── Step 1: Allocate RW page, write shellcode ── */
    void *page = mmap(NULL, alloc,
                      PROT_READ | PROT_WRITE,   /* NOT executable */
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) { perror("mmap"); return; }

    memcpy(page, shellcode, sizeof(shellcode));
    printf("Step 1: Allocated RW page at %p, wrote shellcode bytes.\n", page);
    printf("        Page is writable but NOT executable.\n\n");

    /* ── Step 2: Attempt to execute from RW page — would segfault ── */
    printf("Step 2: Attempting to execute from a non-executable (RW) page...\n");
    printf("        On a real system with NX/XD bit this would SIGSEGV.\n");
    printf("        We skip the actual call to avoid crashing the demo.\n\n");

    /* ── Step 3: mprotect to RX (W^X transition) ── */
    int rc = mprotect(page, alloc, PROT_READ | PROT_EXEC);
    if (rc != 0) { perror("mprotect"); munmap(page, alloc); return; }
    printf("Step 3: mprotect(PROT_READ|PROT_EXEC) — removed write permission.\n");
    printf("        Page is now executable but NOT writable.\n\n");

    /* ── Step 4: Execute the function ── */
    jit_fn_t fn = (jit_fn_t)page;
    int result = fn();
    printf("Step 4: Executed — returned %d (expected 42). W^X works!\n\n", result);

    /* ── Step 5: Attempt to write to RX page — would segfault ── */
    printf("Step 5: With W^X, writing back to this page would SIGSEGV.\n");
    printf("        Attacker cannot overwrite JIT code after it is marked X.\n\n");

    munmap(page, alloc);

#elif defined(_WIN32)
    /* Windows: VirtualAlloc + VirtualProtect */
    size_t alloc = 4096;
    void *page = VirtualAlloc(NULL, alloc, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!page) { fprintf(stderr, "VirtualAlloc failed\n"); return; }

    memcpy(page, shellcode, sizeof(shellcode));
    printf("Step 1: Allocated RW page at %p, wrote shellcode bytes.\n", page);
    printf("        Page is writable but NOT executable (PAGE_READWRITE).\n\n");

    printf("Step 2: Attempting to call RW page would raise ACCESS_VIOLATION.\n");
    printf("        Skipping to avoid crash.\n\n");

    /* Transition to RX */
    DWORD old_protect;
    VirtualProtect(page, alloc, PAGE_EXECUTE_READ, &old_protect);
    printf("Step 3: VirtualProtect(PAGE_EXECUTE_READ) — removed write permission.\n\n");

    jit_fn_t fn = (jit_fn_t)page;
    int result = fn();
    printf("Step 4: Executed — returned %d (expected 42). W^X works!\n\n", result);
    printf("Step 5: Page is now RX only. Write attempt would ACCESS_VIOLATION.\n\n");

    VirtualFree(page, 0, MEM_RELEASE);

#else
    printf("W^X demo: mmap/VirtualAlloc not available on this platform.\n");
    printf("Concept: pages must be either writable (RW) or executable (RX),\n");
    printf("         never both at the same time.\n\n");
#endif

    /* ── Explanation ── */
    printf("Why W^X matters for JIT compilers:\n");
    printf("  1. JIT writes code to a RW buffer.\n");
    printf("  2. JIT calls mprotect/VirtualProtect: RW -> RX.\n");
    printf("  3. JIT executes the now-RX page.\n");
    printf("  4. To patch code: RX -> RW, patch, RW -> RX again.\n\n");
    printf("This means an attacker who can write to memory CANNOT\n");
    printf("directly execute that memory — they must find a separate\n");
    printf("code-execution primitive (return-to-libc, ROP chains, etc.).\n\n");
    printf("Guard pages between JIT regions prevent spraying across regions.\n");
}
