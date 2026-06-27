# Module 25 — Scheduler: Design Decisions

## 1. Four fixed priority levels, not a dynamic nice-value system

**Decision:** SCHED_PRIO_IDLE (0), LOW (1), NORMAL (2), HIGH (3) — four compile-time constants. No `nice` value, no priority inheritance, no aging.

**Why:** Linux uses a 140-level priority space plus aging to avoid starvation; that requires O(1) or CFS-style complexity. For a single-CPU educational kernel the critical insight is the *separation of priority classes*, not the exact granularity. Four levels demonstrate preemption, starvation risks, and the IDLE trampoline clearly without drowning in policy code.

**Trade-off:** A NORMAL-priority process that spawns many threads can monopolize CPU against a LOW-priority background task. A production scheduler adds aging (raise priority of starving tasks) or bandwidth limits. Both are left as exercises.

## 2. Strict preemption: higher queue is always checked first

**Decision:** `pick_next()` walks from SCHED_PRIO_HIGH down to SCHED_PRIO_IDLE and returns the first runnable process in the highest non-empty queue.

**Why:** This models real-time scheduling (POSIX SCHED_FIFO). A HIGH-priority process runs until it blocks or yields — NORMAL processes don't get a turn until all HIGH entries are PROC_WAITING. This makes priority semantics unambiguous and easy to reason about.

**Trade-off:** A misbehaving HIGH-priority process that never yields starves all NORMAL and LOW processes permanently. The fix is a watchdog or CPU-time cap per priority class, not implemented here.

## 3. Spinlock protects the runqueue head pointers

**Decision:** `sched_tick` acquires `sched_lock` before touching `rq_head[]`.

**Why:** On a single CPU the real race is between the timer IRQ and a syscall path that also calls `sched_yield`. Without the lock a timer IRQ arriving mid-yield corrupts the head pointers. The spinlock makes the critical section atomic with respect to nested interrupts.

**Trade-off:** Spinlocks busy-wait. In practice on a single CPU the lock is never contended — but the discipline matters for correctness and future SMP readiness.

## 4. IDLE priority exists — the kernel idle loop runs at PRIO_IDLE

**Decision:** `kernel_idle()` is a `while(1) hlt` loop registered at SCHED_PRIO_IDLE.

**Why:** Without an IDLE process, `pick_next()` returns NULL when every other process is PROC_WAITING. NULL causes a crash. The IDLE process guarantees `pick_next()` always returns something — the same guarantee Linux per-CPU idle threads provide.

**Trade-off:** The IDLE process consumes a PCB slot (one of MAX_PROCESSES=8). A real kernel has a dedicated idle structure, not a full PCB.

## 5. SYS_SETPRIORITY / SYS_GETPRIORITY let processes control their own scheduling class

**Decision:** Syscalls 19 and 20 let a process change its priority at runtime.

**Why:** This mirrors POSIX `setpriority(PRIO_PROCESS, 0, n)` and lets the shell demo spawn a background task at LOW and a foreground task at HIGH to observe preemption without kernel source changes.

**Trade-off:** No privilege check — any process can raise itself to HIGH. A real kernel checks CAP_SYS_NICE before allowing a priority increase.

## 6. Round-robin within each priority level

**Decision:** `rq_head[prio]` advances (mod MAX_PROCESSES) each time a process at that level is selected.

**Why:** Without round-robin, two HIGH-priority processes would always favour the one with the lower PCB index. The head pointer gives each process at the same level equal turns.

**Trade-off:** A process that exits leaves a gap in the PCB array. The full `n < MAX_PROCESSES` scan covers this — no runnable process is ever missed.
