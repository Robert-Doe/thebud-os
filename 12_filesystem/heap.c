/*
 * heap.c — Kernel Heap Allocator (free-list, first-fit)
 *
 * The allocator manages a contiguous pool of pages supplied by the PMM.
 * Because those pages are within the identity-mapped first 4 MB, every
 * physical address is also a valid virtual address — no page-table changes
 * are needed here.
 *
 * Free-list design:
 *
 *   Every region of memory (free or used) begins with an 8-byte header:
 *
 *     struct block_header {
 *         uint32_t size;   // bytes of DATA in this block (not including header)
 *         uint32_t flags;  // bit 0: 1 = used, 0 = free
 *     };
 *
 *   Headers are stored INLINE in the heap pool — no separate metadata array.
 *   Traversal: next block = (char*)current + sizeof(header) + current->size
 *   The last header is a sentinel: size=0, flags=USED.  It marks the end.
 *
 * kmalloc — first-fit search:
 *   Walk the block list until a free block with size >= requested is found.
 *   If the block is large enough to split (leftover >= header + MIN_SPLIT),
 *   split it: mark the first part used, insert a new free header for the rest.
 *   Return pointer to the data area (header + 1).
 *
 * kfree — mark free + forward coalesce:
 *   Mark the target block free.  If the NEXT block is also free, merge them
 *   (add next->size + sizeof(header) to current->size, skip next).
 *   This prevents fragmentation over repeated alloc/free cycles.
 */

#include "heap.h"
#include "pmm.h"

/* -------------------------------------------------------------------------
 * Block header — 8 bytes, placed inline at the start of every block.
 * ------------------------------------------------------------------------- */
struct block_header {
    uint32_t size;    /* bytes of data (not including this header) */
    uint32_t flags;   /* bit 0: USED(1) or FREE(0)                 */
};

#define BLOCK_USED   1u
#define BLOCK_FREE   0u

/* Minimum data payload worth creating when splitting a block.
 * Splitting produces a new header + data; if the leftover data would be
 * smaller than this, don't split — just give the whole block to the caller
 * (internal fragmentation is acceptable here). */
#define MIN_SPLIT    16u

/* Size of the block header */
#define HDR_SIZE     ((uint32_t)sizeof(struct block_header))

/* -------------------------------------------------------------------------
 * Heap state
 * ------------------------------------------------------------------------- */
static struct block_header *heap_start = (struct block_header *)0;
static uint32_t             heap_bytes  = 0;   /* total data bytes in pool */

/* -------------------------------------------------------------------------
 * heap_init
 * ------------------------------------------------------------------------- */
void heap_init(uint32_t num_pages) {
    uint32_t i;
    char *pool;
    uint32_t pool_bytes;
    struct block_header *first;
    struct block_header *sentinel;

    if (num_pages == 0) return;

    /*
     * Allocate contiguous pages from the PMM.
     * The PMM's first-fit allocator returns pages in address order, so
     * consecutive calls produce a contiguous block (as long as those pages
     * are adjacent in the PMM bitmap, which they will be at this early stage).
     */
    pool = (char *)pmm_alloc_page();
    for (i = 1; i < num_pages; i++) {
        pmm_alloc_page();   /* claim the rest — they're contiguous */
    }
    pool_bytes = num_pages * PMM_PAGE_SIZE;

    heap_start = (struct block_header *)pool;

    /*
     * Lay out two headers in the pool:
     *
     *   [first block header] [............data............] [sentinel]
     *    HDR_SIZE bytes        pool_bytes - 2*HDR_SIZE        HDR_SIZE bytes
     *
     * The first block is FREE and covers all data bytes between the two headers.
     * The sentinel has size=0 and is USED — it acts as a stop marker for the
     * traversal loop so we never walk off the end of the pool.
     */
    heap_bytes = pool_bytes - 2u * HDR_SIZE;

    first        = heap_start;
    first->size  = heap_bytes;
    first->flags = BLOCK_FREE;

    sentinel        = (struct block_header *)((char *)first + HDR_SIZE + first->size);
    sentinel->size  = 0;
    sentinel->flags = BLOCK_USED;
}

