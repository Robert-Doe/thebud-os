/*
 * mmap.c -- Virtual memory region table implementation
 *
 * Module 22: Simple linear-scan region table.  No tree, no red-black.
 * Physical frames are NOT allocated here -- they are faulted in on first access
 * by process_fault_handler, which calls paging_set_user_page().
 */

#include <stdint.h>
#include "mmap.h"

void mmap_init_table(struct vm_region *table) {
    int i;
    for (i = 0; i < MAX_REGIONS; i++) {
        table[i].base   = 0;
        table[i].length = 0;
        table[i].prot   = 0;
        table[i].valid  = 0;
    }
}

uint32_t mmap_reserve(struct vm_region *table, uint32_t hint, uint32_t length, int prot) {
    int i;
    uint32_t base;
    /* Round up length to page boundary */
    length = (length + 0xFFFu) & ~0xFFFu;
    base   = hint & ~0xFFFu;

    for (i = 0; i < MAX_REGIONS; i++) {
        if (!table[i].valid) {
            table[i].base   = base;
            table[i].length = length;
            table[i].prot   = prot;
            table[i].valid  = 1;
            return base;
        }
    }
    return 0;
}

int mmap_find(struct vm_region *table, uint32_t addr) {
    int i;
    for (i = 0; i < MAX_REGIONS; i++) {
        if (!table[i].valid) continue;
        if (addr >= table[i].base && addr < table[i].base + table[i].length)
            return table[i].prot;
    }
    return -1;
}
