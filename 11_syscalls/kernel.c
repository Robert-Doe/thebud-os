/*
 * kernel.c — Module 11: System Calls
 *
 * What this proves
 * ────────────────
 * Three worker processes issue `int 0x80` to call kernel services through
 * the syscall interface.  The demo exercises all four syscalls:
 *
 *   SYS_GETPID  — each process asks the kernel for its own PID.
 *   SYS_WRITE   — each process writes lines of text to the VGA console.
 *   SYS_YIELD   — each process voluntarily gives up its time slice after
 *                 writing, demonstrating cooperative hand-off alongside the
 *                 preemptive timer.
 *   SYS_EXIT    — each process terminates itself when its work is done.
 *
 * The key observation: the processes never call vga_putchar() or any kernel
 * function directly.  All interaction goes through `int 0x80`, exactly as
 * a real user-space program would.  The output appearing on screen proves
 * the full path works end-to-end.
 *
 * Screen layout
 * ─────────────
 *   Rows  0-12 : boot messages
 *   Row  13    : separator
 *   Row  14+   : syscall output from worker processes (scrolls naturally)
 */

#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "pic.h"
#include "idt.h"
#include "isr.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"

extern char kernel_end;

/* ── Tiny string helpers for processes (no libc) ─────────────────────── */

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

/* Write a decimal uint into buf (no leading zeros except for 0 itself).
 * Returns number of characters written. buf must be at least 12 bytes. */
static int uint_to_str(uint32_t val, char *buf) {
    char tmp[12];
    int i = 0, len;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    while (val > 0) { tmp[i++] = '0' + (char)(val % 10); val /= 10; }
    len = i;
    int j;
    for (j = 0; j < len; j++) buf[j] = tmp[len - 1 - j];
    buf[len] = '\0';
    return len;
}

/* Concatenate src onto dst in-place, return new length. */
static int str_cat(char *dst, int pos, const char *src) {
    while (*src) dst[pos++] = *src++;
    dst[pos] = '\0';
    return pos;
}

/* ── Worker processes ────────────────────────────────────────────────── */

/*
 * Each worker:
 *   1. Calls SYS_GETPID  to learn its own PID.
 *   2. Prints N lines via SYS_WRITE, each containing the PID and line number.
 *   3. Calls SYS_YIELD between lines to share the CPU cooperatively.
 *   4. Calls SYS_EXIT when done.
 *
 * All inter-process communication with the kernel uses `int 0x80` via the
 * syscall() inline helper from syscall.h.
 */
#define WORKER_LINES  4   /* how many lines each worker prints before exiting */

static void worker(void) {
    char   buf[64];
    char   num[12];
    int    pos;
    int    i;
    uint32_t pid = (uint32_t)sys_getpid();

    for (i = 1; i <= WORKER_LINES; i++) {
        /* Build: "  [PID X] line Y\n" */
        pos = 0;
        pos = str_cat(buf, pos, "  [PID ");
        uint_to_str(pid, num);
        pos = str_cat(buf, pos, num);
        pos = str_cat(buf, pos, "] writing line ");
        uint_to_str((uint32_t)i, num);
        pos = str_cat(buf, pos, num);
        pos = str_cat(buf, pos, " of " );
        uint_to_str(WORKER_LINES, num);
        pos = str_cat(buf, pos, num);
        pos = str_cat(buf, pos, "\n");

        sys_write(1, buf, pos);   /* SYS_WRITE — sends output to VGA */
        sys_yield();              /* SYS_YIELD — cooperative hand-off */
    }

    /* Build exit message. */
    pos = 0;
    pos = str_cat(buf, pos, "  [PID ");
    uint_to_str(pid, num);
    pos = str_cat(buf, pos, num);
    pos = str_cat(buf, pos, "] done -- calling SYS_EXIT\n");
    sys_write(1, buf, pos);

    sys_exit(0);   /* SYS_EXIT — scheduler switches to next process */
    for (;;) {}    /* unreachable; suppresses "no-return" warning */
}

/* ── Kernel entry ────────────────────────────────────────────────────── */

void kernel_main(void) {
    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 11 -- System Calls\n\n");

    gdt_init();
    kprintf("  GDT installed\n");

    pic_init();
    pic_clear_mask(0);
    kprintf("  PIC remapped, IRQ0 (timer) unmasked\n");

    idt_init();
    kprintf("  IDT installed  (vector 0x80 = syscall gate, DPL=3)\n");

    pmm_init(64u * 1024u * 1024u, (uint32_t)&kernel_end);
    kprintf("  PMM: %u free pages\n", pmm_get_free());

    paging_init();
    kprintf("  Paging ON\n");

    heap_init(64);
    kprintf("  Heap: %u KB\n", (64u * 4096u) / 1024u);

    keyboard_init();
    pic_clear_mask(1);
    kprintf("  Keyboard ready\n");

    syscall_init();
    kprintf("  Syscall gate installed  (int 0x80)\n\n");

    __asm__ volatile ("sti");

    process_init();
    kprintf("  Scheduler ready  (idle PID %u)\n", process_current_pid());

    /* Spawn three identical workers.  They each have their own stack so
     * their local variables (pid, buf, i) are independent. */
    uint32_t p1 = process_create(worker);
    uint32_t p2 = process_create(worker);
    uint32_t p3 = process_create(worker);
    kprintf("  Spawned workers: PID %u, %u, %u\n", p1, p2, p3);

    kprintf("\n");
    kprintf("  --- syscall output below (all via int 0x80) ---\n\n");

    /* Idle loop.  Workers will run, print, yield, and eventually exit.
     * When all three are dead the scheduler returns here on every tick. */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
