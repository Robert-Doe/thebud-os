/*
 * syscall.h — System call interface for BobOS
 *
 * Module 17 additions:
 *   SYS_FORK (6) — clone the calling process; returns child PID to parent, 0 to child.
 *   SYS_WAIT (7) — block until any child exits; returns its exit code.
 *
 * Calling convention (Linux i386 ABI):
 *   EAX = syscall number, EBX = arg1, ECX = arg2, EDX = arg3
 *   Return value in EAX (negative = error)
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "isr.h"

#define SYS_EXIT    1
#define SYS_WRITE   2
#define SYS_GETPID  3
#define SYS_YIELD   4
#define SYS_EXEC    5
#define SYS_FORK    6
#define SYS_WAIT    7

#define SYSCALL_OK       0
#define SYSCALL_EBADF   -1
#define SYSCALL_EINVAL  -2
#define SYSCALL_ENOSYS  -3
#define SYSCALL_ENOENT  -4
#define SYSCALL_ENOEXEC -5
#define SYSCALL_ENOMEM  -6
#define SYSCALL_EAGAIN  -7

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
static inline int  sys_exec(const char *path)              { return syscall(SYS_EXEC,   (int)path, 0, 0); }
static inline int  sys_fork(void)                          { return syscall(SYS_FORK,   0,   0,   0); }
static inline int  sys_wait(void)                          { return syscall(SYS_WAIT,   0,   0,   0); }

void     syscall_init(void);
uint32_t syscall_dispatch(struct interrupt_frame *frame);

#endif /* SYSCALL_H */
