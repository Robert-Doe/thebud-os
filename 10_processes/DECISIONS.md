# Component 10: Processes & Scheduler — Deep Dive

---

## 1. Where We Left Off

Components 1–9 built a kernel that initialises hardware and responds to
keyboard input, but it can only do **one thing at a time**.  Every function
call must return before the kernel can do anything else.  Component 10
changes that fundamentally: BobOS can now run multiple tasks **simultaneously**
through **preemptive multitasking**.

The three key ideas in this module:

1. **Process** — an independent flow of execution with its own private stack.
2. **Context switch** — saving one process's CPU state and restoring another's.
3. **Scheduler** — deciding which process runs next and triggering the switch.

---

## 2. What Is a Context Switch?

The CPU has one set of registers (EAX, EBX, ESP, EIP, EFLAGS …).  At any
moment only one process is using them.  A context switch is the act of:

1. **Saving** the current process's register values somewhere in memory.
2. **Loading** a different process's previously saved register values.
3. **Resuming** the new process from exactly where it left off.

From each process's perspective, nothing happened — it just experienced a
pause that it cannot observe.

---

## 3. How Our Context Switch Works (the trick)

We hijack the interrupt return path that **already exists** from Module 5.

When IRQ0 fires, the CPU + `isr_common_stub` push a complete register
snapshot onto the **current process's stack**:

```
Stack (low → high):
  gs, fs, es, ds          ← pushed by stub
  edi…eax                 ← pushed by pusha
  int_no, err_code        ← pushed by stub
  eip, cs, eflags         ← pushed by CPU automatically
```

This is a `struct interrupt_frame`.  The ESP that points to it IS the saved
state of the current process.

Previously, `isr_common_stub` called `interrupt_handler(frame)` and then
discarded the return value:

```asm
push esp
call interrupt_handler
add  esp, 4          ; ← throw away EAX
pop gs … popa … iret
```

**Module 10 change** — use EAX (the return value) as the new ESP:

```asm
push esp
call interrupt_handler   ; EAX = new ESP (may be a different process's stack)
mov  esp, eax            ; ← ONE instruction — the entire context switch
pop gs … popa … iret     ; restores NEXT process's registers
```

`interrupt_handler` now returns `uint32_t`.  Normally it returns
`(uint32_t)frame` (unchanged).  When switching, it returns the next
process's saved ESP instead.  The stub then restores registers from that
different memory location and `iret` jumps to the other process's EIP.

That's it.  One `MOV ESP`.

---

## 4. The Process Control Block (PCB)

```c
struct process {
    uint32_t        pid;    /* unique ID                          */
    uint32_t        esp;    /* saved ESP (points into its stack)  */
    uint8_t        *stack;  /* base of the allocated stack        */
    proc_state_t    state;  /* READY / RUNNING / DEAD             */
    struct process *next;   /* next in circular linked list       */
};
```

The most important field is `esp`.  When a process is NOT running, `esp`
holds the address of its saved `interrupt_frame` on its private stack.
When the scheduler picks a process, it loads that `esp` into the CPU — the
frame is popped and the process resumes.

---

## 5. The Fake Initial Stack Frame

A new process has never been interrupted, so it has no saved frame.  We
build one manually on its freshly allocated stack so the first switch into
it behaves identically to every subsequent switch:

```
High addr ────────────────────────────────────────
  eflags  = 0x202  (bit 1 always set; IF=1 enables interrupts after iret)
  cs      = 0x08   (kernel code selector)
  eip     = entry  (the function to run)
  err_code= 0
  int_no  = 0
  eax…edi = 0      (all registers initialised to zero)
  ds/es/fs/gs = 0x10  (kernel data selector)
Low addr  ← saved ESP points here ───────────────
```

Why `eflags = 0x202`?  Bit 9 is IF (Interrupt Flag).  If we left it 0,
`iret` would restore it as 0 and interrupts would be disabled for this
process — the timer would never fire again and scheduling would stop.
Setting IF=1 ensures the timer keeps working after the first switch.

---

## 6. The Round-Robin Scheduler

```c
uint32_t scheduler_tick(uint32_t current_esp) {
    current->esp   = current_esp;   // save
    current->state = PROC_READY;
    next = current->next;           // advance (circular)
    while (next->state != PROC_READY)
        next = next->next;
    current = next;
    current->state = PROC_RUNNING;
    return current->esp;            // hand new ESP to isr_common_stub
}
```

Called on every IRQ0 (≈18 times per second).  Saves the current ESP,
walks the circular list to find the next READY process, marks it RUNNING,
and returns its saved ESP.  If only one process exists the list wraps back
to the same process — no switch occurs.

---

## 7. The Idle Process

`process_init()` turns **the already-running `kernel_main` context** into
PID 1 (the idle process).  It needs no stack allocation because it already
has a stack (the boot stack at `0x90000` from `kernel_entry.asm`).

Its `esp` field starts at 0 (a sentinel meaning "not yet saved").  The
first IRQ0 writes the real ESP into it before switching away.

After spawning the demo processes, `kernel_main` enters:

```c
for (;;) { __asm__ volatile ("hlt"); }
```

`hlt` sleeps until the next interrupt.  The timer fires, `scheduler_tick`
preempts the idle process, and one of the demo processes runs for a tick.
The idle process is included in the round-robin so it gets CPU time too —
it just spends it sleeping.

