# Module 24 — Threads: Design Decisions

## 1. Threads as lightweight PCB entries sharing CR3 — no separate thread table

**Decision:** A thread is just a `struct process` in `proc_table[]` with `is_thread=1` and the same `cr3` as its creator.  There is no separate thread control block or thread table.

**Why:** The scheduler already iterates over `proc_table` to find runnable entries.  Adding threads as full PCB entries means the scheduler picks them up for free with zero code changes.  A separate thread table would require a unified runqueue or a two-level iteration loop.  The simplest correct approach is the one we took.

**Trade-off:** Threads consume a full PCB slot (MAX_PROCESSES=8 total).  A system with 6 processes can only have 2 more thread slots.  A production kernel separates the process and thread concepts with distinct limits.

## 2. Thread exit: PROC_DEAD immediately — no zombify, no parent wait

**Decision:** When a thread calls `SYS_THREAD_EXIT`, `process_exit` checks `is_thread` and sets `state = PROC_DEAD; pid = 0; proc_count--` without calling `zombify`.

**Why:** POSIX pthreads (non-detached) require a `pthread_join` to reap a thread, but in our minimal model threads are implicitly detached.  The main thread does not call `SYS_WAIT` for its worker threads.  If we zombified threads, the zombie slot would never be reaped (the parent is using `SYS_WAIT` for fork'd children, not threads), leaking a PCB entry forever.

**Trade-off:** There is no way for the main thread to know when a worker thread has finished (no join).  The demo uses `SYS_YIELD` loops instead.  A `SYS_THREAD_JOIN` syscall would be the next step.

## 3. Address space not freed on thread exit — only the group leader's exit frees CR3

**Decision:** `process_exit` skips the `paging_free_address_space(old_cr3)` call when `is_thread` is set.

**Why:** All threads in the group share the same CR3.  If one thread frees the page directory, every other thread's virtual addresses instantly become invalid — a guaranteed crash.  The address space must outlive all threads in the group.  Only when the group leader (main thread) exits is it safe to free CR3.

**Trade-off:** In this module, the group leader always outlives its workers (it calls `SYS_EXIT` last).  A robust implementation would track how many threads are alive in the group and free CR3 when the last one exits.

## 4. Each thread gets its own kernel stack — TSS must be updated on every context switch

**Decision:** `process_thread_create` calls `kmalloc(PROC_STACK_SIZE)` for each thread's kernel stack.  The `kernel_stack_top` field is set, and `scheduler_tick` calls `tss_set_kernel_stack(current->kernel_stack_top)` before returning.

**Why:** When a ring-3 thread triggers a system call or interrupt, the CPU uses the kernel stack from TSS ESP0.  If two threads shared a kernel stack, a nested interrupt or preemption while one thread is inside the kernel would corrupt the other's in-progress kernel-mode execution.  Each thread must have its own kernel stack -- this is non-negotiable.

**Trade-off:** Each thread consumes PROC_STACK_SIZE (4096 bytes) of heap for its kernel stack.  With 8 total PCB slots, the maximum kernel stack overhead is 32 KB -- trivial.

## 5. Thread group ID tracks the owner of the CR3 for cleanup

**Decision:** `thread_group_id` is set to `current->thread_group_id` when creating a thread, and to the process's own PID when a new process is created (fork or process_create).

**Why:** This allows future code to answer the question "who owns this CR3?" without scanning all PCB entries.  When the group leader exits, `paging_free_address_space` should be called exactly once.  Currently the group leader always exits last (by the demo design), but the field enables a correct cleanup check in future: "if I am the last alive member of my thread group, free the CR3."

**Trade-off:** The field is set but not yet used for the last-thread cleanup check.  That logic is deferred — the demo is structured to make it unnecessary.

## 6. No mutex or spinlock — races are visible and intentional at this stage

**Decision:** The shared `counter` variable is incremented by the worker thread with no synchronization.  The main thread reads it with no barrier.  On x86, read-modify-write to an aligned int is not atomic, and the compiler may cache it in a register.

**Why:** The purpose of this module is to demonstrate that two execution contexts share one address space.  Introducing a correct mutex at this stage would obscure that fundamental point behind synchronization machinery.  The race is observable and educationally valuable: the student can see that `counter` increments are visible across the scheduler context switch because the page is literally the same physical memory.

**Trade-off:** The demo happens to work correctly because only one thread writes `counter` at a time (worker runs during yields, main thread reads after yielding enough times).  This is a controlled race, not a safe pattern.  Module 25+ should introduce atomic operations or spinlocks as a prerequisite to safe concurrent data structures.

## 7. Scheduler treats threads identically to processes — correct because they ARE processes

**Decision:** No changes were made to `scheduler_tick`.  It picks any PROC_READY PCB entry regardless of `is_thread`.

**Why:** This is the cleanest possible design.  A thread is a process with a shared address space.  The scheduler's job is to run runnable execution contexts -- it does not need to know whether two runnable entries share a CR3.  Adding thread-specific logic to the scheduler would be premature generalization.

**Trade-off:** The scheduler does not prefer to run threads from the same group together (no thread affinity, no NUMA awareness).  This is fine for a single-CPU educational OS.
