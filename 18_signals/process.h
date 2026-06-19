/*
 * process.h — Process Control Block and Scheduler API
 *
 * Module 18 additions:
 *   - Signal constants (NSIG, SIG_DFL, SIG_IGN, signal numbers).
 *   - PCB gains `pending_signals` (bitmask) and `signal_handlers[NSIG]`
 *     (per-signal handler address: SIG_DFL, SIG_IGN, or a user function).
 *   - process_send_signal(pid, signo) — set signal pending on a process.
 *   - process_set_handler(signo, addr) — called by SYS_SIGNAL to install a
 *     user-space handler for the current process.
 *   - process_fault_handler(esp) — replaces the raw process_exit hook;
 *     delivers SIGSEGV to a user handler if registered, otherwise kills.
 *   - scheduler_tick now calls deliver_user_signals() before handing the CPU
 *     back to a ring-3 process so pending signals are dispatched asynchronously.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "isr.h"

#define MAX_PROCESSES   8
#define PROC_STACK_SIZE 4096u

/* ── Signal constants ───────────────────────────────────────────────────── */

#define NSIG    32          /* total signal count (1-based; 0 is unused) */

#define SIG_DFL 0u          /* default action (kill or ignore per signal) */
#define SIG_IGN 1u          /* explicitly ignore this signal              */

#define SIGHUP   1
#define SIGINT   2
#define SIGKILL  9
#define SIGUSR1  10
#define SIGSEGV  11
#define SIGUSR2  12
#define SIGTERM  15

/* ── Process state ──────────────────────────────────────────────────────── */

typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_DEAD    = 2,
    PROC_ZOMBIE  = 3,
    PROC_WAITING = 4
} proc_state_t;

/* ── Process Control Block ──────────────────────────────────────────────── */

struct process {
    uint32_t        pid;
    uint32_t        esp;
    uint8_t        *stack;
    uint32_t        kernel_stack_top;
    uint32_t        cr3;
    uint32_t        parent_pid;
    uint32_t        exit_code;
    proc_state_t    state;
    struct process *next;

    uint32_t        pending_signals;       /* bitmask: bit N = signal N pending */
    uint32_t        signal_handlers[NSIG]; /* SIG_DFL / SIG_IGN / handler addr  */
};

/* ── Core API ───────────────────────────────────────────────────────────── */

void     process_init(void);
uint32_t process_create(void (*entry)(void));
uint32_t process_fork(struct interrupt_frame *frame);
uint32_t process_wait(uint32_t current_esp, uint32_t *out_exit_code);
uint32_t scheduler_tick(uint32_t current_esp);
uint32_t process_current_pid(void);
uint32_t process_exit(uint32_t current_esp);
uint32_t process_exit_with_code(uint32_t current_esp, uint32_t code);
void     process_set_cr3(uint32_t cr3);
uint32_t process_kernel_stack_top(void);

/* ── Signal API ─────────────────────────────────────────────────────────── */

void     process_send_signal(uint32_t pid, int signo);
void     process_set_handler(int signo, uint32_t handler_addr);
uint32_t process_fault_handler(uint32_t esp);

#endif /* PROCESS_H */
