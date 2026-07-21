#include "confusion_demo.h"
#include "value.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ─── Type Confusion Vulnerability Demo ────────────────────────────────────
 *
 * The js_value_t struct has a type tag AND a union.  Safe code checks the
 * tag before touching the union.  Buggy code skips the check — that is a
 * type confusion vulnerability.
 *
 * This file demonstrates the bug pattern WITHOUT actually crashing:
 * we use a controlled local buffer as the "pointer" so we can print what
 * the engine would see, rather than dereferencing a wild address.
 */

void confusion_demo_run(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║         B7 — Type Confusion Vulnerability Demo       ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* ── 1. Safe usage: always check the tag ── */
    printf("=== Safe: check the type tag before using the union ===\n");
    js_value_t safe = val_number(42.0);
    printf("  safe.type = %s\n", val_type_name(safe.type));
    if (safe.type == VAL_NUMBER) {
        printf("  safe.u.number = %g  (correct — type matched)\n\n", safe.u.number);
    }

    /* ── 2. The bug: skip the type check ── */
    printf("=== Bug: using union without checking the type tag ===\n");

    /* We create a NUMBER value whose bit pattern looks like a pointer.
     * 0x41414141 = 'AAAA' — classic magic value for demonstrations.
     *
     * In a real engine the attacker controls a JIT-compiled number constant,
     * causing this pattern to appear in a value the engine later treats as
     * a string pointer, object pointer, etc.
     */
    js_value_t confused;
    confused.type = VAL_NUMBER;               /* it IS a number */
    confused.u.number = 0.0;                  /* zero out the union first */

    /* Overwrite the low 32 bits of the double with a recognisable pattern.
     * We use memcpy to avoid UB from direct union-alias cast. */
    uint32_t pattern = 0x41414141u;
    memcpy(&confused.u.number, &pattern, sizeof(pattern));

    printf("  confused.type        = %s  (engine thinks: number)\n",
           val_type_name(confused.type));
    printf("  confused.u.number    = %g\n", confused.u.number);

    /* The BUG: read .u.string without checking .type */
    char *bad_ptr = confused.u.string;   /* ← no type check — type confusion */
    printf("  confused.u.string    = %p  (reinterpreted as pointer — WRONG)\n",
           (void *)bad_ptr);
    printf("  The bits 0x%08X are now treated as a string pointer.\n\n",
           pattern);

    /* ── 3. Controlled demonstration of what "arbitrary read" means ── */
    printf("=== Controlled read via type confusion ===\n");

    /* Instead of crashing, we put a real string in a local buffer and
     * construct a js_value_t that points to it via the number slot,
     * as if an attacker had groomed the value to point at their target. */
    char victim_data[] = "SECRET:password123";

    js_value_t forged;
    forged.type = VAL_NUMBER;   /* engine believes this is a number */
    /* Store the pointer in the double's bits via memcpy */
    uintptr_t addr = (uintptr_t)victim_data;
    memcpy(&forged.u.number, &addr, sizeof(addr));

    /* Buggy engine code: treats forged as a STRING without checking type */
    char *leaked = forged.u.string;   /* ← bug: no type check */
    printf("  Forged value tagged as NUMBER, but bits = pointer to victim_data\n");
    printf("  Leaked via type confusion: \"%s\"\n\n", leaked);

    /* ── 4. CVE-2021-21220 explanation ── */
    printf("=== CVE-2021-21220 (V8 type confusion) — What actually happened ===\n");
    printf("\n");
    printf("  In V8, JavaScript objects carry a hidden 'Map' pointer that\n");
    printf("  describes their shape (property names, types, layout).\n");
    printf("\n");
    printf("  The JIT compiler cached the Map pointer and emitted code that\n");
    printf("  skipped re-checking it on subsequent accesses — an optimisation.\n");
    printf("\n");
    printf("  The bug: a FixedArray and a JSObject had overlapping Map\n");
    printf("  representations.  By triggering a specific JIT path, an attacker\n");
    printf("  could cause the engine to treat a FixedArray (whose 'length'\n");
    printf("  field was attacker-controlled) as a regular JSObject.\n");
    printf("\n");
    printf("  Reading array[large_index] then leaked memory past the array's\n");
    printf("  true end, giving the attacker an arbitrary-read primitive.\n");
    printf("\n");
    printf("  Our demo above is the same class of bug:\n");
    printf("    VAL_NUMBER → treated as VAL_STRING → pointer dereference.\n");
    printf("  In V8: FixedArray → treated as JSObject → .length out-of-bounds.\n");
    printf("\n");

    /* ── 5. Defence summary ── */
    printf("=== Defence: Always validate the type tag ===\n");
    printf("  CORRECT:\n");
    printf("    if (v.type == VAL_STRING) { use(v.u.string); }\n");
    printf("    else { throw_type_error(); }\n\n");
    printf("  Modern V8 also uses:\n");
    printf("    • Pointer compression (64-bit ptr → 32-bit offset, wrong\n");
    printf("      offsets produce obviously invalid heap addresses)\n");
    printf("    • Type check hardening (explicit checks in every JIT fast-path)\n");
    printf("    • Sandbox (even with arb-read, can't reach OS without escaping\n");
    printf("      the V8 sandbox — see B12)\n");
}
