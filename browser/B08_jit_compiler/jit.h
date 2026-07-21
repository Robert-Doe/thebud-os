#ifndef JIT_H
#define JIT_H

#include <stdint.h>
#include <stddef.h>

/* ── JIT code buffer ── */
typedef struct {
    uint8_t *buf;
    size_t   len;
    size_t   cap;
} jit_buf_t;

typedef int (*jit_fn_t)(void);

/* Buffer management */
void jit_buf_init(jit_buf_t *b, size_t cap);
void jit_buf_free(jit_buf_t *b);
void jit_emit_byte(jit_buf_t *b, uint8_t byte);
void jit_emit_imm32(jit_buf_t *b, uint32_t val);

/* x86-64 instruction emitters (System V ABI: result in EAX/RAX) */
void jit_emit_mov_eax_imm(jit_buf_t *b, uint32_t val);   /* B8 <imm32> */
void jit_emit_add_eax_imm(jit_buf_t *b, uint32_t val);   /* 05 <imm32> */
void jit_emit_sub_eax_imm(jit_buf_t *b, uint32_t val);   /* 2D <imm32> */
void jit_emit_ret(jit_buf_t *b);                          /* C3 */

/* Compile: allocate executable memory, copy bytes, return callable function.
   Returns NULL if mmap/VirtualAlloc is unavailable. */
jit_fn_t jit_compile(jit_buf_t *b);

/* Free an executable page returned by jit_compile */
void jit_fn_free(jit_fn_t fn, size_t size);

/* Print the emitted bytes as hex */
void jit_print_bytes(const jit_buf_t *b);

#endif /* JIT_H */