/* -------------------------------------------------------------------------
 * kmalloc
 * ------------------------------------------------------------------------- */
void *kmalloc(uint32_t size) {
    struct block_header *blk;

    if (size == 0 || heap_start == (struct block_header *)0) return (void *)0;

    /* Round up to 4-byte alignment so the returned pointer is always aligned */
    size = (size + 3u) & ~3u;

    /* Walk the block list looking for the first free block large enough */
    blk = heap_start;
    while (blk->size != 0 || blk->flags == BLOCK_FREE) {

        /* Sentinel check: size==0 and USED = end of heap */
        if (blk->size == 0 && blk->flags == BLOCK_USED) {
            break;   /* out of memory */
        }

        if (blk->flags == BLOCK_FREE && blk->size >= size) {
            /*
             * Found a suitable free block.  Split it if the leftover would
             * be large enough to be useful (header + MIN_SPLIT data bytes).
             */
            if (blk->size >= size + HDR_SIZE + MIN_SPLIT) {
                /* Insert a new free header immediately after the used region */
                struct block_header *remainder =
                    (struct block_header *)((char *)blk + HDR_SIZE + size);
                remainder->size  = blk->size - size - HDR_SIZE;
                remainder->flags = BLOCK_FREE;
                blk->size = size;
            }
            blk->flags = BLOCK_USED;
            return (void *)(blk + 1);   /* pointer to data area */
        }

        /* Advance to next block */
        blk = (struct block_header *)((char *)blk + HDR_SIZE + blk->size);
    }

    return (void *)0;   /* out of memory */
}

/* -------------------------------------------------------------------------
 * kfree
 * ------------------------------------------------------------------------- */
void kfree(void *ptr) {
    struct block_header *blk;
    struct block_header *next;

    if (ptr == (void *)0) return;

    /* The header sits immediately before the data pointer */
    blk = (struct block_header *)ptr - 1;
    blk->flags = BLOCK_FREE;

    /*
     * Forward coalesce: if the next block is also free, merge them.
     * This is the simplest form of coalescing and prevents the heap from
     * fragmenting into many small unusable free blocks over time.
     *
     * We merge repeatedly in case two or more consecutive free blocks exist.
     */
    next = (struct block_header *)((char *)blk + HDR_SIZE + blk->size);
    while (next->flags == BLOCK_FREE && next->size > 0) {
        blk->size += HDR_SIZE + next->size;
        next = (struct block_header *)((char *)blk + HDR_SIZE + blk->size);
    }
}

/* -------------------------------------------------------------------------
 * Diagnostics — walk the block list and tally up statistics.
 * ------------------------------------------------------------------------- */
static void heap_stats(uint32_t *used, uint32_t *free_bytes, uint32_t *blocks) {
    struct block_header *blk;
    *used = 0; *free_bytes = 0; *blocks = 0;

    if (heap_start == (struct block_header *)0) return;

    blk = heap_start;
    while (!(blk->size == 0 && blk->flags == BLOCK_USED)) {
        (*blocks)++;
        if (blk->flags == BLOCK_USED)
            *used += blk->size;
        else
            *free_bytes += blk->size;
        blk = (struct block_header *)((char *)blk + HDR_SIZE + blk->size);
    }
}

uint32_t heap_get_used_bytes(void)  { uint32_t u,f,b; heap_stats(&u,&f,&b); return u; }
uint32_t heap_get_free_bytes(void)  { uint32_t u,f,b; heap_stats(&u,&f,&b); return f; }
uint32_t heap_get_total_bytes(void) { return heap_bytes; }
uint32_t heap_get_num_blocks(void)  { uint32_t u,f,b; heap_stats(&u,&f,&b); return b; }
