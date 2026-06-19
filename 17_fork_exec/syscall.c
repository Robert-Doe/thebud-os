/*
 * syscall.c — System call dispatch and implementations
 *
 * Module 17 additions
 * ───────────────────
 * SYS_FORK — delegates to process_fork(frame).  process_fork sets frame->eax
 *   to the child PID and returns the parent's unchanged ESP.  The child's copy
 *   of the frame already has EAX=0 baked in.
 *
 * SYS_WAIT — delegates to process_wait().  If a zombie child exists the call
 *   returns immediately with the exit code.  Otherwise the process blocks
 *   (PROC_WAITING) and is woken by process_exit() when its child dies; the
 *   exit code is written directly into the waiting process's saved EAX by
 *   process_exit(), so no extra mechanism is needed to pass it back.
 *
 * SYS_EXIT — now calls process_exit_with_code() so the exit code is stored
 *   in the PCB before the process becomes a zombie.
 */

#include <stdint.h>
#include "syscall.h"
#include "isr.h"
#include "vga.h"
#include "process.h"
#include "elf_loader.h"
#include "paging.h"
#include "heap.h"

#define GDT_SEL_USER_CODE  0x1Bu
#define GDT_SEL_USER_DATA  0x23u
#define INITIAL_EFLAGS     0x00000202u
#define EXEC_USER_STACK_TOP 0x800000u

/* ── SYS_WRITE ─────────────────────────────────────────────────────────── */

static int do_write(int fd, const char *buf, int len) {
    int i;
    if (fd != 1) return SYSCALL_EBADF;
    if (!buf || len <= 0) return SYSCALL_EINVAL;
    for (i = 0; i < len; i++) vga_putchar(buf[i]);
    return len;
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
    /*
     * If process_wait returned current_esp, a zombie was found and reaped
     * immediately — write the exit code into EAX now.
     * If it returned a different ESP (blocked), the parent will be woken by
     * process_exit() which writes EAX directly; we don't touch frame->eax.
     */
    if (new_esp == current_esp)
        frame->eax = exit_code;
    return new_esp;
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

        default:
            frame->eax = (uint32_t)SYSCALL_ENOSYS;
            break;
    }

    return (uint32_t)frame;
}

void syscall_init(void) {
    isr_set_syscall_handler(syscall_dispatch);
}
