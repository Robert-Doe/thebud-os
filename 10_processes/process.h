/*
 * process.h — Process Control Block and Scheduler API
 *
 * A "process" here is a kernel thread: it runs in ring 0 (kernel mode),
 * shares the same page directory as every other process, and has its own
 * private stack.  True user-mode processes come in Module 11 (system calls).
 *
 * The scheduler is round-robin: on every timer tick (IRQ0) the running
 * process is preempted and the next READY process in a circular list takes
 * over.  The switch is invisible to each process — it just appears to run
 * continuously, only slower.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

/* Maximum number of simultaneously live processes (including the idle task). */
#define MAX_PROCESSES   8

/* Stack size allocated from the heap for each process. */
#define PROC_STACK_SIZE 4096u   /* 4 KB */

/* Process states. */
typedef enum {
    PROC_READY   = 0,   /* runnable, waiting for CPU time   */
    PROC_RUNNING = 1,   /* currently executing               */
    PROC_DEAD    = 2    /* exited; slot may be reclaimed     */
} proc_state_t;

/* Process Control Block (PCB) — one per process. */
struct process {
    uint32_t        pid;        /* unique process ID (1-based)       */
    uint32_t        esp;        /* saved ESP (points into the stack) */
    uint8_t        *stack;      /* base of the allocated stack       */
    proc_state_t    state;
    struct process *next;       /* next process in the circular list */
};

/*
 * process_init() — must be called once before process_create().
 * Creates the "idle" process that represents the current kernel main() context
 * (no stack allocation needed — it already has a stack).
 * Registers the scheduler tick with the ISR layer.
 */
void process_init(void);

/*
 * process_create(entry) — spawn a new kernel process.
 * Allocates a stack, sets up a fake interrupt frame so the first context
 * switch jumps to entry(), and adds the process to the ready queue.
 * Returns the new process's PID, or 0 on failure.
 */
uint32_t process_create(void (*entry)(void));

/*
 * scheduler_tick(current_esp) — called by the ISR layer on every IRQ0.
 * Saves current_esp into the current process's PCB, advances to the next
 * READY process, marks it RUNNING, and returns its saved ESP.
 * The ISR stub uses the returned ESP to restore that process's registers.
 */
uint32_t scheduler_tick(uint32_t current_esp);

/* Returns the PID of the currently running process (useful for demos). */
uint32_t process_current_pid(void);

#endif /* PROCESS_H */
