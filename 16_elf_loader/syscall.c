/*
 * syscall.c — System call dispatch and implementations
 *
 * Module 16 addition: SYS_EXEC
 * ────────────────────────────
 * do_exec() calls elf_load() to parse the named file from BobFS and map its
 * segments into a fresh page directory.  It then replaces the calling process's
 * cr3 and entry point, switches to the new PD, and returns a new fake ring-3
 * interrupt frame whose EIP points at the ELF entry.  The old PD is abandoned
 * (physical pages leaked for now — reclaiming them is Module 17's job).
 *
 * The new fake frame is built at the top of the process's existing kernel stack
 * so the ISR stub can simply restore it and iret into the new program.
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

/* User stack top for exec'd programs — 8 MB, well above the 4 MB ELF load base. */
#define EXEC_USER_STACK_TOP  0x800000u
#define EXEC_USER_STACK_SIZE 4096u

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
    (void)code;
    return process_exit(current_esp);
}

/* ── SYS_GETPID ────────────────────────────────────────────────────────── */

static uint32_t do_getpid(void) {
    return process_current_pid();
}

/* ── SYS_YIELD ─────────────────────────────────────────────────────────── */

static uint32_t do_yield(uint32_t current_esp) {
    return scheduler_tick(current_esp);
}

/* ── SYS_EXEC ──────────────────────────────────────────────────────────── */

/*
 * do_exec(filename, frame)
 *
 * Loads an ELF executable from BobFS, sets up a new address space and a fresh
 * ring-3 fake frame on the calling process's kernel stack, then returns the
 * ESP of that frame so isr_common_stub irets into the new program.
 *
 * The user stack for the new program is placed at EXEC_USER_STACK_TOP.
 * We allocate one physical page for it and map it user-accessible in the
 * new PD.
 */
static uint32_t do_exec(const char *filename, struct interrupt_frame *frame) {
    uint32_t  entry, new_cr3;
    uint32_t *sp;
    uint32_t  ustack_phys;
    int       r;

    r = elf_load(filename, &entry, &new_cr3);
    if (r == -1) { frame->eax = (uint32_t)SYSCALL_ENOENT;  return (uint32_t)frame; }
    if (r == -2) { frame->eax = (uint32_t)SYSCALL_ENOEXEC; return (uint32_t)frame; }
    if (r == -3) { frame->eax = (uint32_t)SYSCALL_ENOMEM;  return (uint32_t)frame; }

    /* Allocate and map a user stack page in the new PD. */
    ustack_phys = pmm_alloc_page();
    if (!ustack_phys) { frame->eax = (uint32_t)SYSCALL_ENOMEM; return (uint32_t)frame; }
    paging_set_user_page(new_cr3, EXEC_USER_STACK_TOP - 0x1000u);

    /* Update the current process's page directory and switch to it. */
    process_set_cr3(new_cr3);
    paging_switch(new_cr3);

    /*
     * Build a new ring-3 fake frame on top of the CURRENT kernel stack.
     * We reuse the same kernel stack — exec replaces user space, not kernel stack.
     * sp starts at the top of frame (the original kernel stack position before
     * the interrupt frame was pushed).
     *
     * Layout (high → low):
     *   user_ss
     *   user_esp       = EXEC_USER_STACK_TOP
     *   eflags         = 0x202 (IF=1)
     *   cs             = 0x1B  (ring-3 code)
     *   eip            = ELF entry point
     *   err_code = 0
     *   int_no   = 0
     *   eax..edi = 0   (8 words from pusha)
     *   ds/es/fs/gs    = 0x23
     */
    sp = (uint32_t *)process_kernel_stack_top();
    *--sp = GDT_SEL_USER_DATA;          /* user ss  */
    *--sp = EXEC_USER_STACK_TOP;        /* user esp */
    *--sp = INITIAL_EFLAGS;
    *--sp = GDT_SEL_USER_CODE;          /* cs */
    *--sp = entry;                      /* eip */
    *--sp = 0; *--sp = 0;               /* err_code, int_no */
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;  /* eax ecx edx ebx */
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;  /* esp_d ebp esi edi */
    *--sp = GDT_SEL_USER_DATA; *--sp = GDT_SEL_USER_DATA;
    *--sp = GDT_SEL_USER_DATA; *--sp = GDT_SEL_USER_DATA;  /* ds es fs gs */

    return (uint32_t)sp;
}

/* ── Central dispatcher ───────────────────────────────────────────────── */

uint32_t syscall_dispatch(struct interrupt_frame *frame) {
    uint32_t num = frame->eax;
    int      a   = (int)frame->ebx;
    int      b   = (int)frame->ecx;
    int      c   = (int)frame->edx;

    switch (num) {
        case SYS_EXIT:
            return do_exit((uint32_t)frame, a);

        case SYS_WRITE:
            frame->eax = (uint32_t)do_write(a, (const char *)b, c);
            break;

        case SYS_GETPID:
            frame->eax = do_getpid();
            break;

        case SYS_YIELD:
            return do_yield((uint32_t)frame);

        case SYS_EXEC:
            return do_exec((const char *)a, frame);

        default:
            frame->eax = (uint32_t)SYSCALL_ENOSYS;
            break;
    }

    return (uint32_t)frame;
}

/* ── Init ─────────────────────────────────────────────────────────────── */

void syscall_init(void) {
    isr_set_syscall_handler(syscall_dispatch);
}
