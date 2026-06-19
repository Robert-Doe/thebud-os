/*
 * mmap.h -- Anonymous virtual memory region table for BobOS
 *
 * Module 22: Per-process table of reserved virtual address regions.
 * Physical frames are allocated on first access (demand-paged fault).
 * The region table lives in the PCB -- no dynamic allocation needed.
 */

#ifndef MMAP_H
#define MMAP_H
#include <stdint.h>

#define MAX_REGIONS 8
#define PROT_READ   1
#define PROT_WRITE  2
#define PROT_EXEC   4

struct vm_region {
    uint32_t base;
    uint32_t length;
    int      prot;
    int      valid;
};

/* Initialize (zero) a region table -- call once per process creation */
void mmap_init_table(struct vm_region *table);

/*
 * mmap_reserve -- record a virtual address reservation.
 * hint: preferred base address (rounded down to page boundary).
 * Returns the actual base address, or 0 on failure.
 */
uint32_t mmap_reserve(struct vm_region *table, uint32_t hint, uint32_t length, int prot);

/*
 * mmap_find -- check whether addr falls inside any reserved region.
 * Returns the prot flags if found, or -1 if addr is not mapped.
 */
int mmap_find(struct vm_region *table, uint32_t addr);

#endif /* MMAP_H */
