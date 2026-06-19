# Component 11: System Calls — Deep Dive

---

## 1. Where We Left Off

Modules 1–10 built a kernel with preemptive multitasking.  Every process runs
in **ring 0** (kernel mode) and can call any kernel function directly —
`vga_putchar()`, `kmalloc()`, `process_exit()` — with no restrictions.

That is fine for a teaching OS, but it is fundamentally wrong for a real OS.
User programs must not be allowed to directly call kernel functions because:
- A buggy or malicious program could corrupt kernel memory.
- The kernel cannot validate arguments if the user can call anything.
- Programs written against one kernel version break when internal APIs change.

The solution is a **system call interface**: a formal, auditable boundary
between user code and kernel code.  Module 11 builds that boundary using
x86's software interrupt mechanism (`int 0x80`).

---

## 2. The Problem: Calling Kernel Code from User Code

On x86, code in ring 3 (user mode) cannot:
- Call kernel functions directly (addresses in kernel memory are not mapped
  as user-accessible).
- Execute privileged instructions (CLI, STI, IN, OUT, MOV CR3, …).
- Access kernel memory at all (page tables mark it supervisor-only).

Ring 3 needs a controlled portal into ring 0.  The classic x86-32 mechanism
is **software interrupt** via the `int` instruction.

---

## 3. How `int 0x80` Works

```
User code              CPU                    Kernel
─────────              ───                    ──────
mov eax, 2            ──→                    
mov ebx, 1
mov ecx, buf
mov edx, len
int 0x80              ──→  Look up IDT[128]
                            Check DPL (3) ≥ CPL (3) ✓
                            Push EFLAGS, CS, EIP
                            Clear IF
                            Load CS from gate selector (ring 0)
                            Jump to gate handler
                                               isr_common_stub runs
                                               pusha, push segs
                                               call interrupt_handler
                                               → syscall_dispatch(frame)
                                               → do_write(fd, buf, len)
                                               frame->eax = len
                                               return (uint32_t)frame
                                               mov esp, eax
                                               pop segs, popa
                                               iret
                      ←──  Restore EFLAGS, CS, EIP (ring 3)
EAX = len             ←──
```

The CPU does the privilege check, saves state, and switches rings.  The kernel
does the work and puts the result in EAX via the saved frame.  `iret` restores
everything and the user code sees the result in EAX.

---

## 4. The IDT Gate for Vector 0x80

```
type_attr byte = 0xEE:
  bit 7   = 1  (Present)
  bits 6-5 = 11  (DPL = 3 — ring 3 code CAN trigger this gate)
  bit 4   = 0  (32-bit descriptor type)
  bits 3-0 = 1110  (32-bit interrupt gate — IF cleared on entry)
```

Compare to 0x8E (exception gates):
```
  bits 6-5 = 00  (DPL = 0 — only ring 0 code can use this)
```

If the gate had DPL=0, a ring-3 `int 0x80` would trigger a #GP (general
protection fault) before reaching our handler.  DPL=3 means the CPU allows
the call from any ring.

The **handler still runs in ring 0** because the gate's segment selector
(GDT_SEL_CODE, 0x08) points to a ring-0 code segment.  DPL in the gate is
about who can call it, not about what ring it runs in.

---

## 5. Returning Values via `frame->eax`

The handler cannot write to the caller's EAX register directly — it's already
saved on the stack as part of `struct interrupt_frame`.  But that is fine:

```c
frame->eax = (uint32_t)return_value;
```

When `isr_common_stub` executes `popa`, it pops `frame->eax` into EAX.  So
the caller's EAX after `int 0x80` is exactly whatever we wrote into
`frame->eax`.  No extra mechanism needed — the interrupt frame IS the ABI.

---

## 6. Syscall Dispatch Table

We use a `switch` statement rather than a function pointer array:
- A bounds check on the syscall number is implicit in the default case.
- The compiler can optimize a `switch` to a jump table itself.
- It's visually clear which numbers are implemented.

Production kernels (Linux) use a function pointer array (the syscall table).
For teaching purposes, `switch` is more readable.

---

## 7. The `SYS_EXIT` Special Case

`SYS_EXIT` is different from all other syscalls because the calling process
must stop running immediately.  We cannot just write a value into `frame->eax`
and return — returning to the dead process would be wrong.

Instead, `do_exit()` calls `process_exit(current_esp)` which:
1. Marks the process DEAD.
2. Finds the next READY process.
3. Returns that process's saved ESP.

`syscall_dispatch` returns this new ESP directly to `interrupt_handler`, which
returns it to `isr_common_stub`, which does `mov esp, eax` — switching to the
next process's stack before the `pop`/`iret` sequence.  The dead process's
stack is no longer touched.

---

## 8. The `SYS_YIELD` Special Case

`SYS_YIELD` also changes the running process — it voluntarily surrenders the
CPU.  The implementation calls `scheduler_tick(current_esp)` directly:

