/*
 * paging.c — Virtual Memory / Paging implementation
 *
 * Module 15 changes
 * ─────────────────
 * The kernel identity map (0-4MB) is now supervisor-only: paging_init()
 * fills PTEs with PAGE_PRESENT | PAGE_WRITABLE and deliberately omits
 * PAGE_USER.  A ring-3 process cannot read kernel pages.
 *
 * paging_new_address_space() creates a fresh PD for a user process.  It
 * copies the kernel's PT exactly (all supervisor-only), then
 * process_create_user() calls paging_set_user_page() to selectively open
 * the pages the process actually needs: its code and its user stack.
 *
 * paging_switch() loads CR3.  The scheduler calls it on every context
 * switch so the MMU sees the right page directory.
 *
 * ── Why both PDE and PTE need PAGE_USER ─────────────────────────────────
 *
 * Intel's paging rules: for a user-mode access to succeed, BOTH the PDE
 * and the PTE must have the U/S bit set (bit 2 = PAGE_USER).  If the PDE
 * has U/S=0, the entire 4MB region is supervisor-only regardless of PTEs.
 * paging_set_user_page() therefore sets the bit in both structures.
 */

#include "paging.h"
#include "pmm.h"

static uint32_t pd_phys  = 0;   /* kernel page directory physical addr */
static uint32_t pt0_phys = 0;   /* kernel page table 0 (0-4MB) phys addr */

static void zero_page(uint32_t *p) {
    int i;
    for (i = 0; i < 1024; i++) p[i] = 0;
}

void paging_init(void) {
    int i;
    uint32_t cr0;
    uint32_t *pd = (uint32_t *)pmm_alloc_page();
    uint32_t *pt = (uint32_t *)pmm_alloc_page();

    pd_phys  = (uint32_t)pd;
    pt0_phys = (uint32_t)pt;

    zero_page(pd);
    zero_page(pt);

    /* Identity-map 0-4MB, supervisor-only (no PAGE_USER).
     * Ring-3 processes must be given explicit access via paging_set_user_page(). */
    for (i = 0; i < 1024; i++)
        pt[i] = (uint32_t)(i * 0x1000u) | PAGE_PRESENT | PAGE_WRITABLE;

    /* PDE[0] also supervisor-only until a user process needs access. */
    pd[0] = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITABLE;

    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys));

    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}

uint32_t paging_new_address_space(void) {
    int i;
    uint32_t *new_pd  = (uint32_t *)pmm_alloc_page();
    uint32_t *new_pt  = (uint32_t *)pmm_alloc_page();
    uint32_t *kern_pt = (uint32_t *)pt0_phys;

    zero_page(new_pd);
    zero_page(new_pt);

    /* Copy kernel's PT but strip PAGE_USER — all supervisor-only by default. */
    for (i = 0; i < 1024; i++)
        new_pt[i] = kern_pt[i] & ~PAGE_USER;

    /* Install the new PT in PDE[0].  Supervisor-only until a page within
     * it is explicitly made user-accessible by paging_set_user_page(). */
    new_pd[0] = (uint32_t)new_pt | PAGE_PRESENT | PAGE_WRITABLE;

    return (uint32_t)new_pd;
}

void paging_set_user_page(uint32_t cr3, uint32_t virt_addr) {
    uint32_t *pd  = (uint32_t *)cr3;
    uint32_t  pdi = virt_addr >> 22;
    uint32_t  pti = (virt_addr >> 12) & 0x3FFu;
    uint32_t *pt;

    if (!(pd[pdi] & PAGE_PRESENT)) return;

    /* Set USER on PDE — opens the entire 4MB region for user inspection at
     * the directory level.  Individual PTEs still gate access per-page. */
    pd[pdi] |= PAGE_USER;

    pt = (uint32_t *)(pd[pdi] & ~0xFFFu);
    if (!(pt[pti] & PAGE_PRESENT)) return;

    /* Set USER on PTE — the specific 4KB page is now ring-3 readable. */
    pt[pti] |= PAGE_USER;

    /* Flush the TLB entry for this virtual address. */
    __asm__ volatile ("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

void paging_switch(uint32_t cr3) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

uint32_t paging_kernel_cr3(void) {
    return pd_phys;
}

uint32_t paging_get_pd_phys(void)            { return pd_phys; }
uint32_t paging_get_pt_phys(uint32_t pdi)    { return pdi == 0 ? pt0_phys : 0; }
