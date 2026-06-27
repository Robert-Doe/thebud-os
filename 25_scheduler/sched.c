/*
 * sched.c — Priority Preemptive Scheduler
 *
 * One circular runqueue per priority level.  sched_tick() picks the
 * highest non-empty queue, advances its round-robin pointer, and
 * performs a context switch if the chosen process differs from current.
 *
 * Prerequisite concepts: Module 24 (Threads), Module 05 (IDT/Interrupts),
 * Module 10 (Processes).
 */

#include "sched.h"
#include "process.h"
#include "spinlock.h"
#include "vga.h"
#include "tss.h"

extern struct process  proc_table[];
extern struct process *current;

/* Per-priority circular runqueue — index of the next process to run */
static int rq_head[SCHED_NUM_PRIOS];  /* head pointer into proc_table */
static spinlock_t sched_lock;

void sched_init(void) {
    spinlock_init(&sched_lock);
    for (int i = 0; i < SCHED_NUM_PRIOS; i++)
        rq_head[i] = 0;

    /* Give the first (kernel) process NORMAL priority */
    if (proc_table[0].state == PROC_RUNNING || proc_table[0].state == PROC_READY)
        proc_table[0].priority = SCHED_PRIO_NORMAL;
}

void sched_add(struct process *p, int priority) {
    if (priority < 0 || priority >= SCHED_NUM_PRIOS)
        priority = SCHED_PRIO_NORMAL;
    p->priority = priority;
}

void sched_remove(struct process *p) {
    (void)p; /* PCB slot reuse handles cleanup */
}

void sched_set_priority(uint32_t pid, int priority) {
    if (priority < 0 || priority >= SCHED_NUM_PRIOS) return;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid == pid &&
            proc_table[i].state != PROC_DEAD) {
            proc_table[i].priority = priority;
            return;
        }
    }
}

int sched_get_priority(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid == pid &&
            proc_table[i].state != PROC_DEAD)
            return proc_table[i].priority;
    }
    return -1;
}

/* Pick the next runnable process, honouring priority levels */
static struct process *pick_next(void) {
    /* Walk from HIGH down to IDLE */
    for (int prio = SCHED_NUM_PRIOS - 1; prio >= 0; prio--) {
        /* Scan proc_table starting from rq_head[prio], wrap around */
        for (int n = 0; n < MAX_PROCESSES; n++) {
            int idx = (rq_head[prio] + n) % MAX_PROCESSES;
            struct process *p = &proc_table[idx];
            if (p->state == PROC_READY && p->priority == prio) {
                /* Advance head so next call starts after this one */
                rq_head[prio] = (idx + 1) % MAX_PROCESSES;
                return p;
            }
        }
    }
    return current; /* nothing else runnable — stay put */
}

uint32_t sched_tick(uint32_t current_esp) {
    if (!current) return current_esp;

    current->esp = current_esp;
    if (current->state == PROC_RUNNING)
        current->state = PROC_READY;

    struct process *next = pick_next();
    if (next == current) {
        current->state = PROC_RUNNING;
        return current_esp;
    }

    current = next;
    current->state = PROC_RUNNING;

    tss_set_kernel_stack(current->kernel_stack_top);

    if (next->cr3 != 0) {
        uint32_t cr3 = next->cr3;
        asm volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
    }

    return current->esp;
}

uint32_t sched_yield(uint32_t current_esp) {
    return sched_tick(current_esp);
}
