# Module 15 — Per-Process Address Spaces: Design Decisions

## 1. One page directory per ring-3 process

**Decision:** `process_create_user()` calls `paging_new_address_space()` to allocate a
fresh PD (and its first PT) from the PMM.  The physical address is stored in `PCB.cr3`.
The scheduler writes `PCB.cr3` to CR3 on every context switch.

**Why:** Without separate PDs, all ring-3 processes share one virtual address space.
Any bug or exploit in one process can read or corrupt another's memory.  Separate PDs
give true isolation: each process only sees the pages it has been explicitly given.

**Trade-off:** CR3 writes are cheap (< 100 ns on real hardware) but flush the entire
TLB, so a context switch between processes with different PDs costs more than switching
between two threads that share a PD.  At this stage BobOS has no threads, so the cost
is acceptable.


## 2. Kernel pages are supervisor-only (U/S = 0) in every PD

**Decision:** `paging_init()` maps 0-4MB with `PAGE_PRESENT | PAGE_WRITABLE` and
deliberately omits `PAGE_USER`.  `paging_new_address_space()` copies these PTEs into
the new PT with `& ~PAGE_USER`, preserving the supervisor-only invariant.

**Why:** If kernel pages had U/S=1, a ring-3 process could read kernel stacks, the IDT,
GDT, and all kernel data — a complete loss of isolation.  Omitting PAGE_USER means any
ring-3 attempt to read those pages immediately raises #PF.

**Consequence:** Ring-3 code cannot be placed in the kernel text segment.  The linker
script moves `user_prog.o` into a dedicated `user_code_start`...`user_code_end` region
whose pages are then selectively opened with `paging_set_user_page()`.


## 3. paging_set_user_page sets the U/S bit in BOTH the PDE and the PTE

**Decision:** `paging_set_user_page(cr3, virt)` does:
```
pd[pdi] |= PAGE_USER;   /* open the 4 MB PDE region at directory level */
pt[pti] |= PAGE_USER;   /* open the specific 4 KB page */
invlpg(virt);
```

**Why:** Intel's paging rule (Vol. 3, S4.6): for a user-mode access to succeed, BOTH
the PDE and the PTE must have U/S=1.  If the PDE has U/S=0, the entire 4 MB region is
supervisor-only regardless of individual PTE bits.  We must set both.

**Side-effect:** Opening PDE[0] at the directory level "conceptually" opens 4 MB, but
individual PTEs that still have U/S=0 remain supervisor-only — the AND of directory and
page bits applies.  So setting U/S=1 in the PDE is safe; it is necessary but not
sufficient.


## 4. User code isolated in a dedicated linker section

**Decision:** `linker.ld` uses `EXCLUDE_FILE(*user_prog.o)` to keep `user_prog.o` out
of the kernel `.text` section, then places it in a page-aligned `.user_code` section
between the exported symbols `user_code_start` and `user_code_end`.

**Why:** `process_create_user()` needs to know exactly which physical pages to grant
with PAGE_USER.  Linker symbols give a precise, build-time range: the loop

    for (addr = (uint32_t)&user_code_start;
         addr < (uint32_t)&user_code_end;
         addr += 0x1000)
        paging_set_user_page(cr3, addr);

is simple and correct.  Without the isolation, user code would be interleaved with
kernel code and we could not cleanly grant access to one without the other.


## 5. User stack pages also require PAGE_USER

**Decision:** `process_create_user()` grants PAGE_USER to every page of the user stack
(`ustack` through `ustack + PROC_STACK_SIZE`) in the same loop pattern as user code.

**Why:** The ring-3 iret frame sets `esp` to `ustack + PROC_STACK_SIZE`.  When the
CPU transitions to ring 3 and the first instruction executes (or the first function
call pushes a frame), the user stack is accessed.  If its pages are supervisor-only,
the very first stack access causes a #PF before any user code runs.


## 6. Graceful ring-3 #PF via isr_set_user_fault_handler

**Decision:** `isr.c` checks `frame->cs & 3 == 3` for all CPU exceptions (vectors 0-31).
If the faulting code was at ring 3 AND a `user_fault_hook` is registered, the handler:
1. Reads CR2 (for #PF) to get the faulting address.
2. Prints a diagnostic line (address, error code, EIP).
3. Calls `user_fault_hook(current_esp)` — which is `process_exit()`.

Ring-0 exceptions still call `kernel_panic()` and halt.

**Why:** A ring-3 page fault is a normal, expected event (e.g. stack overflow, bug,
deliberate isolation test).  Panicking the kernel for a user bug is wrong.  Using the
same hook pattern as the scheduler and syscall handler keeps the coupling minimal:
`isr.c` knows nothing about processes; `process.c` knows nothing about ISR internals.


## 7. process_exit registered as the user_fault_handler

**Decision:** `process_init()` calls `isr_set_user_fault_handler(process_exit)`.
`process_exit(current_esp)` already marks the current process DEAD, finds the next
READY process, switches CR3 to that process's PD, and returns its saved ESP.

**Why:** `process_exit` satisfies the exact contract the fault handler needs (mark
current dead, return next ready ESP).  Re-using it avoids a separate "kill process"
function.  The kernel never returns to the killed process.


## 8. CR3 switch performed in both scheduler_tick and process_exit

**Decision:** Both `scheduler_tick()` and `process_exit()` call `paging_switch(current->cr3)`
after updating the `current` pointer and before returning the new ESP.

**Why:** Both are the exit points from the "which process runs next" logic.  If either
omitted the CR3 switch, the CPU would execute the next process's code in the wrong
address space — silently or with mysterious faults.  The two call sites are symmetric:
`scheduler_tick` for preemptive switches, `process_exit` for voluntary exits or kills.
