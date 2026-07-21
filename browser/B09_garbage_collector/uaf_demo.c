#include "uaf_demo.h"
#include "heap.h"
#include "gc.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ─── Use-After-Free Demonstration ──────────────────────────────────────────
 *
 * The classic browser UAF pattern:
 *
 *  1. Allocate object A (e.g., a DOM node or JS object).
 *  2. Keep a raw C pointer to A's data.
 *  3. Drop the GC-visible reference to A (set the JS handle to null).
 *  4. Trigger a GC.  A is now unreachable — GC frees it.
 *  5. Allocate object C.  The allocator reuses A's old memory.
 *  6. C initialises that memory with its own data (0xCC...).
 *  7. Access through the raw pointer from step 2.
 *     → Reads C's data, not A's.  This is use-after-free.
 *
 * In a real browser this lets the attacker:
 *  - Read controlled data through the stale pointer (infoleak).
 *  - Groom the heap so that a JS object (with attacker-chosen properties)
 *    lands at the freed address, turning the UAF into an arbitrary read/write.
 */

void uaf_demo_run(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║         B9 — Use-After-Free (UAF) Demo               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    heap_init();

    /* ── Step 1: Allocate objects A and B ── */
    printf("Step 1: Allocate objects A and B (64 bytes each)\n");
    struct gc_object *A = gc_alloc(64);
    struct gc_object *B = gc_alloc(64);
    memset(A->data, 0x41, 64);  /* 'A' pattern */
    memset(B->data, 0x42, 64);  /* 'B' pattern */

    printf("  A at %p, data[0] = 0x%02X ('A')\n", (void *)A, (uint8_t)A->data[0]);
    printf("  B at %p, data[0] = 0x%02X ('B')\n\n", (void *)B, (uint8_t)B->data[0]);

    /* ── Step 2: Save a raw pointer to A's data ── */
    printf("Step 2: Save raw C pointer to A->data (outside GC knowledge)\n");
    char *raw_ptr = A->data;
    printf("  raw_ptr = %p  (points into A->data)\n", (void *)raw_ptr);
    printf("  raw_ptr[0] = 0x%02X (currently A's data)\n\n", (uint8_t)raw_ptr[0]);

    /* B holds a GC-visible reference to A (simulating a JS property) */
    gc_add_ref(B, 0, A);

    /* ── Step 3: Drop the GC reference to A ── */
    printf("Step 3: Drop GC reference to A (simulate: jsA = null)\n");
    gc_add_ref(B, 0, NULL);  /* B no longer references A */
    A = NULL;                 /* GC root variable cleared */
    printf("  A = NULL, B->refs[0] = NULL.  A is now unreachable.\n\n");

    /* ── Step 4: Trigger GC ── */
    printf("Step 4: Trigger GC collection\n");
    struct gc_object *roots[] = {B};
    gc_collect(roots, 1);
    printf("  A's memory has been freed and zeroed.\n\n");

    /* ── Step 5: Allocate C — likely reuses A's old slot ── */
    printf("Step 5: Allocate object C (64 bytes) — may reuse A's memory\n");
    struct gc_object *C = gc_alloc(64);
    memset(C->data, 0xCC, 64);  /* 'CC' pattern — new owner's data */
    printf("  C at %p, data[0] = 0x%02X\n\n", (void *)C, (uint8_t)C->data[0]);

    /* ── Step 6: The UAF read ── */
    printf("Step 6: USE-AFTER-FREE — read through raw_ptr\n");
    printf("  We wrote 0x41 ('A') to A->data before freeing it.\n");
    printf("  raw_ptr[0] now reads: 0x%02X\n", (uint8_t)raw_ptr[0]);

    /* Detect what happened */
    if ((uint8_t)raw_ptr[0] == 0xCC) {
        printf("  CONFIRMED UAF: raw_ptr reads 0xCC — C's data is visible through A's old pointer!\n");
        printf("  Object C has overwritten A's freed memory.\n");
    } else if ((uint8_t)raw_ptr[0] == 0x00) {
        printf("  Memory was zeroed on free (our heap zeroes on free for safety).\n");
        printf("  In a real malloc (without zeroing), C's data would be visible.\n");
    } else {
        printf("  Memory now contains: 0x%02X (allocator-dependent behaviour).\n",
               (uint8_t)raw_ptr[0]);
    }

    printf("\n");

    /* ── Step 7: Heap grooming explanation ── */
    printf("Step 7: Heap Grooming (how attackers weaponise UAF)\n");
    printf("\n");
    printf("  In a browser, the attacker controls JavaScript object allocation.\n");
    printf("  The exploit sequence:\n");
    printf("\n");
    printf("  1. Trigger the UAF (e.g., callback fires after node is freed).\n");
    printf("  2. Spray many JS objects of exactly the same size as the freed object.\n");
    printf("     The allocator puts one of them at the freed address.\n");
    printf("  3. The attacker controls the content of that new JS object.\n");
    printf("  4. The stale raw_ptr now reads/writes the attacker's JS object.\n");
    printf("  5. By choosing the object type carefully, the attacker can:\n");
    printf("     a. Read its type pointer → infoleak (ASLR bypass)\n");
    printf("     b. Overwrite its length field → out-of-bounds access\n");
    printf("     c. Overwrite its vtable pointer → code execution\n");
    printf("\n");
    printf("  This is why most browser CVEs are UAF bugs: GC languages create\n");
    printf("  the illusion of memory safety at the JavaScript level, but the\n");
    printf("  C++ engine layer maintains raw pointers that can escape the GC.\n");
    printf("\n");

    /* ── Step 8: Mitigation ── */
    printf("Step 8: Mitigation — PartitionAlloc (Chrome's heap)\n");
    printf("\n");
    printf("  PartitionAlloc separates objects by type into different memory\n");
    printf("  partitions.  A freed DOM node can only be reused by another DOM\n");
    printf("  node, not by an attacker-controlled JS object.\n");
    printf("\n");
    printf("  This doesn't fix UAF but prevents cross-type confusion:\n");
    printf("  the attacker can't put a JS ArrayBuffer where a DOM node was.\n");
    printf("  Combined with MiraclePtr (raw_ptr checked before use),\n");
    printf("  Chrome aims to make UAF non-exploitable.\n");

    heap_dump_stats();
    heap_destroy();
}
