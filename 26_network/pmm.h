/*
 * pmm.h — Physical Memory Manager interface
 *
 * The PMM tracks which 4KB pages of physical RAM are free and which are in
 * use.  It is the foundation that all higher-level memory systems (paging,
 * heap allocator) sit on.  Every time any module needs a page of physical
 * RAM it calls pmm_alloc_page(); every time it is done it calls
 * pmm_free_page().
 *
 * Implementation: a flat bitmap — one bit per 4KB page.
 *   bit = 1 → page is FREE (available to allocate)
 *   bit = 0 → page is USED (kernel, reserved, or already allocated)
 *
 * At 4KB per page, a 128 MB address space needs 32,768 pages.
 * The bitmap is 32,768 / 8 = 4,096 bytes — 4 KB of BSS.
 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>

/* Size of one physical page — 4 KB, the smallest unit the MMU handles */
#define PMM_PAGE_SIZE   4096u

/*
 * Maximum physical memory this PMM can track: 128 MB.
 * 128 MB / 4 KB = 32,768 pages → 4,096 bytes of bitmap in BSS.
 * Increase if your machine has more RAM and you recompile.
 */
#define PMM_MAX_PAGES   32768u   /* 128 MB worth of 4 KB pages */

/*
 * pmm_init — initialise the PMM.
 *
 * mem_bytes   : total usable physical RAM in bytes (e.g. 128 * 1024 * 1024).
 * kernel_end  : first byte past the end of the kernel image in RAM.
 *               All physical pages from this address upwards (and above the
 *               first 1 MB reserved region) will be marked free.
 *
 * After this call pmm_alloc_page() is ready to use.
 * Call once, early in kernel_main(), after gdt_init().
 */
void pmm_init(uint32_t mem_bytes, uint32_t kernel_end);

/*
 * pmm_alloc_page — return the physical address of one free 4 KB page.
 *
 * The page is marked used.  Returns NULL (0) if no free pages remain.
 * The caller is responsible for zeroing the page if required.
 */
void *pmm_alloc_page(void);

/*
 * pmm_free_page — return a previously allocated page to the free pool.
 *
 * addr must be page-aligned (a multiple of PMM_PAGE_SIZE) and must have been
 * returned by a prior pmm_alloc_page() call.  Freeing an already-free page
 * or an address that was never allocated is silently ignored.
 */
void pmm_free_page(void *addr);

/*
 * Statistics — useful for diagnostics and debugging.
 */
uint32_t pmm_get_total(void);   /* total pages tracked by the PMM          */
uint32_t pmm_get_used(void);    /* pages currently marked used              */
uint32_t pmm_get_free(void);    /* pages currently marked free              */

/* ── Reference counting (for Copy-on-Write) ──────────────────────────────── */

void     pmm_incref(uint32_t phys);    /* increment frame reference count    */
uint32_t pmm_decref(uint32_t phys);    /* decrement; frees frame if count==0 */
uint32_t pmm_getref(uint32_t phys);    /* read current reference count       */

#endif /* PMM_H */
