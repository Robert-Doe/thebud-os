/*
 * syscall.h — System call interface for BobOS
 *
 * Module 18 additions:
 *   SYS_SIGNAL (8) — install a user-space signal handler.
 *                    arg1 = signo, arg2 = handler address (or SIG_DFL/SIG_IGN).
 *   SYS_KILL   (9) — send a signal to a process.
 *                    arg1 = target pid, arg2 = signo.
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
#define SYS_SIGNAL  8
#define SYS_KILL    9
#define SYS_OPEN   10
#define SYS_CLOSE  11
#define SYS_READ   12
#define SYS_PIPE   13
#define SYS_MMAP   14

#define SYSCALL_OK       0
#define SYSCALL_EBADF   -1
#define SYSCALL_EINVAL  -2
#define SYSCALL_ENOSYS  -3
#define SYSCALL_ENOENT  -4
#define SYSCALL_ENOEXEC -5
#define SYSCALL_ENOMEM  -6
#define SYSCALL_EAGAIN  -7
#define SYSCALL_ESRCH   -8

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

static inline void sys_exit(int code)                       { syscall(SYS_EXIT,   code,    0,   0); }
static inline int  sys_write(int fd, const char *b, int n)  { return syscall(SYS_WRITE, fd, (int)b, n); }
static inline int  sys_getpid(void)                         { return syscall(SYS_GETPID, 0,  0,   0); }
static inline void sys_yield(void)                          { syscall(SYS_YIELD,  0,    0,   0); }
static inline int  sys_exec(const char *path)               { return syscall(SYS_EXEC, (int)path, 0, 0); }
static inline int  sys_fork(void)                           { return syscall(SYS_FORK,   0,    0,   0); }
static inline int  sys_wait(void)                           { return syscall(SYS_WAIT,   0,    0,   0); }
static inline int  sys_signal(int signo, void (*h)(int))    { return syscall(SYS_SIGNAL, signo, (int)h, 0); }
static inline int  sys_kill(int pid, int signo)             { return syscall(SYS_KILL, pid, signo, 0); }
static inline int  sys_open(const char *path, int flags)    { return syscall(SYS_OPEN, (int)path, flags, 0); }
static inline void sys_close(int fd)                        { syscall(SYS_CLOSE, fd, 0, 0); }
static inline int  sys_read(int fd, char *buf, int n)       { return syscall(SYS_READ, fd, (int)buf, n); }
static inline int  sys_pipe(int fds[2])                     { return syscall(SYS_PIPE, (int)fds, 0, 0); }
static inline uint32_t sys_mmap(uint32_t hint, uint32_t len, int prot) { return (uint32_t)syscall(SYS_MMAP, (int)hint, (int)len, prot); }

void     syscall_init(void);
uint32_t syscall_dispatch(struct interrupt_frame *frame);

#endif /* SYSCALL_H */
