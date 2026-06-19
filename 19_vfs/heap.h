/*
 * heap.h — Kernel Heap Allocator interface
 *
 * Provides kmalloc() / kfree() — the kernel's dynamic memory allocator.
 *
 * Implementation: a free-list allocator over a contiguous pool of physical
 * pages.  The pool is backed by pages taken from the PMM at init time.
 * Since those pages fall within the identity-mapped first 4 MB (Module 7),
 * their virtual addresses equal their physical addresses — no extra mapping
 * is required.
 *
 * Block layout in memory:
 *
 *   ┌──────────────────┐  ← heap_start
 *   │ block_header (8B)│
 *   │ ... data ...     │
 *   ├──────────────────┤
 *   │ block_header (8B)│
 *   │ ... data ...     │
 *   ├──────────────────┤
 *   │ sentinel (8B)    │  ← size=0, used=1 — marks end of heap
 *   └──────────────────┘  ← heap_start + pool_bytes
 *
 * Each block_header stores the size of the DATA region (not including the
 * header itself) and a used/free flag.  kmalloc() walks the list for a
 * first-fit free block; kfree() marks the block free and coalesces it with
 * any immediately following free block.
 */

#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

/*
 * heap_init — set up the heap over a contiguous pool of physical pages.
 *
 * Allocates `num_pages` pages from the PMM, treats them as one contiguous
 * block of memory, and initialises the free-list header.
 *
 * Call after pmm_init() and paging_init().
 */
void heap_init(uint32_t num_pages);

/*
 * kmalloc — allocate at least `size` bytes from the kernel heap.
 *
 * Returns a pointer to the allocated block, or NULL if the heap is full.
 * The allocation is 4-byte aligned.
 * The contents of the allocated block are NOT zeroed.
 */
void *kmalloc(uint32_t size);

/*
 * kfree — return a previously kmalloc'd block to the heap.
 *
 * `ptr` must be a pointer returned by kmalloc().  Freeing NULL is a no-op.
 * Adjacent free blocks are coalesced (merged) to prevent fragmentation.
 */
void kfree(void *ptr);

/*
 * Heap diagnostics — useful for verifying allocator behaviour.
 */
uint32_t heap_get_used_bytes(void);   /* bytes currently allocated           */
uint32_t heap_get_free_bytes(void);   /* bytes currently free                */
uint32_t heap_get_total_bytes(void);  /* total heap size (data bytes only)   */
uint32_t heap_get_num_blocks(void);   /* total number of blocks (free+used)  */

#endif /* HEAP_H */
