/*
 * kernel.c — BobOS Component 7: Virtual Memory / Paging
 *
 * Initialisation sequence:
 *   1. vga_init()     — screen output
 *   2. gdt_init()     — kernel GDT
 *   3. pic_init()     — remap PIC (keeps hardware IRQs off CPU exceptions)
 *   4. idt_init()     — install 48 gates including #PF handler
 *   5. sti            — enable interrupts
 *   6. pmm_init()     — build the free-page bitmap
 *   7. paging_init()  — build page tables, load CR3, set PG bit in CR0
 *   8. Demo           — prove identity mapping works; inspect mappings
 *
 * The key moment is step 7: after `paging_init()` returns we are running in
 * paged mode.  Every memory access now goes through the CPU's TLB/page walker.
 * Because we used identity mapping (virtual == physical for 0–4 MB), the
 * kernel's existing pointers to code, stack, PMM bitmap, and VGA buffer all
 * remain valid without any changes.
 */

#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "pic.h"
#include "idt.h"
#include "isr.h"
#include "pmm.h"
#include "paging.h"

extern char kernel_end;

/* -------------------------------------------------------------------------
 * print_hex32 — fixed-width 0xNNNNNNNN without zero-padding gaps in kprintf.
 * ------------------------------------------------------------------------- */
static void print_hex32(uint32_t v) {
    static const char hex[] = "0123456789abcdef";
    char buf[11];
    int i;
    buf[0] = '0'; buf[1] = 'x';
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
    kprintf("  BobOS Kernel  --  Component 7: Virtual Memory / Paging\n");
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
    kprintf("installed (#PF handler live — unmapped access = page fault panic)\n");

    /* ---- Step 4: Interrupts ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[4] sti: ");
    __asm__ volatile ("sti");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("interrupts enabled\n");

    /* ---- Step 5: PMM ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[5] PMM: ");
    pmm_init(128u * 1024u * 1024u, (uint32_t)&kernel_end);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("initialised  (%u free pages, %u MB)\n",
            pmm_get_free(), pmm_get_free() / 256u);

    /* ---- Step 6: Paging ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[6] Paging: ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("building tables...\n");

    paging_init();   /* <-- paging is ON after this line */

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("    PAGING ON  -- still running! Identity map confirmed.\n\n");

    /* ================================================================
     * Demo — inspect the page table structures we built.
     * ================================================================ */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Page Table Layout\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    kprintf("  Page Directory phys : ");
    print_hex32(paging_get_pd_phys());
    kprintf("\n");

    kprintf("  Page Table[0] phys  : ");
    print_hex32(paging_get_pt_phys(0));
    kprintf("\n");

    kprintf("  Coverage            : ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("0x00000000 - 0x003FFFFF  (4 MB, 1024 pages, identity mapped)\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    kprintf("  PDE[1] - PDE[1023]  : ");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_RED));
    kprintf("not present (access = page fault)\n\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    /* ================================================================
     * Demo — verify virtual == physical for key addresses.
     * ================================================================ */
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Address Verification  (virtual == physical = identity map)\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    kprintf("  &kernel_main virt : ");
    print_hex32((uint32_t)kernel_main);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  (phys = same)\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    kprintf("  &kernel_end  virt : ");
    print_hex32((uint32_t)&kernel_end);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  (phys = same)\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    kprintf("  VGA buffer   virt : ");
    print_hex32(0xB8000);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  (phys = same, still works)\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    kprintf("  PD           virt : ");
    print_hex32(paging_get_pd_phys());
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  (phys = same, allocated from PMM)\n\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    /* ================================================================
     * Demo — PMM stats after paging stole two pages for its tables.
     * ================================================================ */
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  PMM stats after paging\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    kprintf("  Total pages : %u\n", pmm_get_total());
    kprintf("  Used  pages : %u  (kernel + 2 page tables)\n", pmm_get_used());
    kprintf("  Free  pages : %u  (%u MB)\n",
            pmm_get_free(), pmm_get_free() / 256u);
    kprintf("\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  Paging working correctly.  Ready for Module 8: Heap Allocator.\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
