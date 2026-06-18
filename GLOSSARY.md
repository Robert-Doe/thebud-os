# BobOS — Glossary of Terms, Files, and Concepts

Every term, filename, and concept used across the project, alphabetically ordered.
The **First seen** column tells you which module introduced it.
This file is updated whenever a new module is added.

---

## How to read this file

- **Term** — the word, acronym, filename, or symbol
- **What it is** — plain-English explanation
- **First seen** — the earliest module where you encounter it

---

## A

### `__asm__ volatile` (GCC inline assembly)
GCC syntax for embedding raw assembly instructions inside C code. The template string
contains the assembly, followed by output operands (what C variables the asm writes),
input operands (what C values the asm reads), and a clobber list (registers the asm
destroys). `volatile` prevents GCC from reordering or eliminating the block. Used in
`gdt_load()` to call `lgdt`, perform the far return, and reload segment registers.
Also used for `lidt`, `sti`, `cli`, `inb`, `outb`, and `pause`.
**First seen:** Module 4

### `__attribute__((packed))` (GCC struct attribute)
Tells the GCC compiler to never insert padding bytes between struct fields. Required
for any struct that models a CPU hardware format (GDT entries, IDT gates, etc.)
because the CPU reads those structures as tightly-packed fixed-size records. Without
it, alignment padding can grow an 8-byte GDT entry to 12 bytes, causing the CPU to
read garbage and crash.
**First seen:** Module 4

### `.` (location counter)
Inside a linker script, the dot `.` is a special variable that tracks the current
memory address the linker is placing bytes at. Writing `. = 0x1000` tells the linker
"start placing everything at address 0x1000 from here."
**First seen:** Module 2

### `0x1000` (kernel load address)
The physical RAM address where the bootloader copies the kernel binary from disk.
Chosen because it is safely above the BIOS data area and IVT, well below the
bootloader at 0x7C00, and a clean power-of-two (4096).
**First seen:** Module 2

### `0x7C00` (bootloader load address)
The fixed address where the BIOS copies the boot sector and begins executing it.
This address is hardwired into the x86 BIOS specification — every PC does this.
**First seen:** Module 1

### `0x90000` (stack base)
Where we place the kernel stack in Protected Mode — above the kernel binary at
0x1000, below the 1MB boundary, in a region that is free RAM.
**First seen:** Module 2

### `0xAA55` (boot signature)
The two-byte magic number that must appear at bytes 510–511 of the boot sector.
The BIOS checks for this signature to decide whether a disk is bootable. If it is
missing, the BIOS skips the disk entirely. Stored little-endian on disk: 0x55 then
0xAA.
**First seen:** Module 1

### `0xB8000` (VGA buffer address)
The physical RAM address of the VGA text mode character buffer. The VGA hardware
reads this 4000-byte region continuously to refresh the display. Writing to these
addresses is how you put text on screen — there is no GPU, no draw call, no frame
buffer separate from this region.
**First seen:** Module 2

### Access Byte
One byte in every GDT descriptor entry that encodes the segment's type and
permissions. Bit 7 = Present (1 = valid segment). Bits 6–5 = privilege ring.
Bit 4 = descriptor type. Bit 3 = executable. Bit 1 = readable/writable.
**First seen:** Module 2

### Attribute Byte
The second byte of every VGA text cell (after the ASCII character byte). Upper
nibble = background color (0–15), lower nibble = foreground color (0–15). Determines
what color each character appears in.
**First seen:** Module 2

---

## B

### BIOS (Basic Input/Output System)
Firmware burned into a chip on the motherboard. The very first program the CPU runs
when the machine powers on. It initializes hardware (RAM, keyboard, disk, video),
runs a self-test (POST), then searches for a bootable disk. Once it finds one, it
copies the first 512-byte sector into RAM at 0x7C00 and jumps to it. The BIOS
operates in 16-bit Real Mode and provides interrupt-based services (INT 0x10 for
video, INT 0x13 for disk) that our bootloader uses. After Protected Mode is entered,
the BIOS can never be called again.
**First seen:** Module 1

### Boot Sector
The first 512-byte sector of a bootable disk. Must end with the boot signature
(0x55, 0xAA at bytes 510–511). The BIOS loads exactly this sector into RAM at 0x7C00
and transfers control to it. Our bootloader must fit entirely within these 512 bytes.
**First seen:** Module 1

### `boot.asm` / `boot2.asm`
The assembly source file for the bootloader. Module 1 calls it `boot.asm`;
Module 2 names it `boot2.asm` to reflect expanded functionality; Module 3 reverts
to `boot.asm` (each module directory is self-contained). Assembled by NASM into a
raw 512-byte binary.
**First seen:** Module 1

### `boot.bin`
The raw 512-byte binary produced by assembling `boot.asm`. This becomes the first
sector of `os.img`.
**First seen:** Module 1

### `boot.img` / `os.img`
The final bootable disk image: `boot.bin` concatenated with `kernel.bin`. The BIOS
can be pointed at this file by QEMU and will boot it as if it were a real floppy
disk. Module 1 calls it `boot.img`; Modules 2 and 3 call it `os.img` to reflect
that it contains both bootloader and kernel.
**First seen:** Module 1

### Bootloader
The small program (≤512 bytes) that the BIOS hands control to. Its job is to bridge
the gap between the BIOS and the kernel: load the kernel binary from disk into RAM,
set up the CPU environment the kernel needs (GDT, Protected Mode), then jump to the
kernel. Once the kernel is running, the bootloader is done — it is overwritten or
simply never called again.
**First seen:** Module 1

### `.bss` section
An ELF section that holds uninitialized global and static C variables (e.g.,
`int counter;` with no `= value`). Unlike `.data`, this section takes no space in
the binary file — the OS (or our kernel startup code) zeros this memory at runtime.
**First seen:** Module 2

---

## C

### `cli` (Clear Interrupt Flag)
An x86 instruction that clears the IF (Interrupt Enable) bit in EFLAGS, disabling
hardware interrupts (IRQs from the PIC). The CPU still handles NMIs and CPU
exceptions. Used before critical sections or before halting. The kernel starts with
interrupts disabled; `sti` re-enables them.
**First seen:** Module 1 (used in hang loop), Module 5 (deliberate use in init)

### `call` / `ret`
x86 instructions for function calls. `call addr` pushes the return address onto the
stack and jumps to `addr`. `ret` pops the return address and jumps back. Any C
function call compiles to a `call` instruction under the hood.
**First seen:** Module 2

### CHS (Cylinder, Head, Sector)
The addressing scheme BIOS INT 0x13 uses to identify a location on a floppy disk.
Cylinder = which concentric ring on the platter. Head = which read/write surface.
Sector = which 512-byte slice within the track (numbered from 1, not 0). Our kernel
starts at Cylinder 0, Head 0, Sector 2 (immediately after the bootloader).
**First seen:** Module 2

### `cli` (Clear Interrupt Flag)
An x86 instruction that disables hardware interrupts. Used before halting (`hlt`) to
prevent the CPU from being woken up by a timer or keyboard interrupt.
**First seen:** Module 1

### Code Segment
GDT entry 1 in our setup. Describes the region of memory the CPU fetches instructions
from. We set it to cover 0 → 4GB (flat model). The segment selector for it is 0x08
(GDT index 1 × 8 bytes).
**First seen:** Module 2

### Control Register (CR0)
A special CPU register that controls fundamental CPU behavior. Bit 0 is the PE
(Protection Enable) bit. Setting it transitions the CPU from Real Mode to Protected
Mode. CR0 cannot be written directly with an immediate value — you must load it into
a general register, modify it, and write it back.
**First seen:** Module 2

### Cursor
In the VGA driver, the logical position (row and column) where the next character
will be written. Tracked in software by the driver (`cur_row`, `cur_col`). Not the
hardware blinking cursor (which requires I/O port writes to the VGA controller chip —
not implemented yet).
**First seen:** Module 3

---

## D

### `.data` section
An ELF section that holds initialized global and static C variables (e.g.,
`int x = 5;`). These values are stored in the binary file and loaded into RAM at
startup.
**First seen:** Module 2

### Data Segment
GDT entry 2 in our setup. Describes the region of memory the CPU reads and writes
data from. Also covers 0 → 4GB (flat model). Selector = 0x10 (GDT index 2 × 8).
**First seen:** Module 2

### `cli` / `sti`
See **`cli`** and **`sti`**.

### `db` / `dw` / `dd` (NASM directives)
NASM pseudo-instructions for placing raw bytes in the output binary.
- `db` = define byte (1 byte)
- `dw` = define word (2 bytes)
- `dd` = define doubleword (4 bytes)
Used to hand-craft GDT entries and the boot signature.
**First seen:** Module 1

### DECISIONS.md
Documentation file created in every module directory before code is written.
Explains the "why" behind every design decision: what alternatives were considered,
why each choice was made, and what each piece of code does conceptually.
**First seen:** Module 1

### Dispatch Table
A C array of function pointers indexed by a key. In Module 5, `irq_handlers[16]` is
a dispatch table: `irq_handlers[0]` holds the timer handler, `irq_handlers[1]` will
hold the keyboard handler (Module 9), etc. `interrupt_handler()` looks up the
appropriate entry by IRQ number and calls it.
**First seen:** Module 5

### Driver
A module that takes exclusive ownership of a piece of hardware and exposes a clean
API for the rest of the kernel. Nothing outside the driver touches the hardware
directly. In Module 3 this means only `vga.c` writes to 0xB8000 — all other code
calls `kprintf()`.
**First seen:** Module 3

---

## E

### EOI (End-of-Interrupt)
A command (`0x20`) written to the PIC's command port after an IRQ handler finishes.
Without EOI, the PIC's in-service register stays latched and it will never deliver
another interrupt on that line. For IRQ8-15 (slave PIC) you must send EOI to both
the slave (`0xA0`) and the master (`0x20`), because the slave's signal reaches the
master on the cascade line (IRQ2).
**First seen:** Module 5

