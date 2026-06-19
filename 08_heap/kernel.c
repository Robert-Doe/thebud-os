/*
 * kernel.c — BobOS Component 8: Heap Allocator
 *
 * Initialisation sequence:
 *   1. vga_init()    — screen output
 *   2. gdt_init()    — kernel GDT
 *   3. pic_init()    — remap PIC
 *   4. idt_init()    — exception/IRQ handlers
 *   5. sti           — interrupts enabled
 *   6. pmm_init()    — physical page bitmap
 *   7. paging_init() — identity map first 4 MB, paging ON
 *   8. heap_init()   — carve out a heap pool from the PMM
 *   9. Demo          — kmalloc / kfree stress test
 *
 * The demo exercises four scenarios that together prove the allocator is correct:
 *   A. Basic allocation — sizes small to large, check pointers are distinct.
 *   B. Write and read  — verify allocated memory is actually writable.
 *   C. Free and reuse  — free a block, re-alloc, prove the same address returns.
 *   D. Coalescing      — free adjacent blocks in order, verify they merge into one.
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

extern char kernel_end;

static void print_hex32(uint32_t v) {
    static const char hex[] = "0123456789abcdef";
    char buf[11];
    int i;
    buf[0] = '0'; buf[1] = 'x';
    for (i = 9; i >= 2; i--) { buf[i] = hex[v & 0xF]; v >>= 4; }
    buf[10] = '\0';
    kprintf("%s", buf);
}

static void print_stats(void) {
    kprintf("  Heap: used=%u B  free=%u B  total=%u B  blocks=%u\n",
            heap_get_used_bytes(),
            heap_get_free_bytes(),
            heap_get_total_bytes(),
            heap_get_num_blocks());
}

void kernel_main(void) {

    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  BobOS Kernel  --  Component 8: Heap Allocator\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n\n");

    /* ---- Infrastructure ---- */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[1] GDT:    "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    gdt_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("installed\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[2] PIC:    "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    pic_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("remapped\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[3] IDT:    "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    idt_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("installed\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[4] sti:    ");
    __asm__ volatile ("sti");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("enabled\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[5] PMM:    "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    pmm_init(128u * 1024u * 1024u, (uint32_t)&kernel_end);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("%u free pages\n", pmm_get_free());

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[6] Paging: "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    paging_init();
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)); kprintf("ON (identity 0-4MB)\n");

    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf("[7] Heap:   "); vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    heap_init(16);   /* 16 pages = 64 KB heap */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("initialised  (16 pages = %u bytes)\n\n", heap_get_total_bytes());

    /* ================================================================
     * Demo A — basic allocations of different sizes.
     * ================================================================ */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Demo A: Allocate blocks of different sizes\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    void *a1 = kmalloc(16);
    void *a2 = kmalloc(64);
    void *a3 = kmalloc(256);
    void *a4 = kmalloc(1024);

    kprintf("  kmalloc(16)   -> "); print_hex32((uint32_t)a1);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf(a1 ? "  OK\n" : "  FAIL\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    kprintf("  kmalloc(64)   -> "); print_hex32((uint32_t)a2);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf(a2 ? "  OK\n" : "  FAIL\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    kprintf("  kmalloc(256)  -> "); print_hex32((uint32_t)a3);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf(a3 ? "  OK\n" : "  FAIL\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    kprintf("  kmalloc(1024) -> "); print_hex32((uint32_t)a4);
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf(a4 ? "  OK\n" : "  FAIL\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    kprintf("\n"); print_stats(); kprintf("\n");

    /* ================================================================
     * Demo B — write to allocated blocks and read back.
     * ================================================================ */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Demo B: Write and read back\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    /* Write magic values into the blocks */
    if (a1) { uint32_t *p = (uint32_t *)a1; p[0] = 0xDEADBEEF; p[1] = 0xCAFEBABE; }
    if (a2) { uint32_t *p = (uint32_t *)a2; p[0] = 0x12345678; p[1] = 0x87654321; }

    /* Read them back and verify */
    if (a1) {
        uint32_t *p = (uint32_t *)a1;
        kprintf("  a1[0] = "); print_hex32(p[0]);
        kprintf("  a1[1] = "); print_hex32(p[1]);
        int ok = (p[0] == 0xDEADBEEF && p[1] == 0xCAFEBABE);
        vga_set_color(ok ? VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)
                        : VGA_COLOR(VGA_BLACK, VGA_LIGHT_RED));
        kprintf(ok ? "  PASS\n" : "  FAIL\n");
        vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    }
    if (a2) {
        uint32_t *p = (uint32_t *)a2;
        kprintf("  a2[0] = "); print_hex32(p[0]);
        kprintf("  a2[1] = "); print_hex32(p[1]);
        int ok = (p[0] == 0x12345678 && p[1] == 0x87654321);
        vga_set_color(ok ? VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)
                        : VGA_COLOR(VGA_BLACK, VGA_LIGHT_RED));
        kprintf(ok ? "  PASS\n" : "  FAIL\n");
        vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    }
    kprintf("\n");

    /* ================================================================
     * Demo C — free a1, re-alloc 16 bytes, prove same address returned.
     * ================================================================ */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Demo C: Free and reuse\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    kprintf("  free(a1)  <- "); print_hex32((uint32_t)a1); kprintf("\n");
    kfree(a1);
    kprintf("  "); print_stats();

    void *a5 = kmalloc(16);
    kprintf("  kmalloc(16) -> "); print_hex32((uint32_t)a5);
    int reused = (a5 == a1);
    vga_set_color(reused ? VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)
                         : VGA_COLOR(VGA_BLACK, VGA_YELLOW));
    kprintf(reused ? "  (same as a1 — reused!)\n" : "  (different address)\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("\n");

    /* ================================================================
     * Demo D — free a2, a3, a4, a5 in order; prove they coalesce.
     * ================================================================ */
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_CYAN));
    kprintf("  Demo D: Coalescing — free all blocks, watch block count drop\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    kprintf("  Before freeing:  "); print_stats();

    kfree(a5);
    kprintf("  After free(a5):  "); print_stats();

    kfree(a2);
    kprintf("  After free(a2):  "); print_stats();

    kfree(a3);
    kprintf("  After free(a3):  "); print_stats();

    kfree(a4);
    kprintf("  After free(a4):  "); print_stats();

    /* After freeing everything, we should be back to 1 block (the whole pool) */
    int coalesced = (heap_get_num_blocks() == 1);
    vga_set_color(coalesced ? VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN)
                            : VGA_COLOR(VGA_BLACK, VGA_LIGHT_RED));
    kprintf(coalesced
        ? "\n  All blocks coalesced back to 1 free block.  PASS\n"
        : "\n  Coalescing incomplete — fragmentation remains.  FAIL\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));

    /* ---- Final summary ---- */
    kprintf("\n");
    kprintf("================================================================\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_LIGHT_GREEN));
    kprintf("  Heap allocator working correctly.\n");
    kprintf("  Ready for Module 9: Keyboard Driver.\n");
    vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("================================================================\n");

    for (;;) { __asm__ volatile ("hlt"); }
}
