# Module 14 — User Mode (Ring 3): Design Decisions

## What this module adds

The CPU's hardware privilege-level enforcement.  Code running at ring 3 cannot
execute privileged instructions (`cli`, `hlt`, `lgdt`, `ltr`, `inb`, `outb`,
etc.) or access I/O ports.  The only way ring-3 code can request kernel
services is through the syscall gate (`int 0x80`), which was built in
Module 11 with DPL=3.  Module 14 makes that gate the ONLY bridge between
user space and the kernel.

---

## Decision 1: TSS — one per CPU, not one per process

The x86 CPU needs the Task State Segment (TSS) to answer one question:
"when an interrupt arrives while I am in ring 3, which kernel stack should
I switch to?"  The answer is `TSS.esp0` and `TSS.ss0`.

A common misconception: you need one TSS per process.  You do not.  You need
one TSS per CPU.  Before the scheduler runs a ring-3 process, it calls
`tss_set_kernel_stack()` to update `TSS.esp0` to that process's kernel stack
top.  This is an O(1) write of 4 bytes — no copying or swapping of TSS
structures.  This is exactly what Linux does.

---

## Decision 2: TSS descriptor in GDT entry 5, loaded with `ltr`

The CPU finds the TSS via the Task Register (TR), which holds a GDT selector.
After `ltr 0x28` (GDT entry 5):

- The CPU reads GDT[5] to find the TSS base address and limit.
- On every ring-3 to ring-0 transition, it reads `TSS.esp0` and `TSS.ss0`
  from that address.

The TSS descriptor is a SYSTEM descriptor (S-bit = 0 in the access byte).
Access byte for a 32-bit available TSS: `0x89 = Present | DPL=0 | Type=1001`.

`gdt_install_tss()` fills GDT[5] after the TSS struct is ready.  This keeps
`tss.c` as the owner of the TSS struct while delegating the GDT write to
`gdt.c`.

---

## Decision 3: Two GDT entries for user mode (DPL = 3)

For the CPU to execute code at ring 3, the code segment's DPL must be 3.
Data segments (DS, ES, FS, GS) must also be DPL=3 to be loadable from ring 3.

```
GDT[3]: user code  — access 0xFA = Present | DPL=3 | S=1 | Exec | RW
GDT[4]: user data  — access 0xF2 = Present | DPL=3 | S=1 | RW

GDT_SEL_USER_CODE = 0x1B = (3 << 3) | 3   (index 3, RPL=3)
GDT_SEL_USER_DATA = 0x23 = (4 << 3) | 3   (index 4, RPL=3)
```

Both cover the full 0-4GB flat address space.  A ring-3 process can read any
physical address because the page tables do not yet mark kernel pages
supervisor-only.  That is the next module's job.

---

## Decision 4: The ring-3 fake interrupt frame has two extra words

When an interrupt fires from ring 3, the CPU:
1. Reads TSS.ss0 and TSS.esp0.
2. Switches to SS=ss0, ESP=esp0 (the kernel stack).
3. Pushes the user stack: SS3, ESP3.
4. Pushes the standard frame: EFLAGS, CS, EIP.

For ring-0 interrupts, steps 1-3 are skipped and only EFLAGS/CS/EIP are
pushed.  So a ring-3 fake frame needs two extra uint32_t values at the TOP:

```
[high addr] user_ss  = GDT_SEL_USER_DATA (0x23)
            user_esp = user_stack_top
            eflags   = 0x202
            cs       = GDT_SEL_USER_CODE (0x1B)
            eip      = user_main
            err_code = 0
            int_no   = 0
            eax..edi = 0  (pusha)
[low addr]  gs/fs/es/ds = GDT_SEL_USER_DATA  <- PCB.esp points here
```

When `iret` pops CS=0x1B (RPL=3 > CPL=0), it also pops user_esp and user_ss,
atomically restoring the user-mode stack and transitioning to ring 3.

---

## Decision 5: Can ring-0 code load the user data selector (0x23)?

Yes.  The CPU's rule for loading a data segment register:

```
DPL(segment) >= max(CPL, RPL(selector))
3             >= max(0,   3)  ==>  3 >= 3  TRUE
```

So `pop ds/es/fs/gs` with value 0x23 while still at CPL=0 (in the iret
restore sequence) is legal.  After `iret` changes CPL to 3, those segment
registers are still valid (DPL=3 >= new CPL=3).

---

## Decision 6: Two stacks per ring-3 process

| Stack | 4 KB from kmalloc | Purpose |
|---|---|---|
| Kernel stack | `p->stack` | CPU switches here via TSS.esp0 on interrupt/syscall |
| User stack | freed when process exits | The process's own call stack at ring 3 |

When `int 0x80` fires from ring 3: CPU switches to kernel stack, pushes
user SS:ESP there, kernel handles the request.  `iret` back to ring 3 pops
user SS:ESP, restoring the user stack.

---

## Decision 7: No memory isolation yet (shared page directory)

All processes share the single page directory from Module 7.  A ring-3
process can read kernel memory — the privilege level blocks instructions,
not page access.  True isolation requires supervisor-only PTEs and
per-process page directories (natural Module 15 scope).

---

## Key lessons from this module

| Concept | Takeaway |
|---|---|
| TSS | One per CPU; update `esp0` per-switch; CPU reads it on ring-3 interrupt |
| `ltr` | Loads TR from a GDT system descriptor; CPU uses it to find esp0 |
| User GDT segments | Same layout as kernel segments, just DPL=3 in the access byte |
| Ring-3 fake frame | Two extra words (user_ss, user_esp) above the standard iret frame |
| `iret` to ring 3 | CS.RPL=3 triggers the automatic pop of user ESP and SS |
| Two stacks | Kernel stack for interrupt/syscall frames; user stack for ring-3 execution |
| Privilege vs memory isolation | Ring 3 blocks privileged instructions; page flags provide memory isolation |
