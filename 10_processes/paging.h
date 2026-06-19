/*
 * paging.h — Virtual Memory / Paging interface
 *
 * x86 paging maps virtual addresses (what code uses) to physical addresses
 * (what the RAM chips respond to) using a two-level table structure:
 *
 *   Virtual address (32 bits):
 *   ┌──────────┬──────────┬────────────┐
 *   │ Dir [31-22] │ Table [21-12] │ Offset [11-0] │
 *   └──────────┴──────────┴────────────┘
 *        10 bits      10 bits       12 bits
 *
 *   1. CPU takes bits 31-22 → index into Page Directory (1024 entries)
 *   2. PDE gives physical address of a Page Table (also 1024 entries)
 *   3. CPU takes bits 21-12 → index into that Page Table
 *   4. PTE gives physical address of the 4 KB page
 *   5. CPU adds bits 11-0 (offset within page) → final physical address
 *
 * In Module 7 we identity-map the first 4 MB so the kernel continues
 * running at the same addresses after paging is enabled (virtual == physical).
 */

#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Page entry flags — ORed into the lower 12 bits of any PDE or PTE.
 * The upper 20 bits hold the physical address of the next level table or page.
 * ------------------------------------------------------------------------- */
#define PAGE_PRESENT   (1u << 0)   /* entry is valid; CPU will use it          */
#define PAGE_WRITABLE  (1u << 1)   /* page can be written (not read-only)      */
#define PAGE_USER      (1u << 2)   /* accessible from Ring 3 (user mode)       */
/* We always use PAGE_PRESENT | PAGE_WRITABLE for kernel pages.               */

/*
 * paging_init — set up the page directory and page tables, then enable paging.
 *
 * What it does:
 *   1. Allocates a 4 KB page directory from the PMM.
 *   2. Allocates one 4 KB page table to cover 0x00000000–0x003FFFFF (4 MB).
 *   3. Fills all 1024 PTEs with identity mappings (virtual == physical).
 *   4. Loads CR3 with the physical address of the page directory.
 *   5. Sets the PG bit (bit 31) of CR0 — paging is now ON.
 *
 * After this returns, every virtual address the CPU sees is translated through
 * the page tables before hitting RAM.  Because we use identity mapping, all
 * existing kernel code and data continue to work without modification.
 *
 * Call after pmm_init() — this function allocates physical pages from the PMM.
 */
void paging_init(void);

/*
 * Diagnostics — return addresses of the allocated structures.
 * Useful for verifying paging is set up correctly.
 */
uint32_t paging_get_pd_phys(void);         /* physical address of page directory */
uint32_t paging_get_pt_phys(uint32_t pdi); /* physical address of page table[pdi]*/

#endif /* PAGING_H */
