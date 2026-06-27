/*
 * spinlock.h — x86 test-and-set spinlock
 *
 * A spinlock is the simplest synchronization primitive: spin (busy-wait)
 * until the lock is free, then atomically grab it.  Correct for SMP but
 * we use it here for interrupt-safety on a single CPU — any code that
 * touches shared scheduler state must hold this lock.
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

typedef struct { volatile uint32_t locked; } spinlock_t;

static inline void spinlock_init(spinlock_t *l) { l->locked = 0; }

static inline void spinlock_acquire(spinlock_t *l) {
    /* xchg is always LOCK-prefix'd on x86: atomic swap */
    while (__sync_lock_test_and_set(&l->locked, 1))
        asm volatile("pause");
}

static inline void spinlock_release(spinlock_t *l) {
    __sync_lock_release(&l->locked);
}

#endif /* SPINLOCK_H */
