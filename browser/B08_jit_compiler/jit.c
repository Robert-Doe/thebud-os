#include "jit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform-specific executable memory allocation */
#ifdef __linux__
#  include <sys/mman.h>
#  include <unistd.h>
#elif defined(_WIN32)
#  include <windows.h>
#endif

void jit_buf_init(jit_buf_t *b, size_t cap) {
    b->buf = malloc(cap);
    b->len = 0;
    b->cap = cap;
    if (!b->buf) { fprintf(stderr, "jit_buf_init: OOM\n"); exit(1); }
}

void jit_buf_free(jit_buf_t *b) {
    free(b->buf);
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}

void jit_emit_byte(jit_buf_t *b, uint8_t byte) {
    if (b->len >= b->cap) {
        fprintf(stderr, "jit_buf: overflow\n");
        return;
    }
    b->buf[b->len++] = byte;
}

void jit_emit_imm32(jit_buf_t *b, uint32_t val) {
    jit_emit_byte(b, (uint8_t)(val & 0xFF));
    jit_emit_byte(b, (uint8_t)((val >> 8)  & 0xFF));
    jit_emit_byte(b, (uint8_t)((val >> 16) & 0xFF));
    jit_emit_byte(b, (uint8_t)((val >> 24) & 0xFF));
}

/* MOV EAX, imm32  →  B8 <imm32> */
void jit_emit_mov_eax_imm(jit_buf_t *b, uint32_t val) {
    jit_emit_byte(b, 0xB8);
    jit_emit_imm32(b, val);
}

/* ADD EAX, imm32  →  05 <imm32> */
void jit_emit_add_eax_imm(jit_buf_t *b, uint32_t val) {
    jit_emit_byte(b, 0x05);
    jit_emit_imm32(b, val);
}

/* SUB EAX, imm32  →  2D <imm32> */
void jit_emit_sub_eax_imm(jit_buf_t *b, uint32_t val) {
    jit_emit_byte(b, 0x2D);
    jit_emit_imm32(b, val);
}

/* RET  →  C3 */
void jit_emit_ret(jit_buf_t *b) {
    jit_emit_byte(b, 0xC3);
}

void jit_print_bytes(const jit_buf_t *b) {
    for (size_t i = 0; i < b->len; i++) {
        printf("%02X ", b->buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

jit_fn_t jit_compile(jit_buf_t *b) {
#ifdef __linux__
    /* Allocate RWX page, copy bytes, return function pointer */
    long page_size = sysconf(_SC_PAGESIZE);
    size_t alloc = (b->len + page_size - 1) & ~(page_size - 1);
    void *mem = mmap(NULL, alloc,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    memcpy(mem, b->buf, b->len);
    return (jit_fn_t)mem;
#elif defined(_WIN32)
    void *mem = VirtualAlloc(NULL, b->len,
                             MEM_COMMIT | MEM_RESERVE,
                             PAGE_EXECUTE_READWRITE);
    if (!mem) {
        fprintf(stderr, "VirtualAlloc failed: %lu\n", GetLastError());
        return NULL;
    }
    memcpy(mem, b->buf, b->len);
    return (jit_fn_t)mem;
#else
    (void)b;
    printf("  [jit_compile: not supported on this platform — bytes printed only]\n");
    return NULL;
#endif
}

void jit_fn_free(jit_fn_t fn, size_t size) {
#ifdef __linux__
    munmap((void *)fn, size);
#elif defined(_WIN32)
    VirtualFree((void *)fn, 0, MEM_RELEASE);
#else
    (void)fn; (void)size;
#endif
}
