/*
 * syscall.h — System call interface for BobOS
 *
 * A system call is the only legitimate way for (future) user-mode code to
 * request kernel services.  The mechanism on x86 is:
 *
 *   1. Caller loads syscall number into EAX, arguments into EBX, ECX, EDX.
 *   2. Caller executes  `int 0x80`.
 *   3. CPU looks up IDT vector 128 (0x80) — our SYSCALL gate (DPL=3).
 *   4. isr_common_stub saves all registers onto the caller's stack.
 *   5. interrupt_handler() dispatches to syscall_dispatch().
 *   6. syscall_dispatch() reads EAX, calls the appropriate handler.
 *   7. The handler writes its return value into frame->eax.
 *   8. isr_common_stub restores registers (including the new EAX) and irets.
 *   9. Caller sees the return value in EAX.
 *
 * Calling convention (same as Linux i386 ABI for familiarity):
 *   EAX = syscall number
 *   EBX = argument 1
 *   ECX = argument 2
 *   EDX = argument 3
 *   Return value in EAX (negative = error)
 *
 * Since all code is currently in ring 0, the `int 0x80` approach works
 * identically for both ring-0 kernel threads and future ring-3 user processes.
 * The DPL=3 gate descriptor means ring-3 code can call it without a #GP.
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "isr.h"

/* ── Syscall numbers ──────────────────────────────────────────────────── */

#define SYS_EXIT    1   /* exit(int code)                         */
#define SYS_WRITE   2   /* write(int fd, const char *buf, int len)*/
#define SYS_GETPID  3   /* getpid() → uint32_t pid                */
#define SYS_YIELD   4   /* yield() — voluntarily give up CPU      */

/* ── Error codes (returned as negative values in EAX) ─────────────────── */

#define SYSCALL_OK      0
#define SYSCALL_EBADF  -1   /* bad file descriptor */
#define SYSCALL_EINVAL -2   /* invalid argument    */
#define SYSCALL_ENOSYS -3   /* syscall not implemented */

/* ── Inline syscall() helper ──────────────────────────────────────────── */

/*
 * syscall(num, a, b, c) — issue int 0x80 with up to three arguments.
 *
 * The `volatile` on the asm block prevents GCC from assuming the call has
 * no side effects and reordering or removing it.  The "memory" clobber tells
 * GCC that the kernel may read or write any memory it can reach through the
 * arguments — important for SYS_WRITE where `buf` is a pointer the kernel
 * will dereference.
 *
 * This function lives in the header as `static inline` so any file that
 * includes syscall.h can issue syscalls without a separate linkage step.
 * In a real OS this would live in a user-space libc, not in the kernel.
 */
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

/* Convenience wrappers — mirror the names real OSes expose. */
static inline void sys_exit(int code)                     { syscall(SYS_EXIT,   code, 0,   0);   }
static inline int  sys_write(int fd, const char *b, int n){ return syscall(SYS_WRITE,  fd,  (int)b, n); }
static inline int  sys_getpid(void)                       { return syscall(SYS_GETPID, 0,   0,   0); }
static inline void sys_yield(void)                        { syscall(SYS_YIELD,  0,   0,   0);   }

/* ── Kernel-side API ──────────────────────────────────────────────────── */

/*
 * syscall_init() — register the syscall dispatch handler with isr.c.
 * Must be called after idt_init() and before sti.
 */
void syscall_init(void);

/*
 * syscall_dispatch(frame) — called by interrupt_handler() for vector 0x80.
 * Reads frame->eax (syscall number) and frame->ebx/ecx/edx (arguments).
 * Writes the return value into frame->eax.
 * Returns the ESP to restore (same frame unless switching process on SYS_EXIT).
 */
uint32_t syscall_dispatch(struct interrupt_frame *frame);

#endif /* SYSCALL_H */
