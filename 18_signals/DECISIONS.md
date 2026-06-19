# Module 18 — Signals: Design Decisions

## 1. Asynchronous delivery via scheduler_tick, not a separate "check" path

**Decision:** `scheduler_tick()` calls `deliver_user_signals()` immediately after
`paging_switch()` and before returning the new process's ESP.  There is no
separate "return-to-user" hook, no re-entrant signal check, no dedicated thread.

**Why:** Every path that hands the CPU back to a ring-3 process goes through
`scheduler_tick`.  Placing the check there gives one canonical delivery point.
The user address space is already active after `paging_switch`, so writing to
the user stack is safe without any extra CR3 switch.

**Trade-off vs. production kernels:** Signals are only checked at timer ticks
(~10ms).  A process in a long kernel-mode operation won't see pending signals
until it reschedules.  Linux sets a `TIF_SIGPENDING` thread flag that is checked
on every kernel-to-user transition — any syscall return, exception return, or
context switch.  That requires instrumenting every kernel exit point.  For our
round-robin OS, the single check in `scheduler_tick` is sufficient.


## 2. Signal call frame: cdecl layout, no sigreturn trampoline

**Decision:** Signal delivery pushes exactly two words onto the user stack:
```
[new_user_esp + 0] = original EIP   ← `ret` in the handler pops this
[new_user_esp + 4] = signo           ← handler's (int sig) argument
```
The saved `eip` in the interrupt frame is replaced with the handler's address.
When the handler executes `ret`, execution resumes at the original EIP.

**Why:** The x86 cdecl calling convention expects `[esp] = return_address` and
`[esp+4] = first_arg` at function entry.  Two pushes reconstruct exactly that
layout without any assembly trampoline.

**What we omit vs. POSIX:**
- No `sigcontext` / `ucontext` — EAX and FPU state are not saved.
- No `sigreturn` syscall — the trampoline for full context restoration.
- SIGSEGV handlers must call `sys_exit()` rather than returning, because
  returning from a SIGSEGV handler would re-execute the faulting instruction.
  This is documented in the demo and in the DECISIONS.md for users.


## 3. SIGKILL cannot be caught; non-current victims are killed immediately

**Decision:** `process_send_signal()` special-cases `SIGKILL`:
- Target is not the current process: call `zombify()` + free its address space
  right now.  Safe because we are in the *sender's* CR3, which is a different
  physical page directory from the victim's.
- Target is the current process: set the pending bit; `scheduler_tick` handles
  it at the top of its selection loop before switching to that process.

`process_set_handler()` rejects `SIGKILL` — no user handler can override it.

**Why the eager kill for non-current processes:** Immediate zombie creation means
a waiting parent is woken up right away and can reap.  Lazy delivery (pending bit
only) would require the victim to be scheduled before being killed, adding
unnecessary latency.


## 4. SIGSEGV can be caught: process_fault_handler replaces process_exit as the ring-3 fault hook

**Decision:** `process_init()` registers `process_fault_handler` instead of
`process_exit` with `isr_set_user_fault_handler()`.

On a ring-3 CPU exception:
1. Check `current->signal_handlers[SIGSEGV]`.
2. If a non-default handler is set: build the signal call frame in-place on the
   user stack and return `esp` unchanged — the process resumes at the handler.
3. Otherwise: set `exit_code = 11` and call `process_exit`.

**Why:** The same two-word frame mechanism used for async signals works here too.
The only difference is we build it inline in the exception path rather than
deferring to the next tick.  Zero new infrastructure needed.


## 5. Signal handlers are inherited across fork; pending signals are not

**Decision:** `process_fork()` copies `current->signal_handlers[]` into the
child PCB.  `pending_signals` is zeroed in the child.

**Why:** POSIX: handler dispositions are inherited; pending signals are not.
A signal pending on the parent before `fork` is the parent's business.  The
child starts with a clean slate but keeps the same handler registrations, so it
can catch the same signals the parent would.


## 6. PCB signal state: bitmask + flat array, no queue

**Decision:** `pending_signals` is a `uint32_t` bitmask.
`signal_handlers[NSIG]` is a flat `uint32_t` array.

**Why:** POSIX standard signals do not queue — if SIGUSR1 is sent 10 times
before delivery, only one delivery occurs.  A bitmask models this exactly.
With NSIG=32, the entire mask fits in one CPU register.  Finding the lowest-set
bit is a simple loop or a single `bsf` instruction.

**What real-time signals would need:** POSIX SIGRTMIN–SIGRTMAX must queue.
That requires a per-signal linked list of queued values.  Deferred to future
work; standard signals cover all the cases this module needs to prove.


## 7. SYS_KILL validates signo at both the syscall and process layers

**Decision:** `do_kill()` returns `SYSCALL_EINVAL` for out-of-range `signo`.
`process_send_signal()` also validates before touching the PCB.

**Why:** The syscall layer is the user-space trust boundary.  The process layer
is called directly from kernel code in some paths (e.g., `process_fault_handler`
could call `process_send_signal`).  Validating at both layers means neither path
can produce a stray bit-set outside the `[1, NSIG)` range.  The cost is two
comparisons; the benefit is a kernel panic cannot be triggered by a bad signo.