---

## 8. Stack Allocation

Each process gets `PROC_STACK_SIZE` (4 KB) from `kmalloc`.  The heap was
initialised in Module 8 and its pages are within the identity-mapped 4 MB,
so no additional page table entries are needed.

The stack pointer begins at the **top** of the allocation
(`stack + PROC_STACK_SIZE`) because x86 stacks grow downward.  The initial
frame is built by pre-decrementing a `uint32_t *` pointer 17 times (one
per `struct interrupt_frame` field).

---

## 9. The Scheduler Hook — Avoiding Circular Headers

`interrupt_handler` in `isr.c` needs to call `scheduler_tick` from
`process.c`.  If `isr.c` included `process.h` directly we'd have a
dependency from a low-level module (isr) up to a high-level one (process),
which makes future reorganisation hard.

Instead, `isr.c` holds a **function pointer**:

```c
static uint32_t (*scheduler_hook)(uint32_t esp) = 0;
void isr_set_scheduler(uint32_t (*fn)(uint32_t)) { scheduler_hook = fn; }
```

`process_init()` calls `isr_set_scheduler(scheduler_tick)`.  The ISR layer
knows nothing about processes — it just calls whatever function is
registered.  This is the **observer / callback** pattern.

---

## 10. Why Interrupts Must Stay Enabled

The scheduler runs inside the IRQ0 handler.  While inside any interrupt
handler the CPU automatically clears IF (disables further interrupts).
This means:

- A process's private stack can never be corrupted by a nested timer IRQ
  happening mid-switch.
- The switch itself (save → advance list → restore) is atomic from the
  hardware's point of view.

Interrupts re-enable automatically when `iret` restores EFLAGS, which has
IF=1 for every process (set in the initial frame and preserved across
switches because EFLAGS is part of the saved frame).

---

## 11. File Structure

| File          | Purpose |
|---------------|---------|
| `process.h`   | PCB struct, API: `process_init`, `process_create`, `scheduler_tick`, `process_current_pid` |
| `process.c`   | PCB table, fake frame builder, round-robin scheduler |
| `isr.h`       | Updated: `interrupt_handler` returns `uint32_t`; adds `isr_set_scheduler` |
| `isr.c`       | Updated: scheduler hook; `interrupt_handler` returns new ESP on IRQ0 |
| `isr.asm`     | Updated: `mov esp, eax` after `call interrupt_handler` (replaces `add esp, 4`) |
| `kernel.c`    | Demo: spawns 3 processes; each updates a VGA row with a live counter |
| All prior files | Carried forward from Module 9 unchanged |

---

## 12. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| Switch mechanism | Hijack isr_common_stub return-ESP via EAX | No separate switch function needed; the stub already saves/restores everything |
| Scheduler trigger | IRQ0 (PIT timer, ~18 Hz) | Pre-existing hardware timer; no new hardware to configure |
| Scheduling policy | Round-robin, switch on every tick | Simplest correct policy; fair; easy to reason about |
| PCB storage | Static array (`proc_table[MAX_PROCESSES]`) | No dynamic allocation risk; fixed upper bound is fine for a teaching OS |
| Process list | Circular singly-linked list | O(1) next-process lookup; insertion at head is O(1) |
| Stack allocation | `kmalloc(PROC_STACK_SIZE)` from heap | Re-uses Module 8; no new allocator needed |
| Idle process | kernel_main context, no stack allocation | Already has a stack; simplest way to include the scheduler loop in the round-robin |
| Scheduler hook | Function pointer in `isr.c` | Avoids isr.c depending on process.h; low-level modules should not import high-level ones |
| EFLAGS in initial frame | 0x202 (IF=1) | Without IF=1, iret disables interrupts in the new process and the timer stops |
| Process stacks size | 4 KB | More than enough for a tight spin loop; can increase later for deeper call stacks |

---

## 13. What We Proved By Getting This Working

- The CPU can be preempted mid-instruction-stream by a hardware timer and
  resumed transparently — the process never knows.
- Saving and restoring 68 bytes (one `interrupt_frame`) on a private stack
  is sufficient to switch between completely independent execution contexts.
- Round-robin scheduling produces fair CPU sharing with zero cooperation
  from the running code.
- The scheduler hook pattern decouples low-level interrupt handling from
  high-level process management cleanly.
- Three spinning processes all advance their counters at similar rates,
  proving time-sliced concurrency works correctly.

---

## 14. What Comes Next (Component 11 Preview)

Component 11 adds **System Calls** — a safe mechanism for code running at
lower privilege to request services from the kernel.

Right now all code runs in ring 0 (kernel mode).  A real OS separates user
code (ring 3) from kernel code (ring 0).  User code cannot call kernel
functions directly — it must use a controlled gate.  The traditional x86
mechanism is **`int 0x80`** (software interrupt):

1. User code loads a syscall number into EAX and arguments into EBX/ECX/EDX.
2. User code executes `int 0x80`.
3. The CPU switches to ring 0 and jumps to our IDT gate for vector 0x80.
4. The kernel dispatches based on EAX, performs the service, puts the result
   in EAX, and returns with `iret` (which drops back to ring 3).

This gives us `write()`, `read()`, `exit()` — the primitives that make
user-space programs possible.
