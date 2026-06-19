# Module 17 — fork & exec: Design Decisions

## 1. Deep-copy fork (not copy-on-write)

**Decision:** `paging_clone_address_space()` allocates fresh physical frames for every
user page and `memcpy`s the source frame into them.  The child immediately owns its own
independent copy of every byte.

**Why:** Copy-on-write (CoW) is the correct production optimisation and is the subject
of Module 21.  A deep-copy fork is simpler to reason about, produces no hidden sharing,
and lets us prove the address space machinery is correct before adding lazy copying on
top.  The performance cost doesn't matter in a single-process demo.

**What changes in Module 21:** PTEs in both parent and child are marked read-only after
fork; the #PF handler allocates a new frame and copies the page only when either
process first writes to it.


## 2. Kernel pages are NOT copied — only the PT struct is duplicated

**Decision:** For PDE[0] (the 0-4MB kernel region), `paging_clone_address_space()`
allocates a new PT struct but copies the same PTE values (pointing to the same physical
frames).  The kernel frames are shared read-only between every process.

**Why:** Kernel pages are supervisor-only (U/S=0).  No ring-3 code can access them.
Copying them would waste PMM pages and invalidate the invariant that all processes see
the same kernel.  Each process needs its own PT struct so the supervisor-only flag can
be enforced independently, but they must all point to the same underlying frames.


## 3. PROC_ZOMBIE state: PCB alive after exit, until parent reaps

**Decision:** `process_exit()` transitions the process to `PROC_ZOMBIE` rather than
`PROC_DEAD`.  The PCB slot stays allocated and `exit_code` is preserved.  Only when
the parent calls `SYS_WAIT` and reaps does the slot become `PROC_DEAD` (PID=0, free
for reuse).

**Why:** This matches the Unix zombie model.  If exit immediately freed the PCB, a
parent that calls `wait()` slightly later would find nothing to reap.  The zombie state
bridges the gap: the child is gone from the CPU but its exit code survives for the
parent.

**Trade-off:** A rogue process that never calls `wait()` leaks PCB slots.  Real OSes
handle this by re-parenting orphans to `init` (PID 1).  That is Module 17.5 territory.


## 4. Blocking SYS_WAIT: exit code written into parent's saved EAX

**Decision:** When `process_wait()` finds no zombie child, it sets the caller to
`PROC_WAITING` and calls `scheduler_tick` to yield.  When a child later exits,
`process_exit()` finds the `PROC_WAITING` parent, writes the exit code directly into
`((struct interrupt_frame *)parent->esp)->eax`, and sets the parent `PROC_READY`.
When the parent is next scheduled, `isr_common_stub` pops that frame and the exit code
is naturally in EAX — no extra wakeup path needed.

**Why:** This avoids adding a "pending exit code" field to the PCB or a separate wakeup
queue.  The saved ESP already points to the parent's interrupt frame on its kernel stack.
Writing into `frame->eax` is the same mechanism syscall_dispatch uses to return values
— it works because `popa` inside `isr_common_stub` reads from that exact memory location.


## 5. Address space freed on exit, not on reap

**Decision:** `process_exit()` calls `paging_free_address_space(old_cr3)` immediately
after switching to the next process's CR3.  The physical pages are returned to the PMM
while the process is still a zombie.

**Why:** The parent's `wait()` call only needs the exit code — it doesn't need the
child's address space.  Freeing early returns pages to the PMM as soon as possible,
reducing peak memory pressure.  The freed CR3 value is no longer in any register or
page table, so it is safe to reclaim.

**Invariant:** `paging_free_address_space` must never be called while the CR3 it refers
to is loaded in the CR3 register.  `process_exit()` guarantees this by switching CR3
first (via `paging_switch(next->cr3)`) and only then calling `paging_free_address_space`.


## 6. Fork frame copy includes user_esp/user_ss for ring-3 callers

**Decision:** `process_fork()` computes:
```c
frame_bytes = sizeof(struct interrupt_frame);
if (frame->cs & 3u) frame_bytes += 8u;
```
and copies that many bytes to the top of the child's kernel stack.

**Why:** When a ring-3 process issues `int $0x80`, the CPU pushes user_esp and user_ss
ABOVE the normal iret frame on the kernel stack (because the privilege level changes
from 3 to 0).  These two words are NOT inside `struct interrupt_frame` — they sit
at `frame + sizeof(*frame)`.  `isr_common_stub` relies on them being present in the
correct position when it executes `iret`.  Without copying them, the child's iret would
load garbage into ESP and SS and crash immediately.


## 7. SYS_EXIT now stores exit code before zombifying

**Decision:** `syscall_dispatch` calls `process_exit_with_code(esp, code)` (new
function) rather than `process_exit(esp)`.  `process_exit_with_code` stores `code`
in `current->exit_code` before calling the main exit path.

**Why:** `process_exit` needs to set the zombie's exit code before waking a PROC_WAITING
parent (which reads it).  Storing it inside `process_exit` via a separate helper keeps
the public `process_exit(esp)` signature unchanged — the user_fault_hook in isr.c still
calls `process_exit` directly with exit code 0, which is correct for a killed process.
