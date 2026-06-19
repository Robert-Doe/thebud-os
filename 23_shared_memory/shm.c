/*
 * shm.c -- Shared memory implementation
 *
 * Module 23: shm_table[] holds up to MAX_SHM segments.  Each segment owns
 * physical frames allocated at shm_get() time.  shm_at() maps those frames
 * into the calling process's virtual address space using paging_map_user_phys().
 * pmm_incref() on each frame ensures they survive partial process exit.
 */

#include <stdint.h>
#include "shm.h"
#include "pmm.h"
#include "paging.h"
#include "process.h"

static struct shm_segment shm_table[MAX_SHM];

void shm_init(void) {
    int i;
    for (i = 0; i < MAX_SHM; i++) {
        shm_table[i].key       = 0;
        shm_table[i].page_count = 0;
        shm_table[i].ref_count  = 0;
        shm_table[i].valid      = 0;
    }
}

int shm_get(uint32_t key, uint32_t size) {
    int i, j;
    uint32_t pages;
    uint32_t frame;

    /* Check for existing segment with this key */
    for (i = 0; i < MAX_SHM; i++) {
        if (shm_table[i].valid && shm_table[i].key == key)
            return i;
    }

    /* Allocate new segment */
    for (i = 0; i < MAX_SHM; i++) {
        if (!shm_table[i].valid) {
            pages = (size + 0xFFFu) >> 12;
            if (pages > 4) pages = 4;
            shm_table[i].key        = key;
            shm_table[i].page_count = pages;
            shm_table[i].ref_count  = 0;
            shm_table[i].valid      = 1;
            for (j = 0; j < (int)pages; j++) {
                frame = (uint32_t)pmm_alloc_page();
                shm_table[i].phys_frames[j] = frame;
                /* incref so the frame persists across process exits */
                pmm_incref(frame);
            }
            return i;
        }
    }
    return -1;
}

uint32_t shm_at(int shm_id, uint32_t vaddr) {
    int j;
    uint32_t cr3;
    if (shm_id < 0 || shm_id >= MAX_SHM || !shm_table[shm_id].valid) return 0;

    cr3 = process_get_cr3();
    if (!cr3) return 0;

    for (j = 0; j < (int)shm_table[shm_id].page_count; j++) {
        paging_map_user_phys(cr3, vaddr + (uint32_t)(j * 4096),
                             shm_table[shm_id].phys_frames[j]);
    }
    shm_table[shm_id].ref_count++;
    return vaddr;
}
