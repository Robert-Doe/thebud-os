/*
 * syscall.c — System call dispatch table and implementations
 *
 * How control flows here
 * ──────────────────────
 * 1. A process executes `int $0x80`.
 * 2. CPU pushes EFLAGS, CS, EIP; clears IF; looks up IDT[0x80].
 * 3. isr_common_stub pushes all remaining registers → struct interrupt_frame.
 * 4. interrupt_handler(frame) sees vec==128, calls syscall_dispatch(frame).
 * 5. syscall_dispatch reads frame->eax (the syscall number) and dispatches.
 * 6. The handler writes its result into frame->eax.
 * 7. syscall_dispatch returns an ESP (usually (uint32_t)frame, unchanged).
 * 8. isr_common_stub does mov esp, eax → restores registers → iret.
 * 9. The caller's EAX now holds the syscall return value.
 *
 * Writing to frame->eax is the key insight: isr_common_stub will execute
 * `popa` using whatever values are on the stack.  frame->eax IS the memory
 * location that `popa` will load into EAX.  So storing a value there before
 * returning is how the kernel passes a value back to the caller.
 */

#include <stdint.h>
#include "syscall.h"
#include "isr.h"
#include "vga.h"
#include "process.h"

/* ── SYS_WRITE (1) ────────────────────────────────────────────────────── */

/*
 * write(fd, buf, len)
 *
 * fd == 1 (stdout) → write len bytes from buf to the VGA text console.
 * Any other fd returns SYSCALL_EBADF.
 *
 * buf is a pointer the caller provides.  In ring 0 this is a direct memory
 * address — no user-space page mapping validation needed yet.  When ring-3
 * processes arrive, we would first verify buf is in user-accessible pages.
 */
static int do_write(int fd, const char *buf, int len) {
    int i;
    if (fd != 1) return SYSCALL_EBADF;
    if (!buf || len <= 0) return SYSCALL_EINVAL;
    for (i = 0; i < len; i++) {
        vga_putchar(buf[i]);
    }
    return len;
}

/* ── SYS_EXIT (2) ─────────────────────────────────────────────────────── */

/*
 * exit(code)
 *
 * Terminates the current process.  Returns the NEXT process's saved ESP
 * so that isr_common_stub switches to it instead of returning to the dead
 * process.  process_exit() never returns.
 */
static uint32_t do_exit(uint32_t current_esp, int code) {
    (void)code;   /* exit code not used yet — no parent to collect it */
    return process_exit(current_esp);
}

/* ── SYS_GETPID (3) ───────────────────────────────────────────────────── */

static uint32_t do_getpid(void) {
    return process_current_pid();
}

/* ── SYS_YIELD (4) ────────────────────────────────────────────────────── */

/*
 * yield()
 *
 * Voluntarily gives up the remainder of the calling process's time slice.
 * Implemented by calling the same scheduler_tick path used by the timer.
 * We call it through process.h's scheduler_tick which takes the current ESP.
 */
static uint32_t do_yield(uint32_t current_esp) {
    return scheduler_tick(current_esp);
}

/* ── Central dispatcher ───────────────────────────────────────────────── */

uint32_t syscall_dispatch(struct interrupt_frame *frame) {
    uint32_t num = frame->eax;       /* syscall number                    */
    int      a   = (int)frame->ebx;  /* argument 1                        */
    int      b   = (int)frame->ecx;  /* argument 2                        */
    int      c   = (int)frame->edx;  /* argument 3                        */

    switch (num) {

        case SYS_EXIT:
            /* do_exit switches to another process — returns a NEW esp.
             * We must NOT touch frame->eax after this because 'frame' points
             * into the now-dead process's stack.  Return immediately. */
            return do_exit((uint32_t)frame, a);

        case SYS_WRITE:
            frame->eax = (uint32_t)do_write(a, (const char *)b, c);
            break;

        case SYS_GETPID:
            frame->eax = do_getpid();
            break;

        case SYS_YIELD:
            /* do_yield returns a (possibly different) esp. */
            return do_yield((uint32_t)frame);

        default:
            frame->eax = (uint32_t)SYSCALL_ENOSYS;
            break;
    }

    return (uint32_t)frame;   /* no stack switch needed */
}

/* ── Init ─────────────────────────────────────────────────────────────── */

void syscall_init(void) {
    isr_set_syscall_handler(syscall_dispatch);
}
