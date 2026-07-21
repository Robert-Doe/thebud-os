#include <stdio.h>
#include "cache_timing.h"
#include "spectre_demo.h"

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║      B11 — Spectre: Cache Timing Side-Channel         ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* ── Part 1: Cache timing basics ── */
    printf("=== Part 1: Cache Timing Measurement ===\n\n");

#if defined(__x86_64__) || defined(__i386__)
    volatile uint8_t buf[4096];
    /* Touch buf to page it in */
    for (int i = 0; i < 4096; i++) buf[i] = (uint8_t)i;

    /* Warm access */
    (void)buf[0];
    uint64_t t_warm = time_access((void *)&buf[0]);

    /* Flush and measure cold */
    cache_flush((void *)&buf[512]);
    mfence();
    uint64_t t_cold = time_access((void *)&buf[512]);

    printf("  Cache HIT  (buf[0],   not flushed): %llu cycles\n",
           (unsigned long long)t_warm);
    printf("  Cache MISS (buf[512], flushed):     %llu cycles\n",
           (unsigned long long)t_cold);
    printf("  Threshold used:                     %d cycles\n\n",
           CACHE_HIT_THRESHOLD);
    printf("  HIT < threshold < MISS confirms timing channel works.\n\n");
#else
    printf("  RDTSC not available on this architecture.\n");
    printf("  Cache timing concept:\n");
    printf("    L1 cache hit:  ~4 cycles  (< %d threshold)\n", CACHE_HIT_THRESHOLD);
    printf("    L2 cache hit:  ~12 cycles\n");
    printf("    L3 cache hit:  ~40 cycles\n");
    printf("    DRAM access:   ~200+ cycles  (> threshold)\n\n");
#endif

    /* ── Part 2: Spectre attack ── */
    spectre_demo_run();

    return 0;
}
