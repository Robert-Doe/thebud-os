#include <stdio.h>
#include "jit.h"
#include "jit_spray_demo.h"
#include "wx_demo.h"

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║        B8 — JIT Compiler Security (Spray & W^X)      ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* ── Part 1: Basic JIT emit demo ── */
    printf("=== Part 1: Basic JIT Code Emitter ===\n\n");

    jit_buf_t b;
    jit_buf_init(&b, 256);

    /* Compile: return 100 + 23 - 7 = 116 */
    jit_emit_mov_eax_imm(&b, 100);
    jit_emit_add_eax_imm(&b, 23);
    jit_emit_sub_eax_imm(&b, 7);
    jit_emit_ret(&b);

    printf("Expression: 100 + 23 - 7\n");
    printf("Emitted %zu bytes:\n  ", b.len);
    jit_print_bytes(&b);

    jit_fn_t fn = jit_compile(&b);
    if (fn) {
        int result = fn();
        printf("JIT executed: result = %d  (expected 116)\n\n", result);
        jit_fn_free(fn, b.len);
    } else {
        printf("(JIT execution not available on this platform)\n\n");
    }
    jit_buf_free(&b);

    /* ── Part 2: JIT Spray ── */
    jit_spray_demo_run();

    /* ── Part 3: W^X ── */
    wx_demo_run();

    return 0;
}
