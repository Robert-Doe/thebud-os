/*
 * process.h — Process Control Block and Scheduler API
 *
 * Module 14 changes:
 *   - PCB gains `kernel_stack_top`: the address one byte past the top of the
 *     per-process kernel stack.  The scheduler writes this into TSS.esp0
 *     before running a ring-3 process, so the CPU has the right kernel stack
 *     ready for the next interrupt.
 *   - process_create_user(entry): like process_create() but sets up a ring-3
 *     fake interrupt frame (user CS=0x1B, user SS=0x23, separate user stack).
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES   8
#define PROC_STACK_SIZE 4096u

typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_DEAD    = 2
} proc_state_t;

struct process {
    uint32_t        pid;
    uint32_t        esp;               /* saved kernel-side ESP                  */
    uint8_t        *stack;             /* kernel stack base (allocated)          */
    uint32_t        kernel_stack_top;  /* stack + PROC_STACK_SIZE (for TSS.esp0) */
    proc_state_t    state;
    struct process *next;
};

void     process_init(void);
uint32_t process_create(void (*entry)(void));

/*
 * process_create_user(entry) — spawn a ring-3 process.
 *
 * Allocates two 4KB stacks:
 *   kernel stack — used by the CPU on interrupt/syscall entry from ring 3
 *   user stack   — the process's own stack at privilege level 3
 *
 * Builds a fake interrupt frame on the kernel stack with:
 *   cs = GDT_SEL_USER_CODE (0x1B)  — triggers ring-3 iret
 *   ss = GDT_SEL_USER_DATA (0x23)
 *   ds/es/fs/gs = GDT_SEL_USER_DATA
 *   eip = entry
 *   user_esp = top of the user stack
 *
 * Returns the new PID, or 0 on failure.
 */
uint32_t process_create_user(void (*entry)(void));

uint32_t scheduler_tick(uint32_t current_esp);
uint32_t process_current_pid(void);
uint32_t process_exit(uint32_t current_esp);

#endif /* PROCESS_H */
