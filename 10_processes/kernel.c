/*
 * kernel.c — Module 10: Processes & Scheduler demo
 *
 * What this proves
 * ────────────────
 * Three kernel processes run "simultaneously" via preemptive round-robin
 * scheduling driven by the PIT timer (IRQ0, ~18 Hz).  Each process spins
 * in a tight counting loop and every ~8 million iterations updates one row
 * of the VGA display with its counter value.  Because the scheduler
 * preempts each process on every timer tick, all three counters advance at
 * similar rates — interleaved execution is visible without any sleep or yield.
 *
 * Screen layout
 * ─────────────
 *   Rows  0-16 : boot messages
 *   Row  17    : separator
 *   Row  18    : demo header
 *   Row  19    : blank
 *   Row  20    : Process 1 live counter + spinner
 *   Row  21    : Process 2 live counter + spinner
 *   Row  22    : Process 3 live counter + spinner
 *   Row  24    : hint
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

extern char kernel_end;

/* ── Direct VGA helpers that bypass the kprintf cursor ───────────────── */

static volatile unsigned char *VGA = (volatile unsigned char *)0xB8000;

static void vga_put_at(int row, int col, char c, unsigned char color) {
    int idx = (row * 80 + col) * 2;
    VGA[idx]     = (unsigned char)c;
    VGA[idx + 1] = color;
}

static void vga_str_at(int row, int col, const char *s, unsigned char color) {
    while (*s) { vga_put_at(row, col++, *s++, color); }
}

static void vga_hex_at(int row, int col, uint32_t val, unsigned char color) {
    const char hex[] = "0123456789ABCDEF";
    int i;
    for (i = 7; i >= 0; i--) {
        vga_put_at(row, col + i, hex[val & 0xF], color);
        val >>= 4;
    }
}

/* ── Spinner ─────────────────────────────────────────────────────────── */

static const char SPINNER[] = "|/-\\";

/* ── Demo process bodies ─────────────────────────────────────────────── */

#define DISPLAY_INTERVAL  8000000u

#define COL_LABEL    2
#define COL_COUNTER 21
#define COL_SPINNER 30

#define C1  VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN)
#define C2  VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)
#define C3  VGA_COLOR(VGA_BLACK, VGA_LIGHT_RED)

static void process1(void) {
    uint32_t n = 0, s = 0;
    for (;;) {
        if (++n % DISPLAY_INTERVAL == 0) {
            vga_hex_at(20, COL_COUNTER, n, C1);
            vga_put_at(20, COL_SPINNER, SPINNER[s++ & 3], C1);
        }
    }
}

static void process2(void) {
    uint32_t n = 0, s = 0;
    for (;;) {
        if (++n % DISPLAY_INTERVAL == 0) {
            vga_hex_at(21, COL_COUNTER, n, C2);
            vga_put_at(21, COL_SPINNER, SPINNER[s++ & 3], C2);
        }
    }
}

static void process3(void) {
    uint32_t n = 0, s = 0;
    for (;;) {
        if (++n % DISPLAY_INTERVAL == 0) {
            vga_hex_at(22, COL_COUNTER, n, C3);
            vga_put_at(22, COL_SPINNER, SPINNER[s++ & 3], C3);
        }
    }
}

/* ── Kernel entry ────────────────────────────────────────────────────── */

void kernel_main(void) {
    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 10 -- Processes & Scheduler\n\n");

    gdt_init();
    kprintf("  GDT installed\n");

    pic_init();
    pic_clear_mask(0);          /* unmask IRQ0 — timer drives the scheduler */
    kprintf("  PIC remapped, IRQ0 (timer) unmasked\n");

    idt_init();
    kprintf("  IDT installed\n");

    pmm_init(64u * 1024u * 1024u, (uint32_t)&kernel_end);
    kprintf("  PMM: %u free pages\n", pmm_get_free());

    paging_init();
    kprintf("  Paging ON (identity 0-4 MB)\n");

    heap_init(64);
    kprintf("  Heap: %u KB\n", (64u * 4096u) / 1024u);

    keyboard_init();
    pic_clear_mask(1);          /* unmask IRQ1 — keyboard */
    kprintf("  Keyboard ready\n\n");

    /* Enable interrupts — IRQ0 will start firing immediately. */
    __asm__ volatile ("sti");

    /* Register the scheduler hook and create the idle process. */
    process_init();
    kprintf("  Scheduler init  (idle PID %u)\n", process_current_pid());

    /* Spawn three demo processes. */
    uint32_t p1 = process_create(process1);
    uint32_t p2 = process_create(process2);
    uint32_t p3 = process_create(process3);
    kprintf("  Spawned PID %u, %u, %u\n\n", p1, p2, p3);

    /* Draw static labels for the live rows. */
    vga_str_at(17, 0,
        "--------------------------------------------------------------------------------",
        VGA_COLOR(VGA_BLACK, VGA_DARK_GREY));
    vga_str_at(18, 2,
        "Round-robin scheduler:  3 processes, timer tick = preempt",
        VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    vga_str_at(20, COL_LABEL, "Process 1 | iters: 00000000 |", C1);
    vga_str_at(21, COL_LABEL, "Process 2 | iters: 00000000 |", C2);
    vga_str_at(22, COL_LABEL, "Process 3 | iters: 00000000 |", C3);
    vga_str_at(24, 2,
        "All three counters advance together -- that is preemptive multitasking.",
        VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREY));

    /* Idle loop: the scheduler will switch away from here on each IRQ0. */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
