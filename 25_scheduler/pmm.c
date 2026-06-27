/*
 * pmm.c — Physical Memory Manager implementation
 *
 * The entire state is a single flat byte array (bitmap[]) where each bit
 * represents one 4KB page.  Bit = 1 means FREE; bit = 0 means USED.
 *
 * Memory layout after pmm_init():
 *
 *   0x00000000 – 0x000FFFFF  (first 1 MB)  — always USED
 *     Contains: Real-Mode IVT, BDA, bootloader, kernel, VGA buffer, BIOS ROM.
 *     We never let the allocator hand out any of these pages.
 *
 *   0x00100000 – kernel_end              — USED (rest of kernel image)
 *     The kernel text, data, rodata, bss, and this bitmap itself live here.
 *
 *   kernel_end – mem_bytes               — FREE (available for allocation)
 *     These pages are handed out by pmm_alloc_page().
 *
 * A note on NULL:
 *   Physical address 0x00000000 is the Real-Mode IVT and is always marked
 *   USED.  So pmm_alloc_page() will never return 0x00000000; returning NULL
 *   unambiguously means "out of memory."
 */

#include "pmm.h"

/* -------------------------------------------------------------------------
 * Bitmap storage — in BSS, so zero-initialised by the C runtime.
 * Zero = USED, which is the correct safe default before pmm_init() runs.
 * ------------------------------------------------------------------------- */
static uint8_t  bitmap[PMM_MAX_PAGES / 8];   /* 4,096 bytes */
static uint32_t total_pages = 0;
static uint32_t used_count  = 0;

/* -------------------------------------------------------------------------
 * Reference counts for CoW — one uint8_t per physical frame.
 * 64MB / 4KB = 16384 frames => 16384 bytes of BSS.
 * Capped at 255 (uint8_t); that is enough for MAX_PROCESSES forks.
 * ------------------------------------------------------------------------- */
#define MAX_FRAMES 16384u
static uint8_t pmm_refcounts[MAX_FRAMES];

/* -------------------------------------------------------------------------
 * Bit manipulation helpers — these compile to single instructions.
 * ------------------------------------------------------------------------- */

static inline void page_set_free(uint32_t page) {
    bitmap[page >> 3] |= (uint8_t)(1u << (page & 7u));
}

static inline void page_set_used(uint32_t page) {
    bitmap[page >> 3] &= (uint8_t)~(1u << (page & 7u));
}

static inline int page_is_free(uint32_t page) {
    return (bitmap[page >> 3] >> (page & 7u)) & 1u;
}

/* -------------------------------------------------------------------------
 * pmm_init
 * ------------------------------------------------------------------------- */
void pmm_init(uint32_t mem_bytes, uint32_t kernel_end) {
    uint32_t i;

    /* How many 4 KB pages fit in the reported memory? */
    total_pages = mem_bytes / PMM_PAGE_SIZE;
    if (total_pages > PMM_MAX_PAGES) {
        total_pages = PMM_MAX_PAGES;
    }

    /*
     * Start with every page marked USED.
     * The bitmap is already zero (BSS), and 0 = USED, so nothing to do here
     * beyond setting the counter.
     */
    used_count = total_pages;

    /*
     * Determine the first page we are allowed to give out:
     *   - The first 1 MB (256 pages) is always reserved.
     *   - The kernel image occupies from 0x1000 up to kernel_end.
     *   - Round kernel_end UP to the next page boundary to be safe.
     *
     * We pick whichever is higher — normally kernel_end is well above 1 MB
     * is not the case here (our kernel lives below 1 MB in this loader
     * model), so the 256-page floor protects us.
     */
    uint32_t free_start = (kernel_end + PMM_PAGE_SIZE - 1u) / PMM_PAGE_SIZE;
    if (free_start < 256u) {
        free_start = 256u;   /* never give out pages 0-255 (first 1 MB) */
    }

    /* Mark every page from free_start to total_pages-1 as free */
    for (i = free_start; i < total_pages; i++) {
        page_set_free(i);
        used_count--;
    }
}

/* -------------------------------------------------------------------------
 * pmm_alloc_page — O(n/8) scan for the first free page.
 *
 * We scan byte-by-byte; a zero byte means all 8 pages in it are used, so
 * we skip it in a single comparison.  Only non-zero bytes need bit-scanning.
 * This makes the common case (mostly-used memory) fast.
 * ------------------------------------------------------------------------- */
void *pmm_alloc_page(void) {
    uint32_t byte_count = (total_pages + 7u) / 8u;
    uint32_t i;

    for (i = 0; i < byte_count; i++) {
        if (bitmap[i] == 0) {
            continue;   /* all 8 pages used — skip quickly */
        }

        /* At least one free page in this byte — find which bit */
        uint8_t b = bitmap[i];
        int bit;
        for (bit = 0; bit < 8; bit++) {
            if (b & (1u << bit)) {
                uint32_t page = i * 8u + (uint32_t)bit;
                if (page >= total_pages) {
                    return (void *)0;   /* shouldn't happen, but guard */
                }
                page_set_used(page);
                used_count++;
                if (page < MAX_FRAMES) pmm_refcounts[page] = 1;
                return (void *)(page * PMM_PAGE_SIZE);
            }
        }
    }

    return (void *)0;   /* out of memory */
}

/* -------------------------------------------------------------------------
 * pmm_free_page — mark a page free.
 * ------------------------------------------------------------------------- */
void pmm_free_page(void *addr) {
    uint32_t page = (uint32_t)addr / PMM_PAGE_SIZE;

    /* Ignore bad addresses silently */
    if (page >= total_pages) {
        return;
    }
    if (page_is_free(page)) {
        return;   /* double-free — ignore */
    }

    page_set_free(page);
    used_count--;
}

/* -------------------------------------------------------------------------
 * Statistics
 * ------------------------------------------------------------------------- */
uint32_t pmm_get_total(void) { return total_pages; }
uint32_t pmm_get_used(void)  { return used_count; }
uint32_t pmm_get_free(void)  { return total_pages - used_count; }

/* -------------------------------------------------------------------------
 * Reference counting for CoW
 * ------------------------------------------------------------------------- */
void pmm_incref(uint32_t phys) {
    uint32_t page = phys / PMM_PAGE_SIZE;
    if (page >= MAX_FRAMES) return;
    if (pmm_refcounts[page] < 255u) pmm_refcounts[page]++;
}

uint32_t pmm_decref(uint32_t phys) {
    uint32_t page = phys / PMM_PAGE_SIZE;
    if (page >= MAX_FRAMES) return 0;
    if (pmm_refcounts[page] == 0) return 0;
    pmm_refcounts[page]--;
    if (pmm_refcounts[page] == 0) {
        /* Actually free the frame */
        if (!page_is_free(page)) {
            page_set_free(page);
            used_count--;
        }
        return 0;
    }
    return pmm_refcounts[page];
}

uint32_t pmm_getref(uint32_t phys) {
    uint32_t page = phys / PMM_PAGE_SIZE;
    if (page >= MAX_FRAMES) return 0;
    return pmm_refcounts[page];
}
