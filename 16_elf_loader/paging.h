/*
 * paging.h — Virtual Memory / Paging interface
 *
 * Module 15 additions:
 *   paging_new_address_space() — create a fresh page directory for a
 *     ring-3 process.  All pages are mapped supervisor-only (U/S=0).
 *   paging_set_user_page(cr3, virt) — mark one page user-accessible
 *     in a given page directory (sets U/S=1 in both PDE and PTE).
 *   paging_switch(cr3) — load CR3 (flushes TLB).
 *   paging_kernel_cr3() — return the kernel's own CR3 value.
 */

#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/* Page entry flags */
#define PAGE_PRESENT   (1u << 0)
#define PAGE_WRITABLE  (1u << 1)
#define PAGE_USER      (1u << 2)   /* U/S bit: 1 = ring-3 accessible */

/*
 * paging_init() — identity-map 0-4MB (supervisor-only) and enable paging.
 * The kernel's page directory is saved for use by paging_kernel_cr3().
 */
void paging_init(void);

/*
 * paging_new_address_space() — allocate a new PD for a ring-3 process.
 *
 * Copies the kernel's 4MB identity map into a fresh PD and PT, but with
 * PAGE_USER stripped from every PTE (all pages supervisor-only).
 * Call paging_set_user_page() afterwards to grant access to specific pages.
 *
 * Returns the physical address of the new PD (store in PCB.cr3).
 */
uint32_t paging_new_address_space(void);

/*
 * paging_set_user_page(cr3, virt_addr) — mark one page user-accessible.
 *
 * Sets PAGE_USER on both the PDE and the PTE for virt_addr in the PD
 * at physical address cr3.  The TLB entry is flushed with invlpg.
 * Only works for pages in 0-4MB (PDE[0]); others are ignored.
 *
 * Used by process_create_user() to grant ring-3 access to:
 *   - The user code pages (from user_code_start to user_code_end)
 *   - The user stack pages
 */
void paging_set_user_page(uint32_t cr3, uint32_t virt_addr);

/*
 * paging_switch(cr3) — write cr3 into the CR3 register.
 * The CPU immediately flushes the TLB and starts using the new PD.
 * Called by the scheduler on every process switch.
 */
void paging_switch(uint32_t cr3);

/* Returns the physical address of the kernel's own page directory. */
uint32_t paging_kernel_cr3(void);

/* Diagnostics */
uint32_t paging_get_pd_phys(void);
uint32_t paging_get_pt_phys(uint32_t pdi);

#endif /* PAGING_H */