### Exception (CPU Exception)
A synchronous interrupt triggered by the CPU itself when it detects an error or
special condition during instruction execution. Examples: divide-by-zero (#DE),
invalid opcode (#UD), page fault (#PF), general protection fault (#GP). Exceptions
are handled by IDT vectors 0-31. Some exceptions push an error code onto the stack;
others do not. See also **IRQ**.
**First seen:** Module 5

### EBP / ESP
32-bit stack registers. ESP (Extended Stack Pointer) points to the current top of the
stack. EBP (Extended Base Pointer) is used by function prologues to anchor the stack
frame. We initialize ESP = EBP = 0x90000 before calling the kernel.
**First seen:** Module 2

### ELF (Executable and Linkable Format)
The standard binary file format on Linux and most Unix systems. GCC and NASM (with
`-f elf32`) produce ELF object files. ELF files contain named sections (`.text`,
`.data`, `.bss`) and a symbol table. The GNU linker (`ld`) merges ELF object files
into a final ELF executable or (with `--oformat binary`) a flat raw binary.
**First seen:** Module 2

### `ENTRY()` (linker script directive)
Tells the linker which symbol is the program's entry point. We use `ENTRY(_start)`
to mark the first instruction in `kernel_entry.asm`.
**First seen:** Module 2

---

## F

### Far Jump (`jmp segment:offset`)
An x86 jump that changes both the instruction pointer (EIP) and the code segment
register (CS) simultaneously. Required immediately after setting the PE bit in CR0
to flush the CPU's instruction pipeline of any stale 16-bit instructions. Also
reloads CS with the new Protected Mode segment selector.
**First seen:** Module 2

### Flat Memory Model
A GDT configuration where both the code and data segments span the entire 4GB address
space (base = 0, limit = 4GB). This means an address in C code equals its physical
RAM address — no segment offset calculation needed. We use this for simplicity; finer
memory control comes later via paging (Module 7).
**First seen:** Module 2

### `-ffreestanding` (GCC flag)
Tells GCC to compile without assuming a standard library or host OS exists. No
`main()` entry point is expected, no `printf()`, no `malloc()`. Essential when
building kernel code that runs on bare metal.
**First seen:** Module 2

### `-fno-pie` (GCC flag)
Tells GCC not to generate position-independent executable code. Our kernel is loaded
at a fixed address (0x1000), so all absolute addresses in the code must be hardcoded.
Position-independent code uses relative addresses that could conflict with our linker
script.
**First seen:** Module 2

---

## G

### GCC (GNU Compiler Collection)
The C compiler used for kernel code. We invoke it with `-m32 -ffreestanding -fno-pie
-c -O0` to produce 32-bit ELF object files suitable for our bare-metal kernel.
**First seen:** Module 2

### GDT (Global Descriptor Table)
A table in RAM, required by the x86 CPU before Protected Mode can be entered. Each
8-byte entry (called a descriptor) defines one memory segment: where it starts, how
large it is, and who is allowed to access it. The CPU is told where the GDT lives via
the `lgdt` instruction, which loads the GDTR register.
**First seen:** Module 2

### GDTR (GDT Register)
A hidden 48-bit register inside the CPU that stores the address and size of the GDT.
Loaded by the `lgdt` instruction. The CPU consults the GDTR on every memory access
to find the GDT and enforce segment permissions.
**First seen:** Module 2

### `gdt.c`
The C source file that implements the GDT: defines the `gdt[]` array and `gdtr`
descriptor as static module-private variables, provides `gdt_set_entry()` to fill
one entry from readable parameters, `gdt_load()` to execute `lgdt` and reload all
segment registers via inline assembly, and the public `gdt_init()` entry point.
**First seen:** Module 4

### `gdt.h`
The public header for the GDT module. Defines the `struct gdt_entry` and
`struct gdt_descriptor` packed structs, all `GDT_ACCESS_*` and `GDT_FLAG_*` bit
constants, the `GDT_SEL_CODE` / `GDT_SEL_DATA` selector values, and the
`gdt_init()` declaration.
**First seen:** Module 4

### `gdt_init()`
The single public function of the GDT module. Calls `gdt_set_entry()` for all three
entries (null, code, data) and then calls `gdt_load()`. Called once, early in
`kernel_main()`, before anything that could cause a memory access fault.
**First seen:** Module 4

### `gdt_load()`
Internal function in `gdt.c`. Sets `gdtr.size` and `gdtr.offset`, executes `lgdt`
via inline assembly to load the GDTR register, then performs the far-return trick to
reload CS, then reloads DS/ES/FS/GS/SS with `GDT_SEL_DATA`.
**First seen:** Module 4

### `gdt_set_entry()`
Internal function in `gdt.c`. Takes a base address, limit, access byte, and flags
nibble and correctly packs them into the scrambled 8-byte Intel GDT descriptor
format. Called three times by `gdt_init()`.
**First seen:** Module 4

### Granularity Bit (G)
Bit 3 of the Flags nibble in a GDT descriptor. When G = 1, the segment limit is
interpreted in 4KB pages rather than bytes. A limit of 0xFFFFF with G = 1 means
0xFFFFF × 4096 = 4GB — which is how our flat segments cover all of memory.
**First seen:** Module 2

---

## H

### Gate Descriptor
An 8-byte IDT entry. Stores the handler's address (split across two 16-bit fields),
the code segment selector, a reserved zero byte, and the type_attr byte (gate type
and privilege level). Analogous to a GDT descriptor — same 8-byte size, same
`__attribute__((packed))` requirement, same `idt_set_gate()` builder to hide the
scrambled layout.
**First seen:** Module 5

### General Protection Fault (#GP)
A CPU exception (interrupt vector 13) fired whenever a program violates a protection
rule: accessing a segment with the wrong privilege, using a null selector, executing
a privileged instruction from Ring 3, or violating segment bounds. After Module 5
(IDT) we can catch and handle these rather than letting the machine triple-fault.
**First seen:** Module 4

### `idt.c` / `idt.h`
The IDT module. `idt.h` defines the packed `struct idt_entry` and
`struct idt_descriptor`, the `IDT_GATE_INTERRUPT` / `IDT_GATE_TRAP` constants, and
declares `idt_set_gate()` and `idt_init()`. `idt.c` holds the static `idt[]` array,
implements `idt_set_gate()` (splits handler address, sets fields), and `idt_init()`
(installs all 48 gates and calls `lidt`).
**First seen:** Module 5

### `idt_init()`
Public function that installs all 48 interrupt gates (32 exceptions + 16 IRQs) into
the IDT and executes `lidt` to load the IDTR register. Called once in `kernel_main()`
after `pic_init()` and before `sti`.
**First seen:** Module 5

### `idt_set_gate()`
Fills one IDT entry with a handler address, code segment selector, and type byte.
Splits the 32-bit handler address into `offset_low` and `offset_high` fields.
**First seen:** Module 5

### `inb` / `outb`
Inline assembly wrapper functions in `io.h` for the x86 `in` and `out` instructions,
which read and write the CPU's I/O port address space. Used to communicate with the
PIC, PIT (timer), and keyboard controller. I/O ports are separate from RAM — writing
to port 0x20 does not change memory at address 0x20.
**First seen:** Module 5

### Interrupt Gate
An IDT gate type (0x8E) where the CPU automatically clears the IF flag (disables
interrupts) when the handler is entered. This prevents the handler from being
interrupted by another IRQ before it has finished. The CPU restores IF via the
EFLAGS saved on the stack when `iret` executes.
**First seen:** Module 5

### `interrupt_handler()`
The single C function called by `isr_common_stub` for all 48 vectors. Checks
`frame->int_no`: if < 32, calls the exception panic handler; if 32-47, sends EOI to
the PIC and dispatches to the registered IRQ handler.
**First seen:** Module 5

### `io.h`
Header providing `outb()`, `inb()`, and `io_wait()` as static inline functions.
Any module that needs to talk to I/O-port hardware includes this file.
**First seen:** Module 5

### `iret` (Interrupt Return)
An x86 instruction that pops EIP, CS, and EFLAGS from the stack (the values the CPU
pushed when it entered the interrupt). This atomically restores the execution context
of the interrupted code. In a privilege-change scenario it also pops ESP and SS.
**First seen:** Module 5

### IRQ (Interrupt Request)
A hardware interrupt signal from a peripheral device. The 8259A PIC arbitrates 16
IRQ lines (IRQ0-15). After PIC remapping, IRQ0-15 map to CPU vectors 32-47. Unlike
CPU exceptions, IRQs are asynchronous — they can arrive at any moment during code
execution.
**First seen:** Module 5

### `irq_register()`
Installs a C handler function into the `irq_handlers[]` dispatch table for a given
IRQ line. Any module that owns a hardware device (timer, keyboard, disk) calls this
to receive interrupts from that device.
**First seen:** Module 5

### `isr.asm`
Assembly file containing 48 interrupt stubs (ISR0-ISR31, IRQ0-IRQ15) plus
`isr_common_stub`. Each stub saves the interrupt vector number (and a dummy error
code if needed), then jumps to the common stub which saves all registers and calls
the C `interrupt_handler()`.
**First seen:** Module 5

### `isr.c` / `isr.h`
C-level interrupt dispatcher. `isr.h` defines `struct interrupt_frame`, the
`irq_handler_t` typedef, and declarations for `irq_register()` and
`interrupt_handler()`. `isr.c` implements the dispatcher: exception panic with
register dump, IRQ EOI and dispatch.
**First seen:** Module 5

### ISR (Interrupt Service Routine)
The handler function executed when a specific interrupt or exception fires. In this
project, ISR refers specifically to CPU exception handlers (vectors 0-31) as distinct
from IRQ handlers (vectors 32-47), though they share the same dispatch mechanism.
**First seen:** Module 5

### `isr_common_stub`
The shared assembly entry point called by all 48 interrupt stubs. Executes `pusha`
(saves general-purpose registers), saves segment registers, switches to kernel data
segment, passes `esp` as a pointer to `interrupt_handler()`, then restores everything
and executes `iret`.
**First seen:** Module 5

### `hlt`
An x86 instruction that halts the CPU until an interrupt arrives. Used in hang loops
(`cli` + `hlt` + `jmp`) to stop execution gracefully when there is nothing left to do.
**First seen:** Module 1

---

## I

### Inline Assembly
See **`__asm__ volatile`**.

### INT (software interrupt)
An x86 instruction that triggers an interrupt service routine. In Real Mode, the BIOS
provides services via interrupts: `INT 0x10` for video output, `INT 0x13` for disk
I/O. These stop working once Protected Mode is entered.
**First seen:** Module 1

### `INT 0x10` (BIOS video interrupt)
BIOS video services. With AH = 0x0E (teletype output), prints the character in AL to
the screen. Used by our bootloader to print status messages before switching modes.
**First seen:** Module 1

### `INT 0x13` (BIOS disk interrupt)
BIOS disk services. With AH = 0x02 (read sectors), copies sectors from disk into RAM
at ES:BX. We use this to load the kernel binary from disk before entering Protected
Mode.
**First seen:** Module 2

### IVT (Interrupt Vector Table)
A 1KB table at physical address 0x00000–0x003FF in Real Mode. Contains 256 four-byte
pointers to BIOS interrupt handlers. We load the kernel above this area (at 0x1000)
to avoid overwriting it before we're done using BIOS services.
**First seen:** Module 2

---

## K

### `lidt` (Load IDT Register)
An x86 instruction that loads the IDTR (Interrupt Descriptor Table Register) with a
6-byte structure: 2-byte size and 4-byte base address of the IDT. Analogous to `lgdt`
for the GDT. After `lidt`, the CPU uses the new IDT for all subsequent interrupts and
exceptions. Called from `idt_init()` via inline assembly.
**First seen:** Module 5

### `lret` (far return)
An x86 instruction that pops EIP then CS off the stack and jumps there. Used in
`gdt_load()` as the only practical way to update the CS register from C code —
you cannot `mov` directly into CS. By pushing `GDT_SEL_CODE` then a return address
before `lret`, the CPU simultaneously updates both CS and EIP.
**First seen:** Module 4

### Kernel Panic
An unrecoverable kernel error. When a CPU exception fires and there is no safe way to
continue, the kernel prints diagnostic information (exception name, error code, all
registers, EIP of the faulting instruction) and halts. Implemented in `isr.c` as
`kernel_panic()`. The red color scheme makes it visually unmistakable.
**First seen:** Module 5

### `KEEP()` (linker script directive)
Prevents the linker from discarding a section as unreferenced. We use `KEEP(*(.text))`
to ensure `_start` is always included even though nothing in C explicitly calls it.
**First seen:** Module 2

### Kernel
The central program of an operating system. It runs in Ring 0 (the most privileged
CPU mode), has direct control over all hardware, manages memory and processes, and
enforces the rules that keep user programs from interfering with each other. Everything
we build after the bootloader is kernel code.
**First seen:** Module 2

### `kernel.bin`
The flat raw binary of the compiled kernel, produced by the linker stripping all ELF
headers from `kernel.elf`. This is what the bootloader loads from disk — it must
start with the first instruction of `kernel_entry.asm` at address 0x1000.
**First seen:** Module 2

### `kernel.c`
The main C source file for the kernel. In Module 2 it directly wrote to VGA memory.
In Module 3 and beyond it calls `kprintf()` via the VGA driver.
**First seen:** Module 2

### `kernel_entry.asm`
A tiny assembly stub that is the first code in the kernel binary. The linker places
it at 0x1000 so the bootloader's jump lands here. Its only job is to call
`kernel_main()` in C, then hang forever if it returns.
**First seen:** Module 2

### `kernel_main()`
The C entry point of the kernel. Called by `kernel_entry.asm`. This is where all
kernel initialization happens — in Module 2 it printed to VGA directly; in Module 3
it uses `kprintf()`.
**First seen:** Module 2

### `kprintf()`
The kernel's minimal printf implementation in `vga.c`. Supports `%c`, `%s`, `%d`,
`%u`, `%x`, `%X`, and `%%`. Does not require any standard library. Uses `<stdarg.h>`
(provided by GCC itself, not libc) to handle variadic arguments.
**First seen:** Module 3

---

## L

### `ld` (GNU Linker)
Combines multiple ELF object files into a single binary, resolving symbol references
between them. Controlled by a linker script (`linker.ld`). We use the `elf_i386`
emulation for 32-bit ELF output and `--oformat binary` to produce a flat binary.
**First seen:** Module 2

### `lgdt` (Load GDT Register)
An x86 instruction that loads the GDTR register with a 6-byte structure containing
the GDT's size (minus 1) and its physical address. Must be called before switching to
Protected Mode.
**First seen:** Module 2

### `linker.ld`
The GNU linker script that controls how object files are combined into the final
kernel binary. Specifies that the binary starts at address 0x1000 and places
`kernel_entry.asm`'s `_start` symbol first.
**First seen:** Module 2

### Location Counter (`.`)
See **`.` (location counter)**.

---

## M

### `Makefile`
A build script read by the `make` tool. Defines rules for assembling, compiling,
linking, and combining files into `os.img`. Also provides `make run` to launch QEMU
and `make clean` to remove build artifacts.
**First seen:** Module 1

### `-m32` (GCC flag)
Tells GCC to produce 32-bit x86 machine code, regardless of whether the host machine
is 64-bit.
**First seen:** Module 2

### MinGW (Minimalist GNU for Windows)
A port of the GNU toolchain (gcc, ld, etc.) to Windows that produces Windows PE/COFF
executables. Used in Module 2 to compile C code. In Module 3 we discovered MinGW's
linker only supports the `i386pe` emulation and cannot produce ELF output, so we
switched to WSL for linking.
**First seen:** Module 2

---

## N

### NASM (Netwide Assembler)
The assembler used to convert `.asm` source files into machine code. Supports
multiple output formats: `-f bin` for a raw binary (the bootloader), `-f elf32` for
an ELF object file (the kernel entry stub).
**First seen:** Module 1

### Null Descriptor
The mandatory first entry (index 0) in the GDT. Must be all zeros. If a segment
register is ever loaded with selector 0 by accident, the CPU immediately fires a
General Protection Fault rather than accessing arbitrary memory. A hardware safety
net built into the x86 spec.
**First seen:** Module 2

---

## O

### `-O0` (GCC flag)
Disables all compiler optimizations. Code runs exactly as written, which makes it
much easier to follow with a debugger or disassembler.
**First seen:** Module 2

### `objcopy`
A GNU binutils tool that converts between binary file formats. We use
`objcopy -O binary` to strip all ELF headers from the linked kernel ELF file and
produce a flat raw binary that the bootloader can jump to directly.
**First seen:** Module 2

### `ORG` (NASM directive)
Tells NASM what memory address the binary will be loaded at. `[ORG 0x7C00]` means
"all labels and addresses in this file are relative to 0x7C00." Required so that
BIOS string addresses in the bootloader resolve correctly.
**First seen:** Module 1

### `os.img`
See **`boot.img` / `os.img`**.

---

## P

### `pause` (x86 instruction)
An SSE2 hint instruction used inside spin-wait loops. Signals to the CPU that the
loop is polling for a condition, allowing it to reduce power consumption and yield
execution resources to a sibling hyperthreading core. Acts as a no-op on older CPUs
and QEMU, but is the correct idiom for spin loops.
**First seen:** Module 5

### PIC (Programmable Interrupt Controller) / 8259A
The hardware chip that manages IRQ lines from peripherals and delivers them to the
CPU. Two 8259A PICs are cascaded: the master handles IRQ0-7, the slave handles
IRQ8-15. The BIOS programs them to conflicting vectors (8-15) which must be remapped
to vectors 32-47 before enabling CPU interrupts.
**First seen:** Module 5

### `pic.c` / `pic.h`
PIC module. `pic.h` defines port addresses, vector offsets, and declarations.
`pic.c` implements `pic_init()` (the 4-step ICW remapping sequence), `pic_send_eoi()`
(sends EOI to master and optionally slave), and `pic_set_mask()` / `pic_clear_mask()`
for enabling/disabling individual IRQ lines.
**First seen:** Module 5

### `pic_init()`
Remaps both PIC chips from their default BIOS vector offsets (8 and 112) to
vectors 32 and 40. Sends the 4 Initialization Command Words (ICW1-ICW4) to each
PIC in sequence. Must be called before `idt_init()` and before `sti`.
**First seen:** Module 5

### `pic_send_eoi()`
Sends the End-of-Interrupt command to the PIC(s). Must be called at the end of every
hardware IRQ handler. For IRQ8-15, sends to both slave and master.
**First seen:** Module 5

### `pusha` / `popa`
x86 instructions that push/pop all eight general-purpose registers at once.
`pusha` pushes: EAX, ECX, EDX, EBX, ESP (original), EBP, ESI, EDI.
`popa` restores them in reverse order. Used in `isr_common_stub` to save and restore
the full CPU state around interrupt handler calls.
**First seen:** Module 5

### PE (Portable Executable)
The binary format used by Windows executables and DLLs. MinGW's linker produces PE
format. Starts with an "MZ" header. Not suitable for our kernel because the bootloader
expects a raw binary, not a file with headers.
**First seen:** Module 3 (discovered as a problem to work around)

### PE Bit (Protection Enable)
Bit 0 of the CR0 control register. Setting this single bit switches the CPU from
Real Mode to Protected Mode. The transition is instantaneous but must be followed
immediately by a far jump to flush the instruction pipeline.
**First seen:** Module 2

### POST (Power-On Self-Test)
Hardware diagnostics run by the BIOS at startup before it hands control to any
software. Checks that RAM, keyboard, and other basic hardware are functional. If POST
fails, the BIOS beeps error codes through the PC speaker.
**First seen:** Module 1

### Privilege Rings
The x86 CPU's four levels of trust: Ring 0 (kernel, most privileged) through Ring 3
(user programs, least privileged). Code in Ring 3 cannot execute privileged
instructions or access Ring 0 memory — the CPU enforces this in hardware. Most OSes
use only Ring 0 and Ring 3.
**First seen:** Module 2

### Protected Mode
A CPU operating mode introduced with the Intel 80286 (1982) and fully realized on the
80386 (1985). Provides: 32-bit registers, up to 4GB of addressable RAM, hardware
memory protection via segments and rings. Required for any real OS. The CPU must be
explicitly switched to Protected Mode by the bootloader after all BIOS services are
done.
**First seen:** Module 2

---

## Q

### QEMU
An open-source machine emulator. We use `qemu-system-i386` to run our OS in a virtual
x86 machine without needing real hardware. QEMU emulates the CPU, BIOS, RAM, VGA
display, and floppy drive from our `os.img` file.
**First seen:** Module 1

---

## R

### RAM (Random Access Memory)
The working memory of the computer. Our kernel binary is loaded into RAM by the
bootloader. All VGA buffer writes go to a specific RAM region (0xB8000) that is
memory-mapped to the display hardware.
**First seen:** Module 1

### Real Mode
The initial 16-bit operating mode of all x86 CPUs at boot. Limitations: only 1MB of
addressable RAM, 16-bit registers, no memory protection, no privilege rings. BIOS
services work here. The bootloader runs in Real Mode and switches to Protected Mode
before calling the kernel.
**First seen:** Module 1

### Ring 0 / Ring 3
See **Privilege Rings**.

### `.rodata` section
An ELF section for read-only data — string literals (`"hello"`) and `const` arrays.
We merge it into `.data` in our linker script since we have no enforcement of
read-only access yet (that comes with paging).
**First seen:** Module 2

### ROADMAP.md
The top-level file listing all 13 planned OS components, their directories, and
their current build status. Updated as each module is completed.
**First seen:** Module 1

---

## S

### Scroll
When the VGA driver's cursor reaches the last row (row 24), it shifts every row up
by one and blanks the bottom row, so new output always has a place to go. Implemented
in `vga.c` by physically copying the VGA buffer contents.
**First seen:** Module 3

### Sector
The smallest addressable unit on a floppy or hard disk — 512 bytes. The BIOS reads
and writes whole sectors. The bootloader occupies sector 1; the kernel starts at
sector 2.
**First seen:** Module 1

### Segment Register (CS, DS, SS, ES, FS, GS)
CPU registers that hold segment selectors in Protected Mode (or raw base addresses
in Real Mode). CS = code segment, DS = data segment, SS = stack segment. After the
far jump into Protected Mode, all segment registers must be reloaded with valid
Protected Mode selectors.
**First seen:** Module 1

### Segment Selector
A 16-bit value loaded into a segment register in Protected Mode. The upper 13 bits
are the GDT index, bit 2 is the table indicator (0 = GDT), bits 1–0 are the
requested privilege level. Selector 0x08 = GDT entry 1 (code), 0x10 = entry 2 (data).
**First seen:** Module 2

### `_start`
The symbol name for the first instruction of our kernel binary, defined in
`kernel_entry.asm`. The linker script uses `ENTRY(_start)` to mark it as the entry
point, and `KEEP(*(.text))` ensures it is placed first at address 0x1000.
**First seen:** Module 2

### `stdarg.h`
A C header providing macros for variadic functions: `va_list`, `va_start`, `va_arg`,
`va_end`. This header is provided by the GCC compiler itself (not by libc), so it
works correctly in `-ffreestanding` mode. Used to implement `kprintf()`.
**First seen:** Module 3

### `sti` (Set Interrupt Flag)
An x86 instruction that sets the IF bit in EFLAGS, enabling hardware interrupts
from the PIC. Called once in `kernel_main()` after the IDT and PIC are fully
configured. After `sti`, the timer fires within ~55ms at the default ~18 Hz PIT rate.
**First seen:** Module 5

### `struct interrupt_frame`
A C struct (packed) that maps exactly onto the register save area built on the stack
by `isr_common_stub`. Fields: segment registers (gs, fs, es, ds), general-purpose
registers from `pusha` (edi through eax), then `int_no` and `err_code` pushed by the
stub, then `eip`, `cs`, `eflags` pushed by the CPU. A pointer to this struct is
passed to `interrupt_handler()`.
**First seen:** Module 5

### `sgdt` (Store GDT Register)
An x86 instruction that writes the CPU's internal GDTR register (6 bytes: 2-byte
limit then 4-byte base) into a memory operand. Used in `kernel_main()` to verify
that `lgdt` was accepted and the GDTR base address matches our C array.
**First seen:** Module 4

### Stack
A region of RAM used for function calls: storing return addresses, local variables,
and function arguments. Grows downward in x86. We initialize the kernel stack at
0x90000 before calling `kernel_main()`.
**First seen:** Module 2

---

## T

### Timer (PIT — Programmable Interval Timer)
The Intel 8253/8254 chip that generates periodic timer interrupts (IRQ0). At the
default frequency (~1.19 MHz input clock, divide-by-65536) it fires at ~18.2 Hz.
Module 5 registers a handler for IRQ0 (vector 32) that increments `tick_count` on
every timer interrupt, demonstrating that hardware interrupts are working.
**First seen:** Module 5

### Triple Fault
A catastrophic CPU condition where: (1) an exception fires, (2) the handler causes
another exception, (3) the double-fault handler (#DF) causes yet another exception.
The CPU has no fourth-level handler and resets the machine. Before Module 5, any
exception would triple-fault because the IDT was empty. Now our exception handlers
catch and display them instead.
**First seen:** Module 5

### `.text` section
An ELF section containing executable machine code — the compiled/assembled
instructions. This is always placed first in our binary so that `_start` is the
very first byte at 0x1000.
**First seen:** Module 2

### `times` (NASM directive)
Repeats a byte value N times. `times 510 - ($ - $$) db 0` pads the boot sector
with zeros up to byte 510, making room for the 2-byte boot signature at bytes
510–511.
**First seen:** Module 1

---

## V

### `va_list` / `va_start` / `va_arg` / `va_end`
C macros from `<stdarg.h>` for accessing variadic function arguments (the `...`
parameters). `va_start` initializes the list, `va_arg` reads the next argument with
a specified type, `va_end` cleans up. Used in `kprintf()`.
**First seen:** Module 3

### VGA (Video Graphics Array)
The display standard built into all x86 PCs. In text mode (mode 3), it provides an
80×25 grid of characters driven by the 4000-byte buffer at 0xB8000. We write
directly to this buffer — no GPU, no driver library needed.
**First seen:** Module 2

### `vga.c`
The VGA text driver implementation. Owns all writes to 0xB8000. Maintains cursor
state, handles special characters (`\n`, `\t`, `\r`), implements scrolling, and
provides `kprintf()`.
**First seen:** Module 3

### `vga.h`
The public header for the VGA driver. Defines color constants, the `VGA_COLOR()`
macro, and function declarations. Any kernel module that wants to print anything
includes this file.
**First seen:** Module 3

### `VGA_COLOR(bg, fg)` macro
Builds the VGA attribute byte by shifting the background color into the upper nibble
and OR-ing in the foreground color: `((bg << 4) | fg)`. Results in a single byte
that the VGA hardware interprets as a color pair.
**First seen:** Module 3

### `volatile`
A C keyword that tells the compiler: "do not optimize away reads or writes to this
variable, because something outside C's view may be observing it." Used on the VGA
buffer pointer so the compiler cannot remove writes it considers "dead." Any
memory-mapped hardware register must be accessed through a `volatile` pointer.
**First seen:** Module 2

---

## W

### WSL (Windows Subsystem for Linux)
A Windows feature that runs a real Linux kernel and userspace inside Windows.
Starting in Module 3, we use WSL's GCC and GNU `ld` for C compilation and linking,
because the native MinGW32 linker only supports the PE format and cannot produce
the ELF binaries our kernel requires. NASM still runs natively on Windows.
**First seen:** Module 3

---

## X

### x86
The CPU architecture used throughout this project. Refers to the family of Intel
processors starting with the 8086 (1978) and extended through 286, 386, 486,
Pentium, and modern CPUs. All x86 CPUs start in 16-bit Real Mode and must be
explicitly switched to 32-bit Protected Mode or 64-bit Long Mode by software.
**First seen:** Module 1

---

---

## Module 6 additions

### Bitmap Allocator
A data structure that represents the state of a pool of fixed-size units (pages) as
an array of bits. One bit per page: 1 = free, 0 = used in our convention. Scanning
for a free page is O(n/8) because entire bytes of zero can be skipped in one
comparison. At 4 KB pages, tracking 128 MB of RAM takes only 4 KB of bitmap storage.
**First seen:** Module 6

### `kernel_end` (linker symbol)
A symbol exported from the linker script (`kernel_end = .`) that marks the first
byte immediately past the end of the kernel image. In C, accessed as
`extern char kernel_end;` and used as `(uint32_t)&kernel_end` — take the **address**
of the symbol, not its value, because it has no storage. The PMM uses this to avoid
handing out any page the kernel occupies.
**First seen:** Module 6

### Page
The fundamental unit of physical memory managed by the PMM. On x86, one page = 4096
bytes (4 KB). Every page starts at an address that is a multiple of 4096. The MMU
hardware works exclusively in pages, so the PMM, paging system, and heap allocator
all speak the same unit.
**First seen:** Module 6

### Page Number
An integer that identifies a physical page. Computed as `physical_address / 4096`
(equivalently, `address >> 12`). Page 0 = 0x00000000, page 1 = 0x00001000,
page 256 = 0x00100000 (the 1 MB boundary), page 32767 = 0x07FFF000 (last page in
128 MB). The bitmap is indexed by page number.
**First seen:** Module 6

### Physical Address Space
The flat array of byte-addressable locations the CPU can put onto the memory bus,
from 0x00000000 upward. Not all addresses are usable RAM — some regions are
memory-mapped hardware (VGA at 0xA0000, BIOS ROM at 0xC0000). The PMM only
manages regions that are actual writable RAM.
**First seen:** Module 6

### PMM (Physical Memory Manager)
The kernel subsystem that tracks which 4 KB pages of physical RAM are free and
which are in use. Provides `pmm_alloc_page()` (claim a free page, returns its
physical address) and `pmm_free_page()` (release a page back to the pool). Every
higher-level memory system (paging, heap) sits on top of the PMM.
**First seen:** Module 6

### `pmm_alloc_page()`
Returns the physical address of one free 4 KB page and marks it used. Scans the
bitmap byte-by-byte for the first non-zero byte, then finds the lowest set bit
within that byte (first-fit). Returns NULL (physical address 0) if no pages are
free.
**First seen:** Module 6

### `pmm_free_page()`
Returns a previously allocated page to the free pool by setting its bit in the
bitmap. Silently ignores double-frees and out-of-range addresses.
**First seen:** Module 6

### `pmm_init()`
Initialises the PMM by zeroing the bitmap (all pages used), then marking pages above
`kernel_end` (rounded up to 4 KB) as free. Enforces a 1 MB floor: pages 0–255
(0x00000000–0x000FFFFF) are always left as used regardless of `kernel_end`.
**First seen:** Module 6

### `pmm.c` / `pmm.h`
The physical memory manager module. `pmm.h` declares the public API and constants
(`PMM_PAGE_SIZE`, `PMM_MAX_PAGES`). `pmm.c` holds the static bitmap array,
bit-manipulation helpers, and the implementations of `pmm_init`,
`pmm_alloc_page`, `pmm_free_page`, and the three stat functions.
**First seen:** Module 6

### 1 MB Reserved Floor
The hard rule that the PMM never allocates pages 0–255 (physical 0x00000000–
0x000FFFFF). This region contains the Real-Mode IVT, BIOS Data Area, bootloader,
VGA frame buffer (0xA0000), and BIOS ROMs (0xC0000). Writing to these pages would
corrupt hardware state or have no effect (ROM). Enforced in `pmm_init()` with:
`if (free_start < 256) free_start = 256;`
**First seen:** Module 6

### First-Fit Allocation
An allocation strategy where the allocator always returns the lowest-numbered free
unit in its pool. Simple, correct, and produces no fragmentation when every
allocation is the same size (one page). The PMM uses first-fit: it scans from
byte 0 of the bitmap and returns the first free page found.
**First seen:** Module 6

### Double-Free Guard
A safety check in `pmm_free_page()` that silently ignores an attempt to free a page
that is already free. In a more advanced kernel this would be a panic (double-free
is a serious memory corruption bug), but silent ignore is safer during early
development when the PMM is new.
**First seen:** Module 6

---

---

## Module 7 additions

### CR3 (Page Directory Base Register / PDBR)
A CPU control register that holds the physical address of the active page
directory. The CPU reads CR3 on every TLB miss to find the top-level page
table. Writing CR3 also flushes the entire TLB. Loaded with `mov %0, %%cr3`
via inline assembly immediately before enabling the PG bit.
**First seen:** Module 7

### Identity Mapping
A paging configuration where virtual address N maps to physical address N —
the page tables are "transparent." Used in Module 7 for the first 4 MB so that
all existing kernel pointers (code, stack, VGA buffer, PMM bitmap) remain valid
after paging is enabled. Virtual == physical for the mapped range.
**First seen:** Module 7

### Page Directory (PD)
The top-level page table structure in x86 32-bit paging. One 4 KB page (1024 ×
4-byte entries). Indexed by bits 31–22 of the virtual address. Each entry (PDE)
points to a Page Table. One page directory per address space. Its physical
address is loaded into CR3.
**First seen:** Module 7

### Page Directory Entry (PDE)
A 32-bit entry in the Page Directory. Bits 31–12 hold the physical address of a
Page Table (must be 4 KB aligned). Bit 0 = Present (1 = valid), Bit 1 = Writable,
Bit 2 = User accessible. If Present = 0, any access into that 4 MB region
triggers a #PF.
**First seen:** Module 7

### Page Fault (#PF, vector 14)
A CPU exception triggered when the CPU's page walker finds a not-present PDE
or PTE, or when an access violates a page's permission flags (e.g., writing a
read-only page, or Ring 3 accessing a supervisor-only page). The faulting virtual
address is stored in CR2. Our IDT handler calls `kernel_panic()`.
**First seen:** Module 7

### Page Table (PT)
A second-level table in the x86 paging hierarchy. One 4 KB page (1024 × 4-byte
entries). Indexed by bits 21–12 of the virtual address. Each entry (PTE) gives
the physical address of one 4 KB page. One page table covers 1024 × 4 KB = 4 MB.
**First seen:** Module 7

### Page Table Entry (PTE)
A 32-bit entry in a Page Table. Bits 31–12 hold the physical frame address. Same
flag layout as a PDE: Present, Writable, User, Accessed, Dirty, etc. The physical
frame address ORed with flags produces the complete PTE value.
**First seen:** Module 7

### `paging.h` / `paging.c`
The paging module. `paging.h` declares `paging_init()`, the flag constants
(`PAGE_PRESENT`, `PAGE_WRITABLE`, `PAGE_USER`), and diagnostic getters.
`paging.c` allocates the PD and PT from the PMM, fills the identity-map PTEs,
loads CR3, and sets the PG bit in CR0 to enable paging.
**First seen:** Module 7

### `paging_init()`
Builds the two-level page tables for the first 4 MB identity map, loads CR3,
and enables paging by setting bit 31 of CR0. After this returns, every memory
access is translated through the hardware page walker.
**First seen:** Module 7

### PG Bit (bit 31 of CR0)
The flag in the CPU's CR0 control register that enables paging. When set, the
CPU translates every virtual address through the page directory pointed to by
CR3. Must be set with a read-modify-write sequence to preserve other CR0 flags.
**First seen:** Module 7

### TLB (Translation Lookaside Buffer)
A small hardware cache inside the CPU that stores recent virtual→physical
translations. A TLB hit serves the translation in ~1 cycle; a TLB miss requires
a full two-level page walk (2 RAM reads). Writing CR3 flushes the entire TLB.
The `invlpg` instruction flushes a single entry (used when remapping one page).
**First seen:** Module 7

### Two-Level Page Table
The paging structure used by 32-bit x86. A Page Directory (1024 entries) holds
pointers to Page Tables (each 1024 entries). Together they map 1024 × 1024 × 4 KB
= 4 GB of virtual address space. Each level is exactly one 4 KB PMM page.
**First seen:** Module 7

### Virtual Address Space
The set of addresses that a program (or the kernel) uses in its code and data
pointers. After paging is enabled, virtual addresses are translated to physical
addresses by the MMU. Different processes have different page directories and
therefore different virtual address spaces — the same virtual address can map to
different physical pages in different processes.
**First seen:** Module 7

### `zero_page()`
A helper in `paging.c` that fills a freshly PMM-allocated page with zeros.
Necessary because the PMM does not zero pages on allocation. Unzeroed page
tables are dangerous: stale non-zero entries with the Present bit set would
cause the CPU to follow garbage physical addresses.
**First seen:** Module 7

---

---

## Module 8 additions

### Block Header
An 8-byte struct stored inline at the start of every heap region (free or used).
Contains the size of the data region and a used/free flag. Allows the allocator
to traverse all blocks by repeatedly stepping forward by `sizeof(header) + block->size`.
No external metadata array is needed — the headers live inside the heap pool itself.
**First seen:** Module 8

### Coalescing
Merging two adjacent free heap blocks into one larger free block. Prevents
fragmentation: without coalescing, repeatedly allocating and freeing small objects
creates many tiny free fragments that individually cannot satisfy larger requests.
Module 8 implements **forward coalescing** — when a block is freed, it is merged
with the immediately following free block(s).
**First seen:** Module 8

### First-Fit Allocation
An allocation strategy that scans the free list from the beginning and returns the
first block that is large enough. Simple and fast. Module 8 uses first-fit inside
`kmalloc()`. Contrast with best-fit (smallest sufficient block — less waste but
slower) and worst-fit (largest block — minimises splitting).
**First seen:** Module 8

### Fragmentation
The condition where the total free memory is large enough to satisfy an allocation,
but no single contiguous free block is. Caused by freed blocks not being merged.
Forward coalescing in `kfree()` prevents the most common form.
**First seen:** Module 8

### Free List
A data structure that tracks all free (unallocated) regions in a heap. Module 8
uses an **implicit free list**: every block (free or used) has an inline header,
and the list is traversed by walking every block in address order. An **explicit**
free list would chain only the free blocks with pointers, but requires more header
space.
**First seen:** Module 8

### `heap_init()`
Takes a number of PMM pages, allocates them contiguously, and initialises the
first free block header and end sentinel. After this call, `kmalloc` and `kfree`
are ready to use. In Module 8 we pass 16 pages (64 KB).
**First seen:** Module 8

### `heap.h` / `heap.c`
The heap allocator module. `heap.h` declares `heap_init`, `kmalloc`, `kfree`,
and four diagnostic getters. `heap.c` implements the block header struct, sentinel,
first-fit search with splitting, and forward-coalescing free.
**First seen:** Module 8

### `kfree()`
Returns a `kmalloc`'d block to the heap by marking its header free, then forward-
coalescing with any adjacent free blocks. Freeing NULL is a safe no-op.
**First seen:** Module 8

### `kmalloc()`
Allocates at least `size` bytes (rounded up to 4-byte alignment) from the kernel
heap. Returns a pointer to the data area or NULL if the heap is full. Uses first-fit
search and splits large blocks when the leftover is above the `MIN_SPLIT` threshold.
**First seen:** Module 8

### Sentinel
A special block header at the very end of the heap pool with `size=0, flags=USED`.
It is never allocated. It terminates the traversal loop in `kmalloc` and `kfree` —
reaching a zero-size used block means the end of the pool has been reached.
**First seen:** Module 8

### Splitting
When `kmalloc` finds a free block larger than needed, it inserts a new free header
partway through the block so the unused portion remains available. The threshold
for splitting is `MIN_SPLIT` (16 bytes) — smaller leftovers are given to the caller
as internal fragmentation rather than creating an unusably tiny block.
**First seen:** Module 8

---

---

## Module 9 additions

### `\b` (Backspace, ASCII 0x08)
A control character that moves the cursor one column to the left without erasing.
Added to `vga_putchar()` in Module 9. To visually erase a character, the caller
writes `'\b'`, then `' '` (space to overwrite), then `'\b'` again (move back onto
the blank). This three-step pattern matches how real terminals implement erase.
**First seen:** Module 9

### Break Code
The scancode emitted when a key is **released**. Equal to the make code with
bit 7 set (`make_code | 0x80`). The keyboard driver ignores break codes for all
keys except the shift keys (whose release must clear the `shift_held` flag).
**First seen:** Module 9

### IRQ1 (Keyboard Interrupt)
Hardware interrupt line 1, connected to the PS/2 keyboard controller. After PIC
remapping, delivers CPU vector 33. Fires once per key press and once per key
release. The handler reads port 0x60 to get the scancode and implicitly
acknowledges the interrupt.
**First seen:** Module 9

### `keyboard_getchar()`
Non-blocking function that pops one character from the keyboard ring buffer.
Returns 0 if no key is waiting. Called in the main idle loop after `hlt` returns.
**First seen:** Module 9

### `keyboard_init()`
Registers the IRQ1 handler via `irq_register(1, keyboard_irq_handler)`. After
this call (and `sti`), the keyboard is live and every keypress fires the handler.
**First seen:** Module 9

### `keyboard.h` / `keyboard.c`
The keyboard driver module. `keyboard.h` declares `keyboard_init`,
`keyboard_getchar`, and `keyboard_pending`. `keyboard.c` implements the IRQ1
handler, two scancode lookup tables (normal and shifted), shift-state tracking,
and the ring buffer push/pop.
**First seen:** Module 9

### Make Code
The scancode emitted when a key is **pressed** (bit 7 = 0). The keyboard driver
translates make codes to ASCII using a lookup table and pushes the result to the
ring buffer. Contrast with break code (key release).
**First seen:** Module 9

### Port 0x60 (Keyboard Data Port)
The I/O port that holds the most recent scancode from the PS/2 keyboard
controller. Read with `inb(0x60)` inside the IRQ1 handler. Reading this port
also clears the controller's output-full flag, lowering the IRQ1 line.
**First seen:** Module 9

### PS/2 Keyboard Controller (8042)
The hardware chip that sits between the keyboard and the CPU. It receives serial
scancode data from the keyboard, buffers one byte in its output register (port
0x60), and asserts IRQ1 to signal the CPU. Named after the original Intel 8042
microcontroller used in the IBM PC/AT.
**First seen:** Module 9

### Ring Buffer (Circular Buffer)
A fixed-size FIFO queue implemented as an array with two indices: `head` (next
write position) and `tail` (next read position). When either index reaches the
end of the array it wraps to 0. Full condition: `(head+1) % SIZE == tail`.
Lock-free for one writer + one reader — safe to share between an IRQ handler
and the main loop without disabling interrupts.
**First seen:** Module 9

### Scancode
A one-byte hardware code emitted by the PS/2 keyboard for each key event.
Encodes physical key position, not character meaning — the same physical key
produces the same scancode regardless of language or keyboard layout. The driver
translates scancodes to ASCII using a lookup table.
**First seen:** Module 9

### Scancode Set 1
The default scancode set used by QEMU and BIOS-initialised x86 keyboards. Each
key has a make code (byte with bit 7 = 0, sent on press) and a break code
(make | 0x80, sent on release). Extended keys (cursor arrows, etc.) use a
two-byte sequence starting with 0xE0.
**First seen:** Module 9

### `hlt` in idle loop
Using the `hlt` instruction inside `for(;;)` to let the CPU sleep between
interrupts. The CPU wakes on any IRQ, services the handler, then returns to
the instruction after `hlt`. This burns zero CPU cycles while waiting for
input — the correct power-efficient idle pattern.
**First seen:** Module 9 (first practical use; introduced conceptually in Module 1)

---

---

---

## Module 10 additions

### Circular Linked List
A linked list where the last node's `next` pointer points back to the first node
rather than to NULL. Used in Module 10's scheduler so that "advance to the next
process" never needs a bounds check or wrap-around — walking `current->next`
eventually returns to `current`, making the round-robin natural.
**First seen:** Module 10

### Context Switch
The act of saving one process's register state to its PCB and loading another
process's previously saved state into the CPU. On x86, the state that must be
saved is: all general-purpose registers, segment registers, EIP, CS, and EFLAGS.
In Module 10 this is done by hijacking the interrupt return path — no separate
switch instruction exists.
**First seen:** Module 10

### Fake Interrupt Frame
A manually-constructed `struct interrupt_frame` placed on a newly-created
process's stack before it has ever run. Lets the context-switch code treat a
brand-new process identically to one that was preempted mid-execution. Fields
set: `eip` = entry function, `cs` = 0x08, `eflags` = 0x202 (IF=1), all GP
registers = 0, segments = 0x10.
**First seen:** Module 10

### `isr_set_scheduler()`
A registration function in `isr.c` that stores a function pointer to the
scheduler's tick callback. Called once by `process_init()`. Allows the high-level
scheduler (`process.c`) to hook into the low-level interrupt path (`isr.c`)
without creating a circular header dependency.
**First seen:** Module 10

### Idle Process
A special kernel process (PID 1) that represents the original `kernel_main()`
execution context. It gets no separate stack allocation because it already has
the boot stack at 0x90000. When no other process is ready to run the idle
process executes `hlt` in a loop, sleeping until the next timer tick.
**First seen:** Module 10

### PCB (Process Control Block)
A data structure that stores everything needed to pause and later resume a
process: its PID, saved ESP, pointer to its stack allocation, and state
(READY/RUNNING/DEAD). In Module 10 all PCBs live in a static array
`proc_table[MAX_PROCESSES]`.
**First seen:** Module 10

### Preemption
Involuntarily stopping a running process mid-execution and switching to another,
without any cooperation from the running process. Driven by a hardware timer
(IRQ0). Contrasted with cooperative multitasking where a process must explicitly
yield the CPU.
**First seen:** Module 10

### `process_create()`
Allocates a 4 KB stack from `kmalloc`, builds a fake interrupt frame on it, and
inserts the new process into the round-robin circular list. Returns the new PID,
or 0 on failure.
**First seen:** Module 10

### `process_init()`
Converts the current `kernel_main` context into the idle process (PID 1), zeroes
the PCB table, and registers `scheduler_tick` as the IRQ0 callback via
`isr_set_scheduler()`.
**First seen:** Module 10

### Round-Robin Scheduling
A scheduling policy that gives each process an equal, fixed time slice (one timer
tick here) and cycles through all ready processes in order. Simple, fair, and
starvation-free as long as all processes eventually become READY. Module 10's
`scheduler_tick()` advances `current` to `current->next` on every IRQ0.
**First seen:** Module 10

### Scheduler
The kernel subsystem that decides which process runs next. Module 10's scheduler
is a round-robin: on every timer tick it saves the current process's ESP, picks
the next READY process from the circular list, and returns its saved ESP to the
ISR stub.
**First seen:** Module 10

### `scheduler_tick()`
Called by `interrupt_handler()` on every IRQ0. Receives the current process's
saved ESP, stores it in the PCB, walks the circular list to the next READY
process, marks it RUNNING, and returns its ESP. The ISR stub uses this returned
value as the new stack pointer before `pop`/`iret`.
**First seen:** Module 10

### Saved ESP
The value of ESP at the moment a process was last preempted, stored in its PCB.
Because `isr_common_stub` has already pushed a full `interrupt_frame` onto the
process's stack at this point, `saved_esp` points to the bottom (lowest address)
of that frame. Restoring ESP to this value and executing the stub's `pop`/`iret`
sequence resumes the process exactly where it was interrupted.
**First seen:** Module 10

### Time Slice (Quantum)
The maximum amount of CPU time a process is allowed to run before the scheduler
preempts it. In Module 10 the quantum is one PIT tick (≈55 ms at 18.2 Hz). With
3 processes plus the idle loop in the round-robin, each process gets ≈1/4 of the
CPU — roughly 4–5 visible updates per second in the demo.
**First seen:** Module 10

### `proc_state_t`
An enum (`PROC_READY`, `PROC_RUNNING`, `PROC_DEAD`) stored in every PCB. The
scheduler skips processes that are not READY. DEAD slots can be reclaimed by
`process_create()`. RUNNING means the process currently holds the CPU.
**First seen:** Module 10

---

---

---

## Module 11 additions

### `int 0x80` (Software Interrupt / Syscall Gate)
The x86 instruction used to invoke a kernel system call. Triggers the CPU to
look up IDT vector 128 (0x80), check the gate's DPL, push EFLAGS/CS/EIP,
clear IF, load the ring-0 code segment, and jump to the gate handler. The
mechanism is identical to a hardware interrupt except it is triggered
deliberately by user code rather than by a peripheral.
**First seen:** Module 11

### DPL (Descriptor Privilege Level) in an IDT Gate
The minimum ring level a caller must be in to execute `int N` without a #GP.
Setting DPL=3 in the syscall gate means ring-3 user code can call `int 0x80`.
Setting DPL=0 (exception gates) means only ring-0 code can call it — a ring-3
`int 0x80` with DPL=0 would trigger a #GP instead of the intended handler.
**First seen:** Module 11

### `IDT_GATE_SYSCALL` (0xEE)
The type_attr byte for an IDT gate callable from ring 3. Bit pattern:
`1 11 0 1110` — Present=1, DPL=11 (3), type=1110 (32-bit interrupt gate).
Compared to `IDT_GATE_INTERRUPT` (0x8E) where DPL=00 (0).
**First seen:** Module 11

### `isr128` / vector 128
The ISR stub in `isr.asm` for IDT vector 0x80 (decimal 128). Added with
`ISR_NOERRCODE 128` — identical in structure to all other stubs. Jumps to
`isr_common_stub` which saves registers and calls `interrupt_handler`.
**First seen:** Module 11

### Privilege Ring (Ring 0 / Ring 3)
x86 protection levels. Ring 0 = kernel (full access). Ring 3 = user (restricted).
A system call is the controlled crossing from ring 3 to ring 0 and back. The CPU
enforces the crossing: user code cannot jump directly into kernel code; it must
go through a gate descriptor in the IDT or GDT.
**First seen:** Module 2 (concept), Module 11 (first practical gate crossing)

### `syscall_dispatch()`
The C function registered as the `int 0x80` handler. Reads `frame->eax` for
the syscall number, `frame->ebx/ecx/edx` for arguments, dispatches to the
appropriate implementation, writes the return value into `frame->eax`, and
returns an ESP (usually unchanged; new process's ESP for SYS_EXIT).
**First seen:** Module 11

### `syscall.h` / `syscall.c`
The system call module. `syscall.h` defines the four syscall numbers
(SYS_EXIT, SYS_WRITE, SYS_GETPID, SYS_YIELD), the `syscall()` inline
assembly helper, convenience wrappers (`sys_write`, `sys_exit`, etc.), and
the kernel-side `syscall_init()` / `syscall_dispatch()` declarations.
`syscall.c` implements the dispatcher and each syscall handler.
**First seen:** Module 11

### `SYS_WRITE` (syscall 2)
Writes `len` bytes from `buf` to file descriptor `fd`. For fd=1 (stdout),
calls `vga_putchar()` for each byte. Returns the number of bytes written,
or a negative error code. The buf pointer is validated only minimally here
— full validation requires ring-3 page permission checking (future module).
**First seen:** Module 11

### `SYS_EXIT` (syscall 1)
Terminates the current process. Unlike all other syscalls, it returns a
DIFFERENT process's saved ESP to `isr_common_stub` so the stub restores
that process instead of the now-dead one. Implemented via `process_exit()`.
**First seen:** Module 11

### `SYS_GETPID` (syscall 3)
Returns the PID of the currently running process. The simplest possible
syscall — demonstrates that reading kernel state and returning it to user
code through `frame->eax` works correctly.
**First seen:** Module 11

### `SYS_YIELD` (syscall 4)
Voluntarily surrenders the current process's time slice by calling
`scheduler_tick(current_esp)`. The calling process is not marked dead — it
remains READY and will be scheduled again on the next round. Demonstrates
cooperative multitasking layered on top of preemptive multitasking.
**First seen:** Module 11

### Syscall Return Value via `frame->eax`
The mechanism for passing a value from the kernel back to the user after an
`int 0x80`. Because `isr_common_stub` saved EAX into `frame->eax` before
calling the handler, writing a new value there before returning causes `popa`
to restore it into the caller's EAX register. No extra ABI needed.
**First seen:** Module 11

### System Call (Syscall)
A request from user-mode code to the kernel for a privileged service. On x86-32,
implemented as `int 0x80` which transfers control to the kernel through a
controlled IDT gate. The kernel validates arguments, performs the service,
and returns the result in EAX. The boundary enforces security: user code cannot
bypass argument validation or call internal kernel functions directly.
**First seen:** Module 11

### User Mode / User Space
Code running in ring 3. Has no direct access to kernel memory, cannot execute
privileged instructions, and can only call the kernel through defined system
call gates. All user-visible OS features (file I/O, process management,
network) are accessed via syscalls. In Module 11 all code is still in ring 0,
but the syscall interface is identical to what ring-3 code would use.
**First seen:** Module 11 (introduced; ring-3 enforcement comes with a future module)

---

---

---

## Module 12 additions

### ATA (Advanced Technology Attachment)
The standard interface that connects hard disks, SSDs, and optical drives to
the motherboard.  Also called IDE (Integrated Drive Electronics).  The primary
ATA bus uses I/O ports 0x1F0-0x1F7.  Our ATA PIO driver speaks directly to
these ports with `inb`/`outb`/`inw`/`outw` instructions — no BIOS, no
driver library.
**First seen:** Module 12

### ATA PIO (Programmed I/O)
The mode where the CPU explicitly reads and writes every byte of a disk
transfer using I/O port instructions.  Contrast with DMA where the disk
controller writes directly to RAM.  PIO is slower for large transfers but
requires no DMA controller programming.  QEMU supports PIO on the emulated
IDE controller with no special configuration.
**First seen:** Module 12

### BobFS
The custom flat file system designed for BobOS.  Layout: sector 0 =
superblock, sector 1 = directory (16 entries × 32 bytes), sectors 2+ = file
data.  Files are stored contiguously; allocation is a bump pointer; deletion
marks the directory entry free but does not reclaim data sectors.
**First seen:** Module 12

### Bump Pointer Allocator
An allocation strategy that maintains a single pointer (`next_sector`) to the
first free unit.  Each allocation advances the pointer by the requested size.
Freeing is not supported (or in BobFS, leaves a gap).  O(1) allocation, zero
per-block metadata overhead.  Used in BobFS for data sector allocation.
**First seen:** Module 12

### `disk.c` / `disk.h`
The ATA PIO driver.  `disk.h` declares `disk_init()`, `disk_present()`,
`disk_read_sector()`, and `disk_write_sector()`.  `disk.c` implements the
LBA28 read and write sequences using the ATA port map, with local `ata_inw`
and `ata_outw` helpers for 16-bit data port access.
**First seen:** Module 12

### `disk.img`
A raw binary file treated as a virtual hard disk by QEMU (`-hda disk.img`).
Created with `dd if=/dev/zero bs=512 count=4096` — 4096 zero-filled sectors
(2 MB).  BobFS auto-formats it on first boot.  Unlike `os.img` (the floppy),
`disk.img` persists across QEMU runs, providing real data persistence.
**First seen:** Module 12

### `fs.c` / `fs.h`
The BobFS implementation.  `fs.h` defines the on-disk structures
(`struct fs_super`, `struct fs_dirent`) and the public API (`fs_init`,
`fs_create`, `fs_read`, `fs_delete`, `fs_list`, `fs_num_files`).  `fs.c`
maintains in-memory copies of the superblock and directory, and flushes them
back to disk after every mutation.
**First seen:** Module 12

### `fs_init()`
Reads the superblock from sector 0 and checks the magic number.  If the magic
does not match `FS_MAGIC` (0x424F5342), the disk is blank and BobFS writes a
fresh superblock and empty directory.  Returns 0 (existing FS), 1 (formatted
fresh), or -1 (no disk).
**First seen:** Module 12

### LBA (Logical Block Addressing)
A disk addressing scheme that treats the disk as a flat array of sectors
numbered from 0.  Replaces the older CHS (Cylinder/Head/Sector) scheme.
LBA28 uses a 28-bit address (bits 24-27 go in the DRIVE/HEAD register at
0x1F6) and supports disks up to 137 GB.
**First seen:** Module 12

### Superblock
The first sector (sector 0) of a BobFS volume.  Contains the magic number
(`FS_MAGIC = 0x424F5342`), version, number of live files, and the next free
data sector pointer.  `fs_init()` reads the superblock to determine whether
the disk is blank or has an existing filesystem.
**First seen:** Module 12

### Directory Entry (`struct fs_dirent`)
A 32-byte record in sector 1 of a BobFS volume.  Fields: `name[20]` (filename,
null-terminated), `size` (bytes), `start_sector` (first data sector), `flags`
(0=free, 1=present).  Sixteen entries fit in one 512-byte sector.
**First seen:** Module 12

### `CMD_FLUSH` (0xE7)
The ATA FLUSH CACHE command.  Written to the ATA command port (0x1F7) after a
write sequence to force the drive's internal write buffer to permanent storage.
Without this, the drive may acknowledge the write but not have committed it to
the platter/flash yet.
**First seen:** Module 12

---

---

## Module 13 additions

### Shell
A command-line interpreter that reads user input, parses it into a command and
arguments, and dispatches to the appropriate kernel function.  In Module 13 the
shell is a kernel process (PID 2) spawned by `kernel_main`.  It supports:
`help`, `clear`, `echo`, `pid`, `ls`, `cat`, `write`, `rm`, `reboot`.
**First seen:** Module 13

### `shell.c` / `shell.h`
The shell module.  `shell.h` declares `shell_main()`, the process entry point.
`shell.c` implements the read-echo-dispatch loop, the manual command parser,
and one handler function per command.  Two static buffers (`line_buf`,
`read_buf`) keep large data off the process stack.
**First seen:** Module 13

### `shell_main()`
The shell process entry point, passed to `process_create()`.  Loops forever:
calls `sys_yield()` to give other processes a turn, checks `keyboard_getchar()`
for a character, echoes it, and dispatches on Enter.
**First seen:** Module 13

### Idle Process
The process that runs when no other process is READY.  In Module 13,
`kernel_main` becomes the idle process after spawning the shell: it executes
`hlt` in a tight loop, waking on every timer tick only to be immediately
preempted back to the shell.  PID 1.
**First seen:** Module 10 (concept), Module 13 (first real use)

### Command Parser
The piece of `shell.c` that splits a line buffer into a command token and an
argument string.  Implemented as 12 lines of C: scan for the first space,
null-terminate there, skip leading spaces in the remainder.  No `strtok`,
no standard library.
**First seen:** Module 13

### Cooperative Multitasking (in shell context)
The shell calls `sys_yield()` on every poll iteration, voluntarily surrendering
its time slice.  Combined with the preemptive scheduler (IRQ0), this means the
shell gives up the CPU both voluntarily (yield) and involuntarily (timer).
Keeps the idle process alive and CPU usage low between keystrokes.
**First seen:** Module 13

### Triple-Fault Reboot
A hard reset technique: load a zero-size IDT (`lidt (0)`) then trigger any
CPU exception.  The CPU cannot find a handler, fires #DF, cannot find that
handler, and triple-faults — resetting the machine.  Used by the `reboot`
shell command.  Works reliably in QEMU; not recommended on real hardware.
**First seen:** Module 13

---

---

---

## Module 14 additions

### Ring 3 (User Mode)
CPU privilege level 3 — the most restricted level.  Code at ring 3 cannot
execute privileged instructions (`cli`, `hlt`, `lgdt`, `ltr`, `in`, `out`,
etc.) or write to control registers.  The CPU enforces this in hardware: any
attempt raises a General Protection Fault (#GP, vector 13).  All user
applications on a real OS run at ring 3.  The kernel runs at ring 0.
**First seen:** Module 14

### TSS (Task State Segment)
A 104-byte structure required by the x86 CPU for privilege-level transitions.
The most important field is `esp0`: when an interrupt or exception fires while
code runs at ring 3, the CPU automatically loads ESP from `TSS.esp0` and SS
from `TSS.ss0` before pushing any interrupt frame.  One TSS per CPU (not per
process); `tss_set_kernel_stack()` updates `esp0` before each ring-3 process
is scheduled.
**First seen:** Module 14

### `tss.c` / `tss.h`
The TSS module.  `tss.h` defines `struct tss_entry` (all 26 fields, packed)
and declares `tss_init()` and `tss_set_kernel_stack()`.  `tss.c` holds the
single static TSS, zeroes it, sets `esp0`/`ss0`/`iomap_base`, calls
`gdt_install_tss()` to install GDT entry 5, and executes `ltr` to load the
Task Register.
**First seen:** Module 14

### TR (Task Register)
A hidden CPU register that holds a GDT selector pointing at the active TSS.
Loaded by the `ltr` instruction (privileged; only ring 0 can execute it).
After `ltr 0x28`, the CPU reads GDT[5] to find the TSS base address and
consults `TSS.esp0` on every ring-3 interrupt.
**First seen:** Module 14

### `ltr` (Load Task Register)
An x86 privileged instruction that loads the Task Register with a GDT
selector.  Must be called exactly once during kernel init after the TSS
descriptor is installed in the GDT.  Analogous to `lgdt` for the GDT and
`lidt` for the IDT.
**First seen:** Module 14

### `GDT_SEL_USER_CODE` (0x1B)
Segment selector for the ring-3 code segment: GDT index 3, RPL=3
(bits 1-0 = 11b).  Used as the CS value in a ring-3 process's fake interrupt
frame.  When `iret` pops this selector and sees RPL=3 > CPL=0, it also pops
the user ESP and SS, completing the ring transition.
**First seen:** Module 14

### `GDT_SEL_USER_DATA` (0x23)
Segment selector for the ring-3 data segment: GDT index 4, RPL=3.  Used as
DS/ES/FS/GS and SS for a ring-3 process.  DPL=3 means it can be loaded from
ring 3 (CPU check: DPL >= max(CPL, RPL) → 3 >= 3).
**First seen:** Module 14

### `gdt_install_tss(base, limit)`
Public function added to `gdt.c` in Module 14.  Fills GDT entry 5 with the
TSS descriptor using access byte 0x89 (System, DPL=0, Type=9 — 32-bit TSS
available) and flags=0 (limit in bytes, not 4KB pages).  Called once by
`tss_init()`.
**First seen:** Module 14

### `process_create_user(entry)`
Creates a ring-3 process.  Allocates a kernel stack and a user stack from the
heap.  Calls `build_frame_ring3()` to build a 19-word fake interrupt frame on
the kernel stack (17 standard words + user_esp + user_ss at the top).  Sets
`kernel_stack_top` in the PCB so the scheduler can update TSS.esp0 before
running this process.
**First seen:** Module 14

### Kernel Stack (per ring-3 process)
A 4 KB `kmalloc` allocation that serves as the stack for interrupt and syscall
handling when the process runs in ring 3.  Its top address is stored in
`PCB.kernel_stack_top` and written to `TSS.esp0` before the process is
scheduled.  Separate from the user stack.
**First seen:** Module 14

### User Stack (per ring-3 process)
A 4 KB `kmalloc` allocation used as the process's own stack at ring 3.  Its
top address appears as `user_esp` in the ring-3 fake frame.  The CPU restores
this value (and `user_ss`) from the frame on `iret` to ring 3.
**First seen:** Module 14

### Ring-3 Fake Frame
The 19-word (76-byte) fake interrupt frame built by `build_frame_ring3()` for
a new ring-3 process.  Identical to the ring-0 fake frame except for two
additional words at the top: `user_esp` and `user_ss`.  When `iret` pops
`cs=0x1B` (RPL=3), it automatically pops those two words, restoring the user
stack and transitioning the CPU to ring 3.
**First seen:** Module 14

### I/O Permission Bitmap (IOPM)
A bitmap in the TSS (pointed to by `TSS.iomap_base`) that controls which I/O
ports ring-3 code may access.  If `iomap_base` equals `sizeof(tss)` (pointing
past the end of the TSS), all ports are forbidden to ring-3.  We use this
default to prevent user code from directly talking to hardware.
**First seen:** Module 14

### `user_prog.c` / `user_prog.h`
The ring-3 demo program.  `user_main()` is the entry function: it calls
`sys_getpid()`, `sys_yield()` three times, several `sys_write()` calls, and
finally `sys_exit(0)`.  All of these use `int 0x80` — the only legal path
from ring 3 to the kernel.
**First seen:** Module 14

---

---

---

---

## Module 15 additions

### Per-Process Address Space
A private virtual address space given to each ring-3 process by loading a
distinct page directory into CR3.  Each process sees the same virtual addresses
but backed by potentially different (or absent) physical pages.  Two processes
that hold different CR3 values cannot read each other's memory, even if they
both try to access the same virtual address.
**First seen:** Module 15

### `paging_new_address_space()`
Allocates a fresh page directory and page table from the PMM, copies the
kernel's 4MB identity-map PTEs into the new PT (with `PAGE_USER` stripped),
and installs the new PT in PDE[0] (supervisor-only).  Returns the physical
address of the new PD for storage in `PCB.cr3`.
**First seen:** Module 15

### `paging_set_user_page(cr3, virt)`
Marks one 4KB page user-accessible in a process's page directory.  Sets
`PAGE_USER` in both the PDE (directory level) and the PTE (page level), then
calls `invlpg` to flush that TLB entry.  Must be called for every page a ring-3
process needs to access: its code pages and its stack pages.
**First seen:** Module 15

### `paging_switch(cr3)`
Writes `cr3` into the CR3 register via `mov %0, %%cr3`.  Causes the CPU to
flush the TLB and begin translating all subsequent memory accesses through the
new page directory.  Called by the scheduler on every context switch.
**First seen:** Module 15

### U/S Bit (User/Supervisor Bit, PAGE_USER)
Bit 2 of every PDE and PTE.  When 0 (supervisor), only ring-0 code can access
the page; ring-3 access triggers #PF.  When 1 (user), ring-3 code may access
the page subject to the W/R bit.  Both the PDE and the PTE must have U/S=1 for
a user-mode access to succeed — the CPU ANDs the two bits.
**First seen:** Module 7 (constant defined), Module 15 (first deliberate use for isolation)

### `user_code_start` / `user_code_end` (linker symbols)
Exported symbols from `linker.ld` that bound the `.user_code` output section
containing all of `user_prog.o`.  In C, referenced as `extern char
user_code_start; extern char user_code_end;`.  Taking their ADDRESS (not value)
gives the first and one-past-last byte of the user code region.
`process_create_user()` loops from `&user_code_start` to `&user_code_end` in
4KB steps, calling `paging_set_user_page()` for each page.
**First seen:** Module 15

### `.user_code` (linker section)
A custom output section in `linker.ld` that contains all sections from
`user_prog.o` (`.text`, `.rodata`, `.data`, `.bss`), 4KB-aligned at both ends.
`EXCLUDE_FILE(*user_prog.o)` in the kernel `.text` and `.data` rules ensures
user code is completely absent from kernel sections, giving a clean page range.
**First seen:** Module 15

### `EXCLUDE_FILE(*user_prog.o)` (linker script directive)
A linker script operator that removes all contributions from `user_prog.o`
within a particular input section pattern.  Used to prevent user code from
being linked into the kernel `.text` section.  This produces two completely
separate regions: the kernel text (supervisor-only pages) and the user code
region (pages we selectively open with PAGE_USER).
**First seen:** Module 15

### `PCB.cr3`
The new field added to `struct process` in Module 15.  Stores the physical
address of the page directory belonging to this process.  Initialized by
`process_create_user()` (from `paging_new_address_space()`) and by
`process_init()` / `process_create()` (from `paging_kernel_cr3()`).  The
scheduler reads this field to load CR3 on every context switch.
**First seen:** Module 15

### `isr_set_user_fault_handler(fn)`
Registers a callback for CPU exceptions (vectors 0-31) that fire while code
runs at ring 3 (`frame->cs & 3 == 3`).  The callback receives the current
kernel-side ESP and must return the saved ESP of the next ready process.
`process_exit()` satisfies this contract and is registered by `process_init()`.
Ring-0 exceptions still call `kernel_panic()`.
**First seen:** Module 15

### Graceful Process Kill (on #PF)
When the user_fault_hook fires for a ring-3 #PF, the kernel:
1. Reads CR2 (the faulting virtual address).
2. Prints the address, error code, and EIP.
3. Calls `process_exit()` to mark the process DEAD and switch to the next ready
   process.
The system continues running; no kernel panic occurs.  This is how real OSes
handle segmentation faults.
**First seen:** Module 15

### CR2 Register
A CPU control register that holds the virtual address that caused the most
recent Page Fault (#PF).  Set automatically by the CPU before the #PF handler
is entered.  Read with `mov %%cr2, %0` in inline assembly inside the ISR.
**First seen:** Module 15

### `invlpg` (Invalidate Page)
An x86 instruction that flushes a single TLB entry for one virtual address.
Used in `paging_set_user_page()` after modifying a PTE so the CPU immediately
sees the updated permissions.  More precise than a full CR3 write (which flushes
all entries) — only invalidates the one page being changed.
**First seen:** Module 7 (instruction exists), Module 15 (first explicit use)

---

*Last updated: Module 15 (Per-Process Address Spaces)*
