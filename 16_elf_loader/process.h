/*
 * process.h — Process Control Block and Scheduler API
 *
 * Module 16 additions:
 *   - process_set_cr3(cr3): replace the current process's page directory
 *     (used by SYS_EXEC after loading a new ELF into a fresh PD).
 *   - process_kernel_stack_top(): return the top of the current process's
 *     kernel stack so SYS_EXEC can build a new fake frame there.
 *
 * Module 15 additions:
 *   - PCB gains `cr3`: the physical address of the process's page directory.
 *     The scheduler writes this to CR3 on every context switch so each process
 *     sees its own virtual address space.
 *   - Kernel process (PID 1) uses the kernel's own page directory.
 *   - Ring-3 processes get a fresh PD from paging_new_address_space(), with
 *     only their code and stack pages marked user-accessible.
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
    uint32_t        cr3;               /* page directory physical address         */
    proc_state_t    state;
    struct process *next;
};

void     process_init(void);
uint32_t process_create(void (*entry)(void));


uint32_t scheduler_tick(uint32_t current_esp);
uint32_t process_current_pid(void);
uint32_t process_exit(uint32_t current_esp);

/* Module 16: exec support */
void     process_set_cr3(uint32_t cr3);
uint32_t process_kernel_stack_top(void);

#endif /* PROCESS_H */
