/*
 * shm.h -- Shared memory interface for BobOS
 *
 * Module 23: Key-based shared memory segments.  Multiple processes can
 * attach the same physical frames into their own virtual address spaces
 * via paging_map_user_phys().  Reference counting prevents premature
 * frame release.
 */

#ifndef SHM_H
#define SHM_H
#include <stdint.h>

#define MAX_SHM 8

struct shm_segment {
    uint32_t key;
    uint32_t phys_frames[4];  /* up to 4 pages (16KB) per segment */
    uint32_t page_count;
    int      ref_count;
    int      valid;
};

void     shm_init(void);
int      shm_get(uint32_t key, uint32_t size);         /* returns shm_id or -1 */
uint32_t shm_at(int shm_id, uint32_t vaddr);           /* map into current process; returns vaddr */

#endif /* SHM_H */
