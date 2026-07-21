/*
 * syscall.h — System call interface for BobOS
 *
 * Module 16 addition:
 *   SYS_EXEC (5) — load a named ELF executable from BobFS and replace the
 *   current process's address space with it.  The calling process's old page
 *   directory is abandoned; a fresh one is built from the ELF segments.
 *   Execution begins at the ELF entry point at ring 3.
 *
 * Calling convention (Linux i386 ABI):
 *   EAX = syscall number
 *   EBX = argument 1
 *   ECX = argument 2
 *   EDX = argument 3
 *   Return value in EAX (negative = error)
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "isr.h"

/* ── Syscall numbers ──────────────────────────────────────────────────── */

#define SYS_EXIT    1   /* exit(int code)                          */
#define SYS_WRITE   2   /* write(int fd, const char *buf, int len) */
#define SYS_GETPID  3   /* getpid() → uint32_t pid                 */
#define SYS_YIELD   4   /* yield() — voluntarily give up CPU       */
#define SYS_EXEC    5   /* exec(const char *filename) — load ELF   */

/* ── Error codes ──────────────────────────────────────────────────────── */

#define SYSCALL_OK      0
#define SYSCALL_EBADF  -1
#define SYSCALL_EINVAL -2
#define SYSCALL_ENOSYS -3
#define SYSCALL_ENOENT -4   /* file not found                      */
#define SYSCALL_ENOEXEC -5  /* not a valid ELF executable          */
#define SYSCALL_ENOMEM -6   /* out of memory during load           */

/* ── Inline syscall() helper ──────────────────────────────────────────── */

static inline int syscall(int num, int a, int b, int c) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a"  (num), "b" (a), "c" (b), "d" (c)
        : "memory"
    );
    return ret;
}

static inline void sys_exit(int code)                      { syscall(SYS_EXIT,   code, 0,   0);   }
static inline int  sys_write(int fd, const char *b, int n) { return syscall(SYS_WRITE,  fd,  (int)b, n); }
static inline int  sys_getpid(void)                        { return syscall(SYS_GETPID, 0,   0,   0); }
static inline void sys_yield(void)                         { syscall(SYS_YIELD,  0,   0,   0);   }
static inline int  sys_exec(const char *path)              { return syscall(SYS_EXEC, (int)path, 0, 0); }

/* ── Kernel-side API ──────────────────────────────────────────────────── */

void     syscall_init(void);
uint32_t syscall_dispatch(struct interrupt_frame *frame);

#endif /* SYSCALL_H */
