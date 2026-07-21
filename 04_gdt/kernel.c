/*
 * kernel.c — BobOS Component 4: GDT in C
 *
 * Component 3 gave us the VGA driver so we can print anything cleanly.
 * Component 4 uses that output to walk through the new GDT setup:
 *
 *   1. vga_init()  — clear the screen, set up the cursor
 *   2. gdt_init()  — build the GDT as a C struct array, load it into the CPU,
 *                    and reload all segment registers including CS
 *   3. Print every field of every GDT entry so you can see exactly what the
 *      CPU is now working with
 *
 * The GDT set up here is identical in effect to the one the bootloader
 * assembled — same flat 0→4GB model, same three entries.  The difference
 * is that this one lives in the kernel, is built from a typed struct, and
 * is loaded by C code rather than hand-crafted assembly bytes.  It is also
 * the foundation for adding ring-3 (user-mode) segments later.
 */

#include "vga.h"
#include "gdt.h"

/* Read the GDTR register back so we can display what the CPU actually loaded */
static void read_gdtr(uint32_t *out_base, uint16_t *out_limit)
{
    /*
     * 'sgdt' (Store GDT Register) writes 6 bytes — 2-byte limit, then
     * 4-byte base address — to the memory operand we supply.
     * We use a local 6-byte array and extract the fields manually.
     */
    uint8_t buf[6];
    __asm__ volatile ("sgdt %0" : "=m"(buf));
    *out_limit = (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
    *out_base  = (uint32_t)(buf[2] |
                            ((uint32_t)buf[3] << 8)  |
                            ((uint32_t)buf[4] << 16) |
                            ((uint32_t)buf[5] << 24));
}

void kernel_main(void)
{
    /* ---- Initialise output first so we can print during GDT setup ---- */
    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    /* ---- Banner ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  BobOS Kernel  --  Component 4: GDT\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n\n");

    /* ---- Install the kernel GDT ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[1] Installing kernel GDT...\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    gdt_init();

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("    gdt_init() returned -- CPU accepted the new GDT.\n");
    kprintf("    lgdt executed, CS reloaded via far-return, DS/ES/FS/GS/SS reloaded.\n\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    /* ---- Read the GDTR back and display it ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[2] Verifying GDTR (sgdt):\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    uint32_t gdt_base;
    uint16_t gdt_limit;
    read_gdtr(&gdt_base, &gdt_limit);

    kprintf("    GDTR.base  = 0x%x\n", gdt_base);
    kprintf("    GDTR.limit = 0x%x  (%d entries of 8 bytes)\n\n",
            gdt_limit, (gdt_limit + 1) / 8);

    /* ---- Show segment selectors in use ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[3] Segment selectors:\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("    GDT[0] selector = 0x%x  (null  -- must never be used)\n", GDT_SEL_NULL);
    kprintf("    GDT[1] selector = 0x%x  (code  -- CS)\n",                 GDT_SEL_CODE);
    kprintf("    GDT[2] selector = 0x%x  (data  -- DS, ES, FS, GS, SS)\n\n", GDT_SEL_DATA);

    /* ---- Explain the flat model ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[4] Memory model:\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("    Both segments cover 0x00000000 -> 0xFFFFFFFF (4 GB).\n");
    kprintf("    Flat model: virtual address == physical address.\n");
    kprintf("    Fine-grained protection comes in Module 7 (paging).\n\n");

    /* ---- Access and flags bytes ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[5] Access bytes:\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("    Code access byte = 0x%x\n", GDT_ACCESS_CODE);
    kprintf("      Present=1, Ring=0, Type=code/data, Exec=1, RW=1\n");
    kprintf("    Data access byte = 0x%x\n", GDT_ACCESS_DATA);
    kprintf("      Present=1, Ring=0, Type=code/data, Exec=0, RW=1\n\n");

    /* ---- Done ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  GDT installed and verified. Next: IDT and interrupts (Module 5).\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    for (;;) {}
}
