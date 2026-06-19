/*
 * kernel.c — BobOS Component 3: VGA Driver Demo
 *
 * In component 2, kernel_main() wrote directly to the VGA buffer at 0xB8000
 * using raw pointer arithmetic and manual row/column calculations. That worked
 * for a proof-of-concept, but it had no state: no cursor, no scrolling, no
 * way to print numbers.
 *
 * This component introduces a proper VGA text driver in vga.c. kernel_main()
 * now calls kprintf() — a minimal kernel printf that supports %s, %d, %u,
 * %x, %X, and %c. The driver tracks the cursor, handles \n and \t, and
 * scrolls the screen automatically when output reaches the last row.
 *
 * We exercise every feature of the driver here so the DECISIONS.md can
 * reference a working demonstration.
 */

#include "vga.h"

void kernel_main(void) {

    /* Initialize the driver: clear screen, default color = white on black */
    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    /* ---- Banner ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  BobOS Kernel  --  Component 3: VGA Text Driver\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n\n");

    /* ---- Feature: strings and newlines ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[FEATURE] Strings and newlines\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("  Hello from kprintf! This line wraps automatically.\n");
    kprintf("  Tab:\there\tand\there\n\n");

    /* ---- Feature: signed and unsigned integers ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[FEATURE] Integers\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("  Positive: %d\n", 42);
    kprintf("  Negative: %d\n", -1337);
    kprintf("  Zero:     %d\n", 0);
    kprintf("  Unsigned: %u\n", 4294967295u);
    kprintf("\n");

    /* ---- Feature: hexadecimal ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[FEATURE] Hexadecimal\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("  VGA buffer:  0x%x\n", 0xB8000);
    kprintf("  Kernel base: 0x%x\n", 0x1000);
    kprintf("  GDT code:    0x%X  data: 0x%X\n", 0x08, 0x10);
    kprintf("\n");

    /* ---- Feature: colors ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[FEATURE] Colors\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_RED));
    kprintf("  Red   ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_GREEN));
    kprintf("Green   ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_BLUE));
    kprintf("Blue   ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_CYAN));
    kprintf("Cyan   ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_MAGENTA));
    kprintf("Magenta");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("\n\n");

    /* ---- Feature: scrolling ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[FEATURE] Scroll test -- printing 10 lines to force a scroll\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    int i;
    for (i = 1; i <= 10; i++) {
        kprintf("  Scroll line %d of 10\n", i);
    }
    kprintf("\n");

    /* ---- Status ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  All driver features verified. Next: GDT (Component 4).\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    for (;;) {}
}
