/*
 * process.h — Process Control Block and Scheduler API
 *
 * Module 17 additions:
 *   - PCB gains `parent_pid` and `exit_code`.
 *   - New states: PROC_ZOMBIE (exited, waiting for parent to reap) and
 *     PROC_WAITING (blocked in SYS_WAIT until a child exits).
 *   - process_fork(frame) — clone the current process for SYS_FORK.
 *   - process_wait(current_esp, out_exit_code) — block until a child is
 *     zombie, reap it, return its exit code.  Called by SYS_WAIT.
 *   - process_exit now frees the address space, zombifies the process,
 *     and wakes a waiting parent if one exists.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "isr.h"

#define MAX_PROCESSES   8
#define PROC_STACK_SIZE 4096u

typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_DEAD    = 2,
    PROC_ZOMBIE  = 3,   /* exited; PCB kept until parent calls wait()  */
    PROC_WAITING = 4    /* blocked in SYS_WAIT for a child to exit      */
} proc_state_t;

struct process {
    uint32_t        pid;
    uint32_t        esp;               /* saved kernel-side ESP                   */
    uint8_t        *stack;             /* kernel stack base (kmalloc'd)           */
    uint32_t        kernel_stack_top;  /* stack + PROC_STACK_SIZE (TSS.esp0)      */
    uint32_t        cr3;               /* page directory physical address          */
    uint32_t        parent_pid;        /* PID of parent process (0 = no parent)   */
    uint32_t        exit_code;         /* set by process_exit, read by wait()     */
    proc_state_t    state;
    struct process *next;
};

void     process_init(void);
uint32_t process_create(void (*entry)(void));

/*
 * process_fork(frame) — SYS_FORK implementation.
 *
 * Clones the calling process:
 *   - Deep-copies address space via paging_clone_address_space().
 *   - Allocates a new kernel stack and copies the interrupt frame onto it
 *     so the child resumes right after the `int $0x80` instruction.
 *   - Child's EAX is set to 0; parent's EAX is set to child PID.
 *
 * Returns the (unchanged) parent ESP.  The child PCB is inserted into the
 * scheduler ring and will run on the next timer tick.
 */
uint32_t process_fork(struct interrupt_frame *frame);

/*
 * process_wait(current_esp, out_exit_code) — SYS_WAIT implementation.
 *
 * If a zombie child exists: reaps it (ZOMBIE→DEAD), writes exit_code to
 * *out_exit_code, returns current_esp (caller resumes immediately).
 *
 * If no zombie child yet: marks the calling process PROC_WAITING and calls
 * scheduler_tick to yield.  When a child later exits, process_exit() will
 * wake the parent by setting it PROC_READY and writing the exit code into
 * the parent's saved EAX field.  The parent then naturally resumes from
 * SYS_WAIT with the exit code already in EAX.
 *
 * Returns the ESP to restore (may be a different process's ESP if blocked).
 */
uint32_t process_wait(uint32_t current_esp, uint32_t *out_exit_code);

uint32_t scheduler_tick(uint32_t current_esp);
uint32_t process_current_pid(void);
uint32_t process_exit(uint32_t current_esp);
uint32_t process_exit_with_code(uint32_t current_esp, uint32_t code);

/* exec support (Module 16) */
void     process_set_cr3(uint32_t cr3);
uint32_t process_kernel_stack_top(void);

#endif /* PROCESS_H */
