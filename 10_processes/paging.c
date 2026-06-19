/*
 * paging.c — Virtual Memory / Paging implementation
 *
 * x86 paging uses a two-level table hierarchy:
 *
 *   Page Directory (PD)  — one per address space, 1024 × 4-byte entries.
 *   Page Tables (PT)     — one per 4 MB region, 1024 × 4-byte entries each.
 *
 * Each entry is 32 bits:
 *   Bits 31-12  Physical address of the next level (4 KB aligned; low 12 bits = 0)
 *   Bits 11-9   Available for OS use
 *   Bit  7      Page Size (PDE only; 0 = 4 KB page table, 1 = 4 MB page)
 *   Bit  5      Accessed (CPU sets when entry is used)
 *   Bit  4      Cache Disable
 *   Bit  3      Write-Through
 *   Bit  2      User/Supervisor (0 = ring 0 only)
 *   Bit  1      Writable
 *   Bit  0      Present (must be 1 for CPU to use the entry)
 *
 * We fill in only PDE[0] (the first 4 MB) and leave the other 1023 PDEs as 0
 * (not present).  Accessing a not-present address would trigger a page fault
 * (#PF, vector 14) — our IDT catches it and shows a panic dump.
 */

#include "paging.h"
#include "pmm.h"

/* -------------------------------------------------------------------------
 * Module state — physical addresses of the structures we build.
 * We keep only PD and the first PT; further PTs are added in later modules.
 * ------------------------------------------------------------------------- */
static uint32_t pd_phys = 0;    /* page directory physical address */
static uint32_t pt0_phys = 0;   /* page table for PDE[0] (0 – 4 MB) */

/* -------------------------------------------------------------------------
 * zero_page — fill a PMM-allocated 4 KB page with zeros.
 *
 * The PMM does not zero pages on allocation (it would be wasteful for callers
 * that will immediately overwrite the whole page anyway).  Page tables MUST be
 * zeroed before the CPU uses them: a stale non-zero PDE with the Present bit
 * accidentally set would make the CPU follow a garbage physical address.
 * ------------------------------------------------------------------------- */
static void zero_page(uint32_t *p) {
    int i;
    for (i = 0; i < 1024; i++) {
        p[i] = 0;
    }
}

/* -------------------------------------------------------------------------
 * paging_init
 * ------------------------------------------------------------------------- */
void paging_init(void) {
    int i;
    uint32_t cr0;

    /* ---- Step 1: allocate and zero the page directory ---- */
    uint32_t *pd = (uint32_t *)pmm_alloc_page();
    pd_phys = (uint32_t)pd;
    zero_page(pd);

    /* ---- Step 2: allocate and zero the first page table (0 – 4 MB) ---- */
    uint32_t *pt = (uint32_t *)pmm_alloc_page();
    pt0_phys = (uint32_t)pt;
    zero_page(pt);

    /* ---- Step 3: fill 1024 PTEs — identity map 0x00000000–0x003FFFFF ----
     *
     * PTE format:
     *   bits 31-12 = physical page frame address (page_number << 12)
     *   bit  1     = PAGE_WRITABLE
     *   bit  0     = PAGE_PRESENT
     *
     * Identity mapping: virtual page N maps to physical page N.
     * After paging is on, a virtual address of 0x1234 still translates to
     * physical 0x1234 — the kernel's existing code pointers remain valid.
     */
    for (i = 0; i < 1024; i++) {
        pt[i] = (uint32_t)(i * 0x1000) | PAGE_PRESENT | PAGE_WRITABLE;
    }

    /* ---- Step 4: install the page table in PDE[0] ----
     *
     * PDE format (same bit layout as PTE, but points to a page table):
     *   bits 31-12 = physical address of the page table
     *   bit  1     = PAGE_WRITABLE
     *   bit  0     = PAGE_PRESENT
     *
     * All other PDEs stay 0 (not present); accessing virtual addresses above
     * 4 MB will trigger a #PF exception.
     */
    pd[0] = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITABLE;

    /* ---- Step 5: load CR3 with the physical address of the page directory.
     *
     * CR3 (also called PDBR — Page Directory Base Register) tells the CPU
     * where to find the top-level page directory.  The CPU flushes its TLB
     * (Translation Lookaside Buffer — its page-walk cache) when CR3 is written.
     */
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys));

    /* ---- Step 6: enable paging — set bit 31 (PG) of CR0. ----
     *
     * We must read CR0 first (it has other bits we must preserve), set bit 31,
     * and write it back.  The moment this write completes, the CPU starts
     * translating ALL memory accesses through the page tables.
     *
     * After this instruction returns, we are running paged code.  The next
     * fetch will be at the virtual address of the instruction after this one.
     * That virtual address resolves to the same physical address (identity map),
     * so execution continues seamlessly.
     */
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    /*
     * If we reach here without a page fault, paging is working.
     * The identity mapping means virtual == physical for every address the
     * kernel currently holds (code, stack, VGA buffer, PMM bitmap — all < 4 MB).
     */
}

/* -------------------------------------------------------------------------
 * Diagnostics
 * ------------------------------------------------------------------------- */
uint32_t paging_get_pd_phys(void) {
    return pd_phys;
}

uint32_t paging_get_pt_phys(uint32_t pdi) {
    if (pdi == 0) return pt0_phys;
    return 0;   /* only PDE[0] is populated in Module 7 */
}
