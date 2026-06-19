/*
 * kernel.c — BobOS Component 5: IDT and Interrupts
 *
 * Initialisation sequence:
 *   1. vga_init()   — screen output
 *   2. gdt_init()   — kernel GDT (prerequisite for valid segment registers)
 *   3. pic_init()   — remap PIC so IRQs land on vectors 32-47, not 8-15
 *   4. idt_init()   — install 48 gates (32 exceptions + 16 IRQs) and lidt
 *   5. irq_register(0, timer_handler) — hook the PIT tick
 *   6. sti          — enable CPU interrupts; from this point the timer fires
 *   7. Main loop    — displays a live tick counter at a fixed screen location
 *
 * The timer tick counter (live update) proves that hardware interrupts are
 * reaching and executing our C handler without crashing the kernel.
 *
 * Any CPU exception (divide-by-zero, page fault, etc.) will trigger the
 * kernel_panic() path in isr.c, print a full register dump in red, and halt.
 */

#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "pic.h"
#include "idt.h"
#include "isr.h"

/* -------------------------------------------------------------------------
 * Timer state — modified in interrupt context, read in kernel context.
 * 'volatile' ensures the compiler reloads from RAM every time rather than
 * serving a cached register value.
 * ------------------------------------------------------------------------- */
static volatile uint32_t tick_count = 0;

static void timer_handler(struct interrupt_frame *frame) {
    (void)frame;   /* unused parameter */
    tick_count++;
}

/* -------------------------------------------------------------------------
 * Display a uint32_t in decimal at a fixed screen position, without moving
 * the VGA driver's cursor.  Writes directly to the VGA buffer.
 * Row 14, col 18 — just after the "Timer ticks: " label printed in main.
 * ------------------------------------------------------------------------- */
#define TICK_ROW  14
#define TICK_COL  18

static void display_uint(uint32_t n) {
    volatile unsigned char *vga_buf = (volatile unsigned char *)0xB8000;
    unsigned char color = VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN);
    char buf[11];  /* max 10 decimal digits + null */
    int i;

    buf[10] = '\0';
    for (i = 9; i >= 0; i--) {
        buf[i] = (char)('0' + (n % 10));
        n /= 10;
    }

    for (i = 0; i < 10; i++) {
        int idx = (TICK_ROW * 80 + TICK_COL + i) * 2;
        vga_buf[idx]     = (unsigned char)buf[i];
        vga_buf[idx + 1] = color;
    }
}

/* -------------------------------------------------------------------------
 * kernel_main
 * ------------------------------------------------------------------------- */
void kernel_main(void) {

    /* ---- Screen setup ---- */
    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  BobOS Kernel  --  Component 5: IDT and Interrupts\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n\n");

    /* ---- Step 1: GDT ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[1] GDT: ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    gdt_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("installed (flat 0-4GB, code=0x08, data=0x10)\n");

    /* ---- Step 2: PIC remapping ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[2] PIC: ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    pic_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("remapped  (IRQ0-7 -> vectors 32-39, IRQ8-15 -> vectors 40-47)\n");

    /* ---- Step 3: IDT ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[3] IDT: ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    idt_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("installed (32 exception gates + 16 IRQ gates, lidt done)\n");

    /* ---- Step 4: Timer IRQ ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[4] IRQ0 (timer): ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    irq_register(0, timer_handler);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("handler registered\n");

    /* ---- Step 5: Enable interrupts ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[5] sti: ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    __asm__ volatile ("sti");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("interrupts enabled -- timer firing at ~18 Hz\n");

    /* ---- Status summary ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("\n");
    kprintf("  Exceptions 0-31 : kernel_panic() with register dump\n");
    kprintf("  Hardware IRQ0   : timer tick counter (see below)\n");
    kprintf("  Hardware IRQ1-15: unhandled (no-op, EOI sent)\n");
    kprintf("\n");
    kprintf("  Timer ticks: ");   /* counter appears at TICK_ROW, TICK_COL */
    kprintf("\n\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  System running. Interrupts active.\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    /* ---- Main loop: update tick counter on screen ---- */
    uint32_t last_displayed = (uint32_t)-1;
    for (;;) {
        uint32_t current = tick_count;
        if (current != last_displayed) {
            display_uint(current);
            last_displayed = current;
        }
        /* 'pause' is a hint to the CPU that this is a spin-wait loop.
         * On real hardware it reduces power and improves performance of
         * hyperthreading.  On QEMU it has no effect but is correct practice. */
        __asm__ volatile ("pause");
    }
}
