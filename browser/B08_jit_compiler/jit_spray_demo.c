#include "jit_spray_demo.h"
#include "jit.h"
#include <stdio.h>
#include <stdint.h>

/* ─── JIT Spray Attack Demonstration ────────────────────────────────────────
 *
 * JIT spray exploits the fact that JIT compilers embed attacker-controlled
 * constants directly into the machine code stream.
 *
 * Example:  x + 0x90909090
 *
 * The JIT emits:
 *   B8 00 00 00 00   MOV EAX, 0
 *   05 90 90 90 90   ADD EAX, 0x90909090   ← attacker's constant
 *   C3               RET
 *
 * If the attacker can redirect execution to byte offset +1 (into the opcode
 * for ADD), they land on:
 *   90 90 90 90      NOP NOP NOP NOP   ← a NOP sled
 *   C3               RET               ← then return (or next spray sled)
 *
 * By spraying many such gadgets into executable JIT memory, an attacker
 * creates a large region where every few bytes is a valid NOP sled.
 * They then redirect EIP/RIP to anywhere in that region to land on shellcode.
 *
 * Mitigation: constant blinding — the JIT XORs every constant with a random
 * mask before emitting, so the attacker cannot predict the byte pattern.
 *   Instead of:  05 90 90 90 90
 *   Emit:        B8 <masked_val>  XOR EAX, <mask>  05 <actual_val>
 */

void jit_spray_demo_run(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║              B8 — JIT Spray Demo                     ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* ── Compile: EAX = 0 + 0x90909090 ── */
    jit_buf_t b;
    jit_buf_init(&b, 256);

    uint32_t attacker_constant = 0x90909090u; /* four NOP bytes */

    jit_emit_mov_eax_imm(&b, 0);             /* MOV EAX, 0     */
    jit_emit_add_eax_imm(&b, attacker_constant); /* ADD EAX, 0x90909090 */
    jit_emit_ret(&b);                         /* RET            */

    printf("Compiled code for: EAX = 0 + 0x90909090\n");
    printf("Emitted bytes:\n  ");
    jit_print_bytes(&b);

    printf("Annotated:\n");
    printf("  Offset 0: B8 00 00 00 00  -> MOV EAX, 0\n");
    printf("  Offset 5: 05 90 90 90 90  -> ADD EAX, 0x90909090\n");
    printf("  Offset 10: C3             -> RET\n\n");

    printf("Attack: jump to offset +6 (skip the 05 opcode byte):\n");
    printf("  Offset 6:  90             -> NOP\n");
    printf("  Offset 7:  90             -> NOP\n");
    printf("  Offset 8:  90             -> NOP\n");
    printf("  Offset 9:  90             -> NOP\n");
    printf("  Offset 10: C3             -> RET\n\n");
    printf("  The attacker controls 0x90909090 and has created a NOP sled\n");
    printf("  inside executable JIT memory just by choosing that constant!\n\n");

    /* Spray pattern: compile many such functions, creating many NOP sleds */
    printf("Spray: if we repeat this gadget 1000 times across JIT memory,\n");
    printf("  every ~11 bytes of executable memory contains a landing point.\n");
    printf("  Attacker sprays shellcode as: NOP sled + shellcode + NOP sled...\n\n");

    /* ── Execute the compiled function (just to show it actually runs) ── */
    jit_fn_t fn = jit_compile(&b);
    if (fn) {
        int result = fn();
        printf("Executed JIT function: returned 0x%08X  (= 0x90909090 = %d)\n\n",
               (uint32_t)result, result);
        jit_fn_free(fn, b.len);
    } else {
        printf("(JIT execution skipped — platform not supported)\n\n");
    }

    /* ── Mitigation: constant blinding ── */
    printf("Mitigation — Constant Blinding:\n");
    printf("  Instead of emitting: 05 90 90 90 90  (ADD EAX, constant)\n");
    printf("  The JIT emits:\n");
    printf("    B8 <constant XOR mask>   MOV EAX, blinded\n");
    printf("    35 <mask>                XOR EAX, mask\n");
    printf("    05 <actual constant>     ADD EAX, actual\n");
    printf("  Now the attacker cannot predict which bytes appear in JIT memory.\n\n");

    /* Show what blinded emission looks like */
    uint32_t mask = 0xDEADBEEFu;
    uint32_t blinded = attacker_constant ^ mask;
    jit_buf_t b2;
    jit_buf_init(&b2, 256);
    jit_emit_mov_eax_imm(&b2, blinded);     /* MOV EAX, blinded */
    /* XOR EAX, mask: 35 <imm32> */
    jit_emit_byte(&b2, 0x35);
    jit_emit_imm32(&b2, mask);
    jit_emit_add_eax_imm(&b2, attacker_constant); /* ADD EAX, constant */
    jit_emit_ret(&b2);

    printf("Blinded emission bytes (mask = 0x%08X):\n  ", mask);
    jit_print_bytes(&b2);
    printf("  Bytes at attacker-controlled offsets are now unpredictable.\n\n");

    jit_buf_free(&b);
    jit_buf_free(&b2);
}