```c
case SYS_YIELD:
    return do_yield((uint32_t)frame);
```

This is exactly what happens on every IRQ0, except it's triggered by the
process itself rather than the timer.  The result: a yield mid-loop feels
instant to the yielding process (it resumes from the same `int 0x80`
instruction on the next switch back), while other processes get to run.

---

## 9. Why the Inline `syscall()` Helper Belongs in the Header

```c
static inline int syscall(int num, int a, int b, int c) { ... }
```

In a real OS, this would be in `libc` — the user-space C library that every
program links against.  Since we have no libc yet, the header is the simplest
place.  `static inline` means:
- No separate object file needed.
- Each translation unit gets its own copy (compiled away entirely if unused).
- No name collision — it's local to each compilation unit.

---

## 10. Argument Passing Convention (Linux i386-compatible)

| Register | Role          |
|----------|---------------|
| EAX      | Syscall number |
| EBX      | Argument 1   |
| ECX      | Argument 2   |
| EDX      | Argument 3   |
| EAX      | Return value (after call) |

We deliberately match the Linux i386 ABI so the concepts transfer directly.
Linux adds ESI (arg4), EDI (arg5), EBP (arg6) for syscalls with more arguments.

---

## 11. File Structure

| File          | Purpose |
|---------------|---------|
| `syscall.h`   | Syscall numbers, `syscall()` inline helper, `sys_*` convenience wrappers, API declarations |
| `syscall.c`   | `syscall_dispatch()` router + `do_write`, `do_exit`, `do_getpid`, `do_yield` implementations |
| `isr.h`       | Added `isr_set_syscall_handler()` |
| `isr.c`       | Added `syscall_hook`; dispatches vec==0x80 to it |
| `idt.h`       | Added `IDT_GATE_SYSCALL = 0xEE` |
| `idt.c`       | Added `idt_set_gate(0x80, isr128, …, IDT_GATE_SYSCALL)` |
| `isr.asm`     | Added `isr128` stub (ISR_NOERRCODE 128) |
| `process.h/c` | Added `process_exit()` for SYS_EXIT |
| `kernel.c`    | Demo: 3 workers use SYS_GETPID, SYS_WRITE, SYS_YIELD, SYS_EXIT |

---

## 12. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| Mechanism | `int 0x80` software interrupt | Classic x86-32 approach; same as Linux; reuses existing IDT/ISR infrastructure |
| Gate type | IDT_GATE_SYSCALL = 0xEE (DPL=3) | DPL=3 required so ring-3 code won't #GP; gate DPL controls who can CALL, not where it runs |
| Syscall number register | EAX | Matches Linux i386 ABI; familiarity transfers directly |
| Argument registers | EBX, ECX, EDX | Same Linux i386 convention; all already saved in interrupt_frame |
| Return value | Written into frame->eax | The only way to pass a value back through the interrupt frame; popa will restore it into EAX |
| Dispatch | switch statement | Readable; implicit bounds check via default case; easily extended |
| SYS_EXIT | Returns next process's ESP directly | The dead process can never be returned to; must switch before the iret |
| SYS_YIELD | Calls scheduler_tick() | Reuses the same mechanism as the timer; zero additional code |
| Syscall hook in isr.c | Function pointer (same pattern as scheduler) | Avoids isr.c importing syscall.h; low-level modules don't depend on high-level ones |
| No syscall table array | switch vs. array | array would need bounds checking anyway; switch is cleaner for 4 entries |

---

## 13. What We Proved By Getting This Working

- The `int 0x80` gate fires correctly from any privilege level.
- The CPU saves all registers, switches to kernel mode, calls our handler,
  and returns — the full round-trip works.
- Writing to `frame->eax` successfully passes a return value back to the caller.
- `SYS_WRITE` correctly routes process output through the VGA driver.
- `SYS_GETPID` returns correct, distinct PIDs to different processes.
- `SYS_YIELD` correctly triggers a context switch mid-syscall.
- `SYS_EXIT` terminates the process and switches to the next without ever
  returning to the dead process's code.
- The three worker processes print interleaved output, proving they run
  concurrently AND communicate with the kernel only through `int 0x80`.

---

## 14. What Comes Next (Component 12 Preview)

Component 12 adds a **File System** — the ability to store and retrieve named
data on a virtual disk.

Our kernel currently has no persistent storage.  Everything lives in RAM and
disappears when QEMU exits.  A file system adds:
- A **disk image** (a second QEMU block device, passed via `-hda disk.img`).
- A **disk driver** to read and write 512-byte sectors.
- A **file system format** — we'll implement a minimal custom flat FS:
  a fixed-size directory table and raw data blocks.
- API: `fs_open()`, `fs_read()`, `fs_write()`, `fs_close()`, `fs_list()`.

This will be the first time BobOS data survives between runs.
