/*
 * sched.h — Priority Preemptive Scheduler
 *
 * Four priority levels; round-robin within each level.
 * Higher-priority runqueues are always drained before lower ones.
 */

#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include "process.h"

#define SCHED_PRIO_IDLE    0   /* only runs when nothing else can */
#define SCHED_PRIO_LOW     1
#define SCHED_PRIO_NORMAL  2   /* default for new processes */
#define SCHED_PRIO_HIGH    3   /* real-time / kernel workers */

#define SCHED_NUM_PRIOS    4
#define SCHED_TIMESLICE_MS 10  /* timer fires every 10 ms (IRQ0) */

void     sched_init(void);
void     sched_add(struct process *p, int priority);
void     sched_remove(struct process *p);
void     sched_set_priority(uint32_t pid, int priority);
int      sched_get_priority(uint32_t pid);

/* Called from IRQ0 handler — returns new ESP after context switch */
uint32_t sched_tick(uint32_t current_esp);

/* Called from syscall yield — voluntarily gives up CPU */
uint32_t sched_yield(uint32_t current_esp);

#endif /* SCHED_H */
