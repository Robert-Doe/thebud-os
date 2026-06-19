# Component 5: IDT and Interrupts — Deep Dive

---

## 1. Where We Left Off

Component 4 gave us a proper GDT in C. With segment registers correctly loaded, the
CPU enforces memory protection — but it has no way to handle anything that goes wrong.
If a program divides by zero, accesses an unmapped address, or if the timer chip
fires, the CPU looks up its IDT. Our IDT was still empty (all zeros from the
bootloader). The CPU would immediately triple-fault and reset.

Component 5 fills the IDT with 48 handlers:
- **Vectors 0–31** — the 32 CPU-defined exceptions (divide-by-zero, page fault, etc.)
- **Vectors 32–47** — the 16 hardware IRQ lines after PIC remapping

With the IDT loaded and the PIC remapped, we call `sti` to enable interrupts. The
timer chip immediately starts firing at ~18 Hz, and our C handler increments a tick
counter that displays live on screen.

---

## 2. What Is an Interrupt?

An **interrupt** is the CPU's mechanism for saying "stop what you're doing and handle
this right now." There are three sources:

| Source | Type | Examples |
|--------|------|---------|
| CPU itself | Exception | Divide-by-zero (#DE), page fault (#PF), invalid opcode (#UD) |
| Software | `int n` instruction | System calls, BIOS calls (in Real Mode) |
| Hardware | IRQ via PIC | Timer tick, keyboard keypress, disk completion |

When any of these occurs, the CPU:
1. Finishes the current instruction (or partially — some exceptions are precise).
2. Pushes EFLAGS, CS, and EIP onto the stack (saving where it was).
3. Optionally pushes an error code (exceptions only, and only some of them).
4. Looks up the IDT entry for the vector number.
5. Loads the new CS and EIP from the gate descriptor.
6. Execution resumes at the handler.

When the handler is done, `iret` (Interrupt Return) pops EIP, CS, and EFLAGS and
resumes where the code was before the interrupt.

---

## 3. The IDT Gate Descriptor — 8 Bytes

Each of the 256 IDT entries is an 8-byte "gate descriptor":

```
Bytes 7-6   Bytes 5    Byte 4     Bytes 3-2    Bytes 1-0
┌──────────┬──────────┬──────────┬────────────┬──────────┐
│offset    │type_attr │ zero     │ selector   │ offset   │
│[31:16]   │          │(always 0)│(code seg)  │ [15:0]   │
└──────────┴──────────┴──────────┴────────────┴──────────┘
```

The **handler address (offset)** is split across bytes 1–0 and bytes 7–6 — the same
fragmented Intel legacy pattern as the GDT. We hide this in `idt_set_gate()`.

The **type_attr byte** (byte 5):
```
Bit 7   Present (P)  = 1  (gate is valid)
Bits 6-5  DPL        = 00 (ring 0 — only kernel code can trigger this gate)
Bit 4   Storage (S)  = 0  (must be 0 for interrupt and trap gates)
Bits 3-0  Gate type:
  0xE = 32-bit Interrupt Gate
  0xF = 32-bit Trap Gate
```

Combined for our kernel gates: `0x8E` (P=1, DPL=00, S=0, type=0xE).

**Interrupt Gate (0x8E):** The CPU clears the IF (Interrupt Enable) flag upon entry.
This means hardware interrupts are automatically disabled while your handler runs, so
it won't be interrupted by another IRQ mid-execution. The `iret` instruction restores
EFLAGS (including IF) so interrupts re-enable after the handler returns.

**Trap Gate (0x8F):** IF is left unchanged. We don't use trap gates in Module 5 but
they're used for debugging and some exception handlers in more advanced kernels.

---

## 4. Why We Need Assembly Stubs

A C function call only saves the registers the ABI says it must: EBX, ESI, EDI, EBP.
It does not save EAX, ECX, EDX, the segment registers, or the flags register. But an
interrupt can fire between any two CPU instructions, including in the middle of a C
expression that has live values in EAX or ECX. If we let the CPU jump directly to a
C handler:

- The C function prologue would not save EAX, ECX, EDX.
- The C function epilogue would not restore them.
- The interrupted code would resume with corrupted registers and produce wrong results.

The assembly stub solves this by being the first code the CPU reaches. It runs before
any C function prologues and saves everything:

```
   CPU fires interrupt
   ↓
   [stub] push dummy error code (if needed)
   [stub] push int_no
   [stub] pusha   ← saves all 8 general-purpose registers
   [stub] push ds, es, fs, gs
   [stub] call interrupt_handler()
   [stub] pop gs, fs, es, ds
   [stub] popa
   [stub] add esp, 8  (remove int_no + err_code)
   [stub] iret         ← restores EIP, CS, EFLAGS
```

---

## 5. The Dummy Error Code

Some CPU exceptions push a hardware error code onto the stack; others do not. The
ones that do: #DF (8), #TS (10), #NP (11), #SS (12), #GP (13), #PF (14), #AC (17).

If we let some stubs push two things (int_no) and others push one thing (just int_no,
with the CPU-pushed error code already there), the `struct interrupt_frame` layout
would be different for each vector. The C handler would need to know which exceptions
have error codes to read the struct correctly — fragile and error-prone.

The fix: for exceptions WITHOUT a hardware error code, the stub pushes a dummy `0`
first to act as a placeholder error code. Now every vector produces an identical stack
layout and a single `struct interrupt_frame` works for all 48 handlers.

---

## 6. `struct interrupt_frame` — The Register Snapshot

```c
struct interrupt_frame {
    /* segment registers — pushed last by stub = lowest address */
    uint32_t gs, fs, es, ds;
    /* general-purpose — pushed by pusha */
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    /* interrupt identity — pushed by stub */
    uint32_t int_no, err_code;
    /* CPU state — pushed automatically by the CPU on interrupt entry */
    uint32_t eip, cs, eflags;
};
```

When `isr_common_stub` does `push esp` and calls `interrupt_handler(frame*)`, the
pointer `frame` points to the `gs` field at the lowest address. Reading
`frame->eip` tells you exactly where in code the interrupt fired. This is what
the panic handler uses to print the crash address.

`esp_dummy` is the ESP value captured by `pusha` — it reflects ESP before `pusha`
ran, so it's not the "real" current stack pointer but is useful for stack inspection.

---

## 7. The 8259A PIC — Why It Needs Remapping

The PIC (Programmable Interrupt Controller) translates IRQ signals from hardware into
CPU interrupt vectors. There are two 8259A chips in a PC:
- **Master PIC:** handles IRQ0-7 (timer, keyboard, COM2, COM1, LPT2, floppy, LPT1, spurious)
- **Slave PIC:** handles IRQ8-15 (RTC, ACPI, free, free, PS/2, FPU, ATA primary, ATA sec)

**The problem:** The BIOS programs the master PIC to deliver vectors 8-15 and the
slave to deliver 112-119. But CPU exception vectors 8-15 are **also** defined by
Intel (#DF, #TS, #NP, #SS, #GP, #PF, reserved, #MF). If the timer fires (IRQ0) and
the master PIC delivers vector 8, the CPU would call the Double Fault (#DF) handler
instead of the timer handler. This is wrong and dangerous.

**The fix:** Remap the PICs to deliver vectors starting at 32 (where CPU exceptions
end):

```
Before remapping:                 After remapping:
IRQ0 (timer)    → vector 8       IRQ0 (timer)    → vector 32
IRQ1 (keyboard) → vector 9       IRQ1 (keyboard) → vector 33
...                               ...
IRQ7 (spurious) → vector 15      IRQ7 (spurious) → vector 39
IRQ8 (RTC)      → vector 112     IRQ8 (RTC)      → vector 40
...                               ...
IRQ15 (ATA sec) → vector 119     IRQ15 (ATA sec) → vector 47
```

The remapping is done by writing a 4-step Initialisation Command Word sequence to the
PIC's command and data ports before enabling any interrupts.

---

## 8. EOI — End of Interrupt

After a hardware IRQ handler finishes, we must send an **End-of-Interrupt (EOI)**
command to the PIC. Without it, the PIC's "in-service register" stays latched on the
current IRQ and it will never assert the CPU INT line again — the timer would fire
once and then go silent forever.

For IRQ0-7 (master PIC), send EOI to the master only: `outb(0x20, 0x20)`.
For IRQ8-15 (slave PIC), send EOI to **both** slave and master: the slave clears its
bit, but the master also has its bit 2 (the cascade line) latched and needs clearing.

We send EOI **before** calling the user handler (at the start of the IRQ dispatch
path). This means the PIC can already accept new interrupts from other lines while
we're still in the handler for this one. It also avoids the complexity of the handler
needing to know whether to call EOI.

---

## 9. `inb` / `outb` — I/O Port Access

The x86 CPU has two address spaces: **memory** (accessed with normal load/store) and
**I/O ports** (accessed with `in`/`out` instructions). I/O ports are how the CPU
talks to hardware chips like the PIC, the PIT (timer), and the keyboard controller.

```c
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
```

- `"a"(val)` puts `val` in AL (byte-sized output).
- `"Nd"(port)` accepts either an 8-bit constant port address (the `N` constraint) or
  the DX register (the `d` constraint) for variable port addresses.
- `volatile` prevents GCC from reordering the port write relative to other I/O.

`io_wait()` (a write to port 0x80) burns approximately 1-4 microseconds between
consecutive PIC writes. Old ISA bus hardware needs this settling time; QEMU doesn't,
but it is correct practice.

---

## 10. `sti` / `cli` — Enabling and Disabling Interrupts

**`cli`** (Clear Interrupt Flag) disables hardware interrupts. The CPU will still
handle NMIs (Non-Maskable Interrupts) and CPU exceptions, but IRQs from the PIC are
ignored. Our kernel starts with interrupts disabled (the bootloader never called `sti`).

**`sti`** (Set Interrupt Flag) re-enables hardware interrupts. After `sti`, the PIC
can deliver IRQs to the CPU. The timer fires almost immediately (within ~55ms at the
default ~18 Hz rate).

We call `sti` only after both the IDT and PIC are fully configured. Calling `sti`
before `lidt` would mean the CPU receives a timer IRQ with no valid handler table and
immediately triple-faults.

---

## 11. The `pause` Instruction in the Spin Loop

```c
for (;;) {
    if (current != last_displayed) { ... }
    __asm__ volatile ("pause");
}
```

`pause` is an SSE2 hint instruction that tells the CPU: "this loop is a spin-wait."
On modern CPUs with hyperthreading, it yields the execution pipeline to the sibling
thread for a few cycles instead of spinning at full speed burning power. On QEMU
and older hardware it acts as a no-op but is the correct idiom for spin loops.

---

## 12. The Live Tick Counter Display

The timer handler increments `tick_count`, a `volatile uint32_t` global. `volatile`
is critical here: without it, the compiler is allowed to hoist the read of `tick_count`
out of the loop body (since nothing in the C code it can see modifies it), and the
counter would never appear to change. `volatile` forces a fresh memory read on every
loop iteration.

Rather than calling `kprintf()` to display the counter (which would move the cursor
and scroll the screen), we write directly to the VGA buffer at a fixed row/column
using a small helper function. This is safe because the VGA buffer is memory-mapped
hardware and direct pointer writes are always valid.

---

## 13. The Kernel Panic Handler

When a CPU exception fires, `interrupt_handler()` calls `kernel_panic()` which:
1. Switches to red-on-white color for maximum visibility.
2. Prints the exception name from the `exception_names[]` table.
3. Prints the error code (where the CPU provides one — e.g., #PF gives the faulting
   address in CR2, but we don't read CR2 yet).
4. Dumps EIP, CS, EFLAGS and all general-purpose registers.
5. Disables interrupts and halts.

The register dump is invaluable in later modules when a bug in paging, the allocator,
or process switching causes a fault — you can see exactly which address faulted and
what all the registers contained at that moment.

---

## 14. File Structure

| File           | Purpose |
|----------------|---------|
| `io.h`         | Inline `inb()`, `outb()`, `io_wait()` for I/O port access |
| `pic.h/c`      | 8259A PIC remapping, EOI, mask/unmask |
| `idt.h/c`      | IDT struct, `idt_set_gate()`, `lidt`, `idt_init()` |
| `isr.h`        | `struct interrupt_frame`, `irq_register()`, `interrupt_handler()` declaration |
| `isr.asm`      | 32 ISR stubs + 16 IRQ stubs + `isr_common_stub` |
| `isr.c`        | C dispatcher: exception panic handler, IRQ handler table |
| `kernel.c`     | Calls all init functions, registers timer, runs tick display loop |
| `vga.c/h`      | Carried forward from Module 3 |
| `gdt.c/h`      | Carried forward from Module 4 |

---

## 15. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| Dummy error code | Push `0` for no-error exceptions | Keeps `struct interrupt_frame` layout identical for all vectors |
| EOI timing | Before calling user handler | Allows PIC to accept other IRQs while handler runs |
| Gate type | Interrupt (0x8E) for all | IF is cleared automatically — no nested IRQs until we're ready |
| `volatile tick_count` | Required | Compiler would cache the value in a register without it; interrupt writes wouldn't be visible to the main loop |
| Tick display | Direct VGA buffer write | Avoids moving the VGA driver's cursor or scrolling the screen |
| Vectors 48-255 | Null gates (not installed) | These vectors never fire in a minimal kernel; a null gate gives a clean triple-fault if one somehow does |
| `io_wait()` between PIC writes | Yes | Correct for real ISA hardware; harmless on QEMU |
| `pause` in spin loop | Yes | Standard spin-wait idiom; reduces power on real hardware |

---

## 16. What We Proved By Getting This Working

- The IDT can be defined in C using packed structs (same discipline as the GDT).
- `lidt` loaded the IDTR successfully — the CPU accepted our table.
- 48 assembly stubs correctly save and restore the full CPU register state.
- The dummy error code trick makes the interrupt frame layout uniform.
- The PIC was remapped — timer fires at vector 32, not vector 8.
- `sti` was safe to call after the IDT and PIC were configured.
- The timer handler runs at ~18 Hz without crashing the kernel.
- `volatile` makes the tick counter visible across the interrupt/kernel boundary.
- The kernel panic handler prints useful diagnostic output for any CPU exception.

---

## 17. What Comes Next (Component 6 Preview)

Component 6 builds the **Physical Memory Manager (PMM)** — the system that tracks
which 4KB pages of RAM are free and which are in use. Before we can do paging (Module
7) or a heap allocator (Module 8), we need to know what RAM we actually have.

The PMM will:
- Read the BIOS memory map (passed via GRUB or a fixed probe) to know the total
  usable RAM
- Maintain a **bitmap**: one bit per 4KB page (1 = free, 0 = used)
- Provide `pmm_alloc_page()` and `pmm_free_page()` — the physical-layer equivalents
  of malloc/free

With the PMM, every subsequent module has a clean way to request and release memory.
