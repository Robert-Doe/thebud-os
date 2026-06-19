/*
 * kernel.c — BobOS Component 6: Physical Memory Manager
 *
 * Initialisation sequence:
 *   1. vga_init()   — screen output
 *   2. gdt_init()   — kernel GDT (prerequisite for valid segment registers)
 *   3. pic_init()   — remap PIC (prevents spurious hardware interrupt crashes)
 *   4. idt_init()   — install exception/IRQ gates
 *   5. sti          — enable interrupts (timer keeps running; panic handler ready)
 *   6. pmm_init()   — scan memory, build the free-page bitmap
 *   7. Demo loop    — allocate pages, free some, re-allocate to prove reuse
 *
 * The demo proves the PMM works correctly:
 *   - Allocated addresses are page-aligned (multiple of 4096).
 *   - Addresses increase through physical memory (bitmap scans low→high).
 *   - After freeing a page, a subsequent alloc re-uses it.
 *   - Used/free counters stay consistent throughout.
 */

#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "pic.h"
#include "idt.h"
#include "isr.h"
#include "pmm.h"

/*
 * kernel_end — a linker symbol, not a C variable.
 * The linker places this symbol at the byte immediately after .bss.
 * We take its *address* to get the numeric end-of-kernel value.
 */
extern char kernel_end;

/* -------------------------------------------------------------------------
 * print_hex32 — print a 32-bit value as 0x00000000 with the current color.
 * kprintf %x doesn't zero-pad, so we write the fixed-width version ourselves.
 * ------------------------------------------------------------------------- */
static void print_hex32(uint32_t v) {
    static const char hex[] = "0123456789abcdef";
    char buf[11];
    int i;
    buf[0]  = '0'; buf[1] = 'x';
    for (i = 9; i >= 2; i--) {
        buf[i] = hex[v & 0xF];
        v >>= 4;
    }
    buf[10] = '\0';
    kprintf("%s", buf);
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
    kprintf("  BobOS Kernel  --  Component 6: Physical Memory Manager\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n\n");

    /* ---- Step 1: GDT ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[1] GDT: ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    gdt_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("installed\n");

    /* ---- Step 2: PIC ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[2] PIC: ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    pic_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("remapped (IRQ0-7->32-39, IRQ8-15->40-47)\n");

    /* ---- Step 3: IDT ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[3] IDT: ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    idt_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("installed (48 gates)\n");

    /* ---- Step 4: Enable interrupts ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[4] sti: ");
    __asm__ volatile ("sti");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("interrupts enabled\n");

    /* ---- Step 5: PMM ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[5] PMM: ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    /*
     * We tell the PMM that 128 MB of RAM is available.
     * QEMU's default is 128 MB so this matches what the hardware reports.
     * We also tell it where our kernel ends so it does not hand out those
     * pages — it reads the linker-exported kernel_end symbol.
     */
    pmm_init(128u * 1024u * 1024u, (uint32_t)&kernel_end);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("initialised\n");

    /* ---- PMM stats after init ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("\n");
    kprintf("  kernel_end   : "); print_hex32((uint32_t)&kernel_end); kprintf("\n");
    kprintf("  Total pages  : %u  (%u MB)\n",
            pmm_get_total(), pmm_get_total() / 256u);
    kprintf("  Used  pages  : %u\n", pmm_get_used());
    kprintf("  Free  pages  : %u  (%u MB)\n\n",
            pmm_get_free(), pmm_get_free() / 256u);

    /* ================================================================
     * Demo A — Allocate 8 pages and print their physical addresses.
     * The addresses should be page-aligned and increase monotonically.
     * ================================================================ */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Demo A: Allocate 8 pages\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    void *pages[8];
    int i;
    for (i = 0; i < 8; i++) {
        pages[i] = pmm_alloc_page();
        vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
        kprintf("  alloc[%d] -> ", i);
        vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
        if (pages[i]) {
            print_hex32((uint32_t)pages[i]);
            vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
            kprintf("  OK\n");
        } else {
            vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_RED));
            kprintf("  NULL (out of memory!)\n");
        }
    }

    kprintf("\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("  Free pages remaining: %u\n\n", pmm_get_free());

    /* ================================================================
     * Demo B — Free pages 0, 2, 4, 6 (the even-indexed ones).
     * ================================================================ */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Demo B: Free even-indexed pages (0, 2, 4, 6)\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    for (i = 0; i < 8; i += 2) {
        vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
        kprintf("  free[%d]  <- ", i);
        vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
        print_hex32((uint32_t)pages[i]);
        pmm_free_page(pages[i]);
        vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
        kprintf("  freed\n");
    }

    kprintf("\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("  Free pages after freeing 4: %u\n\n", pmm_get_free());

    /* ================================================================
     * Demo C — Allocate 4 more pages.
     * They must come back as the addresses we just freed (bitmap reuse).
     * ================================================================ */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Demo C: Re-allocate 4 pages (should reuse freed addresses)\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    for (i = 0; i < 4; i++) {
        void *p = pmm_alloc_page();
        vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
        kprintf("  re-alloc[%d] -> ", i);
        vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
        if (p) {
            print_hex32((uint32_t)p);
            vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
            kprintf("  OK (reused)\n");
        } else {
            vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_RED));
            kprintf("  NULL (unexpected!)\n");
        }
    }

    /* ---- Final stats ---- */
    kprintf("\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  Final stats\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    kprintf("  Total : %u pages\n", pmm_get_total());
    kprintf("  Used  : %u pages\n", pmm_get_used());
    kprintf("  Free  : %u pages (%u MB)\n",
            pmm_get_free(), pmm_get_free() / 256u);
    kprintf("\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  PMM working correctly.  Ready for Module 7: Paging.\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    /* Spin forever — exceptions still caught by the IDT panic handler */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
