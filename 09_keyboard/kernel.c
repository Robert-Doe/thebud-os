/*
 * kernel.c — BobOS Component 9: Keyboard Driver
 *
 * Initialisation sequence:
 *   1. vga_init()       — screen output
 *   2. gdt_init()       — kernel GDT
 *   3. pic_init()       — remap PIC (IRQ1 will land on vector 33)
 *   4. idt_init()       — install exception/IRQ gates
 *   5. keyboard_init()  — register IRQ1 handler
 *   6. sti              — interrupts enabled; keyboard is now live
 *   7. pmm_init()       — physical page bitmap
 *   8. paging_init()    — identity map first 4 MB
 *   9. heap_init()      — 64 KB kernel heap
 *  10. Interactive loop — echo typed characters to screen (backspace works)
 *
 * The interactive loop is the proof: every key the user presses appears on
 * screen in real time because the IRQ1 handler fires, translates the scancode,
 * and pushes the character into the ring buffer, which the main loop drains.
 *
 * This is the first module where BobOS responds to user input.
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

extern char kernel_end;

/* -------------------------------------------------------------------------
 * vga_erase_last — erase the character immediately before the cursor.
 * Used to implement visible backspace in the echo loop.
 * Writes directly to the VGA buffer at a fixed offset behind the cursor.
 * This is a one-off helper; a full terminal would track column state.
 * ------------------------------------------------------------------------- */
static void vga_erase_last(void) {
    /* The VGA driver does not expose cursor position, so we implement backspace
     * by overwriting with a space and letting the driver handle '\b' itself.
     * We re-use the standard putchar interface: '\b' moves the cursor back one
     * column (if col > 0) but does NOT erase.  We then write a space to blank
     * the character, then '\b' again to leave the cursor on the blank cell. */
    vga_putchar('\b');
    vga_putchar(' ');
    vga_putchar('\b');
}

void kernel_main(void) {

    /* ---- Infrastructure ---- */
    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  BobOS Kernel  --  Component 9: Keyboard Driver\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[1] GDT:      "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    gdt_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("installed\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[2] PIC:      "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    pic_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("remapped (IRQ1 -> vector 33)\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[3] IDT:      "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    idt_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("installed\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[4] Keyboard: "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    keyboard_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("IRQ1 handler registered\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[5] sti:      ");
    __asm__ volatile ("sti");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("interrupts enabled -- keyboard live\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[6] PMM:      "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    pmm_init(128u * 1024u * 1024u, (uint32_t)&kernel_end);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("%u free pages\n", pmm_get_free());

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[7] Paging:   "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    paging_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("ON (identity 0-4MB)\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[8] Heap:     "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    heap_init(16);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("%u bytes ready\n\n", heap_get_total_bytes());

    /* ---- Keyboard test prompt ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Keyboard ready.  Type anything -- characters appear below.\n");
    kprintf("  Shift works.  Backspace erases.  Enter submits a line.\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n\n");

    /* Print the first prompt */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("> ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    /* -------------------------------------------------------------------------
     * Interactive echo loop.
     *
     * The loop sleeps in HLT (low power) and wakes on any interrupt.
     * When a keyboard IRQ fires, the handler fills the ring buffer.
     * We drain the ring buffer after every wake-up.
     *
     * Behaviour:
     *   - Printable character  → echo it at the current cursor position.
     *   - Backspace ('\b')     → erase the previous character visually.
     *   - Enter ('\n')         → newline + fresh prompt.
     *   - All other characters → echoed as-is (ESC, Tab, etc.).
     * ------------------------------------------------------------------------- */
    for (;;) {
        __asm__ volatile ("hlt");   /* sleep until next interrupt */

        char c;
        while ((c = keyboard_getchar()) != 0) {
            if (c == '\b') {
                vga_erase_last();
            } else if (c == '\n') {
                vga_putchar('\n');
                vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
                kprintf("> ");
                vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
            } else {
                vga_putchar(c);
            }
        }
    }
}
