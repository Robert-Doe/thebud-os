/*
 * syscall.c — System call dispatch and implementations
 *
 * Module 18 additions
 * ───────────────────
 * SYS_SIGNAL (8) — delegates to process_set_handler(signo, handler_addr).
 *   The handler address is a function pointer in the user process's address
 *   space.  scheduler_tick will call it the next time the process is selected
 *   and has the corresponding signal pending.
 *
 * SYS_KILL   (9) — delegates to process_send_signal(pid, signo).
 *   Sets the signal pending on the target process.  SIGKILL for non-current
 *   processes takes effect immediately (zombie + address space freed).
 *   All other signals are delivered asynchronously on the target's next tick.
 */

#include <stdint.h>
#include "syscall.h"
#include "isr.h"
#include "vga.h"
#include "process.h"
#include "elf_loader.h"
#include "paging.h"
#include "heap.h"
#include "vfs.h"

#define GDT_SEL_USER_CODE   0x1Bu
#define GDT_SEL_USER_DATA   0x23u
#define INITIAL_EFLAGS      0x00000202u
#define EXEC_USER_STACK_TOP 0x800000u

/* ── SYS_WRITE ─────────────────────────────────────────────────────────── */

static int do_write(int fd, const char *buf, int len) {
    int r;
    if (!buf || len <= 0) return SYSCALL_EINVAL;
    r = vfs_write(fd, buf, len);
    return (r < 0) ? SYSCALL_EBADF : r;
}

/* ── SYS_OPEN ──────────────────────────────────────────────────────────── */

static int do_open(const char *path, int flags) {
    int fd;
    if (!path) return SYSCALL_EINVAL;
    fd = vfs_open(path, flags);
    return (fd < 0) ? SYSCALL_ENOENT : fd;
}

/* ── SYS_CLOSE ─────────────────────────────────────────────────────────── */

static int do_close(int fd) {
    vfs_close(fd);
    return SYSCALL_OK;
}

/* ── SYS_READ ──────────────────────────────────────────────────────────── */

static int do_read(int fd, char *buf, int len) {
    int r;
    if (!buf || len <= 0) return SYSCALL_EINVAL;
    r = vfs_read(fd, buf, len);
    return (r < 0) ? SYSCALL_EBADF : r;
}

/* ── SYS_EXIT ──────────────────────────────────────────────────────────── */

static uint32_t do_exit(uint32_t current_esp, int code) {
    return process_exit_with_code(current_esp, (uint32_t)code);
}

/* ── SYS_EXEC ──────────────────────────────────────────────────────────── */

static uint32_t do_exec(const char *filename, struct interrupt_frame *frame) {
    uint32_t  entry, new_cr3;
    uint32_t *sp;
    int       r;

    r = elf_load(filename, &entry, &new_cr3);
    if (r == -1) { frame->eax = (uint32_t)SYSCALL_ENOENT;  return (uint32_t)frame; }
    if (r == -2) { frame->eax = (uint32_t)SYSCALL_ENOEXEC; return (uint32_t)frame; }
    if (r == -3) { frame->eax = (uint32_t)SYSCALL_ENOMEM;  return (uint32_t)frame; }

    paging_set_user_page(new_cr3, EXEC_USER_STACK_TOP - 0x1000u);
    process_set_cr3(new_cr3);
    paging_switch(new_cr3);

    sp = (uint32_t *)process_kernel_stack_top();
    *--sp = GDT_SEL_USER_DATA;
    *--sp = EXEC_USER_STACK_TOP;
    *--sp = INITIAL_EFLAGS;
    *--sp = GDT_SEL_USER_CODE;
    *--sp = entry;
    *--sp = 0; *--sp = 0;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    *--sp = GDT_SEL_USER_DATA; *--sp = GDT_SEL_USER_DATA;
    *--sp = GDT_SEL_USER_DATA; *--sp = GDT_SEL_USER_DATA;
    return (uint32_t)sp;
}

/* ── SYS_FORK ──────────────────────────────────────────────────────────── */

static uint32_t do_fork(struct interrupt_frame *frame) {
    return process_fork(frame);
}

/* ── SYS_WAIT ──────────────────────────────────────────────────────────── */

static uint32_t do_wait(uint32_t current_esp, struct interrupt_frame *frame) {
    uint32_t exit_code = 0;
    uint32_t new_esp   = process_wait(current_esp, &exit_code);
    if (new_esp == current_esp)
        frame->eax = exit_code;
    return new_esp;
}

/* ── SYS_SIGNAL ────────────────────────────────────────────────────────── */

static int do_signal(int signo, uint32_t handler_addr) {
    if (signo <= 0 || signo >= NSIG || signo == SIGKILL)
        return SYSCALL_EINVAL;
    process_set_handler(signo, handler_addr);
    return SYSCALL_OK;
}

/* ── SYS_KILL ──────────────────────────────────────────────────────────── */

static int do_kill(uint32_t pid, int signo) {
    if (signo <= 0 || signo >= NSIG) return SYSCALL_EINVAL;
    process_send_signal(pid, signo);
    return SYSCALL_OK;
}

/* ── Central dispatcher ───────────────────────────────────────────────── */

uint32_t syscall_dispatch(struct interrupt_frame *frame) {
    uint32_t num = frame->eax;
    int      a   = (int)frame->ebx;
    int      b   = (int)frame->ecx;
    int      c   = (int)frame->edx;
    (void)c;

    switch (num) {
        case SYS_EXIT:
            return do_exit((uint32_t)frame, a);

        case SYS_WRITE:
            frame->eax = (uint32_t)do_write(a, (const char *)b, (int)frame->edx);
            break;

        case SYS_GETPID:
            frame->eax = process_current_pid();
            break;

        case SYS_YIELD:
            return scheduler_tick((uint32_t)frame);

        case SYS_EXEC:
            return do_exec((const char *)a, frame);

        case SYS_FORK:
            return do_fork(frame);

        case SYS_WAIT:
            return do_wait((uint32_t)frame, frame);

        case SYS_SIGNAL:
            frame->eax = (uint32_t)do_signal(a, (uint32_t)b);
            break;

        case SYS_KILL:
            frame->eax = (uint32_t)do_kill((uint32_t)a, b);
            break;

        case SYS_OPEN:
            frame->eax = (uint32_t)do_open((const char *)a, b);
            break;

        case SYS_CLOSE:
            frame->eax = (uint32_t)do_close(a);
            break;

        case SYS_READ:
            frame->eax = (uint32_t)do_read(a, (char *)b, (int)frame->edx);
            break;

        default:
            frame->eax = (uint32_t)SYSCALL_ENOSYS;
            break;
    }

    return (uint32_t)frame;
}

void syscall_init(void) {
    isr_set_syscall_handler(syscall_dispatch);
}
