# Component 2: Kernel Entry — Deep Dive

---

## 1. Where We Left Off and What the Goal Is

At the end of Component 1, our bootloader:
- Ran at address 0x7C00 in 16-bit Real Mode
- Printed a message using BIOS interrupts
- Halted

That was the absolute minimum — proof the CPU handed control to our code. But a real
OS needs to run C code, address gigabytes of RAM, and protect memory from runaway
programs. None of that is possible in Real Mode.

Component 2 does three things in sequence:
1. **Load the kernel binary from disk into RAM** using BIOS disk services
2. **Switch the CPU from 16-bit Real Mode to 32-bit Protected Mode**
3. **Jump into a C function** — `kernel_main()` — running on bare metal

Each of these is a major milestone. Let's understand every part from scratch.

---

## 2. What is a Kernel?

The **kernel** is the core program of an operating system. It is the one program that:
- Runs with complete control over the CPU and RAM
- Manages every piece of hardware (keyboard, screen, disk, network)
- Decides which programs get CPU time and when
- Enforces the rules that keep programs from interfering with each other

Everything you use on a computer — web browsers, file managers, terminals — are
**user space** programs. They do not control hardware directly. They ask the kernel
to do things on their behalf. The kernel is the trusted intermediary between software
and hardware.

Right now our kernel is tiny — it just prints to screen to prove it ran. Over the
next components we will grow it into something capable of running multiple processes,
managing files, and accepting user input.

### Where the kernel lives on disk

Our bootloader occupies **sector 1** of our virtual disk (the first 512 bytes). The
kernel is a separate binary file. We place it starting at **sector 2** — immediately
after the bootloader. The bootloader's job is to read those sectors into RAM before
we switch CPU modes.

---

## 3. Why We Must Leave Real Mode

Real Mode has three fatal problems for a kernel:

| Problem | Real Mode | Protected Mode |
|---------|-----------|----------------|
| Maximum RAM | 1 MB | 4 GB |
| Memory protection | None — any code touches any address | Enforced by CPU — programs can't touch kernel memory |
| Register width | 16-bit (max value 65535) | 32-bit (max value ~4 billion) |

Writing a real OS in Real Mode is impossible:
- Modern programs need hundreds of megabytes of RAM. 1MB is not enough.
- Without memory protection, a buggy app can overwrite the kernel itself and crash everything.
- 16-bit math is too limited for meaningful computation.

So we must switch to Protected Mode as quickly as possible. But before we do, we must
finish all BIOS operations — because BIOS does not work in Protected Mode.

### Why BIOS dies in Protected Mode

The BIOS is itself a Real Mode program. Its interrupt handlers (`INT 0x10`, `INT 0x13`
etc.) were written for 16-bit mode. The moment we switch the CPU to Protected Mode,
the CPU's rules for how memory segments work change completely. The BIOS routines
would crash instantly if called.

This is why our bootloader must:
1. Use BIOS to load the kernel from disk while still in Real Mode
2. Then switch to Protected Mode
3. Never call BIOS again — we write our own drivers instead

---

## 4. Loading the Kernel from Disk (BIOS INT 0x13)

Before switching modes, the bootloader reads the kernel binary into RAM using
**BIOS Interrupt 0x13** — the disk service interrupt.

### CHS Addressing

Early hard drives were physical spinning platters. The BIOS talked to them using a
three-part address called **CHS — Cylinder, Head, Sector**:

- **Cylinder** — which circular track on the platter (like a ring on a dartboard)
- **Head** — which read/write head (which side of which platter)
- **Sector** — which 512-byte slice within a track (sectors are numbered from 1, not 0)

This physical model is long obsolete, but BIOS kept the CHS interface for decades for
compatibility. Our virtual floppy disk in QEMU also uses it.

For a floppy disk:
- Cylinder 0, Head 0, Sector 1 = the bootloader (first 512 bytes)
- Cylinder 0, Head 0, Sector 2 = where the kernel starts

### Reading with INT 0x13

```asm
mov bx, 0x1000   ; ES:BX = destination address (where to put the data in RAM)
mov ah, 0x02     ; function 0x02 = read sectors
mov al, 15       ; read 15 sectors (7680 bytes — more than enough for our kernel)
mov ch, 0        ; cylinder 0
mov cl, 2        ; sector 2 (first sector after the bootloader)
mov dh, 0        ; head 0
mov dl, 0        ; drive 0 (floppy A)
int 0x13         ; call BIOS
jc disk_error    ; if Carry Flag set, an error occurred
```

The BIOS reads 15 × 512 = 7680 bytes from the disk and copies them to address 0x1000
in RAM. Our kernel binary lands there, ready to be jumped to.

### Why load at 0x1000?

Memory in the first 1MB is not all available — much of it is reserved:

```
0x00000 - 0x003FF  Interrupt Vector Table (IVT) — BIOS uses this
0x00400 - 0x004FF  BIOS Data Area
0x00500 - 0x07BFF  Free (we could use this)
0x07C00 - 0x07DFF  Our bootloader (512 bytes)
0x07E00 - 0x9FFFF  Free
0xA0000 - 0xBFFFF  Video memory (VGA buffer at 0xB8000)
0xC0000 - 0xFFFFF  BIOS ROM
```

We chose **0x1000** (4096) because it's:
- Well above the IVT and BIOS data area
- Well below the bootloader at 0x7C00 (no overlap)
- A clean round number, easy to reason about

---

## 5. Protected Mode — What It Is and Where It Came From

**Protected Mode** was introduced by Intel with the **80286** processor in 1982 and
fully completed on the **80386** in 1985.

The 8086 (1978) had no memory protection. Any program could read or write any memory
address, including the OS itself. This made multitasking dangerous — one bad program
could corrupt everything. Intel designed Protected Mode to solve this.

### What Protected Mode adds

**1. Memory Protection via Segments**
Each memory segment has an access permission. The CPU checks every memory access
against these permissions. If a program tries to write to a read-only segment, or
access memory outside its permitted range, the CPU fires an exception — the kernel
catches it and terminates the offending program.

**2. Privilege Rings**
The CPU has four privilege levels called "rings":

```
┌─────────────────────────────────────┐
│  Ring 0 — Kernel (most privileged)  │  ← Our kernel runs here
│  ┌─────────────────────────────┐    │
│  │  Ring 1 — (rarely used)    │    │
│  │  ┌──────────────────────┐  │    │
│  │  │  Ring 2 (rarely used)│  │    │
│  │  │  ┌────────────────┐  │  │    │
│  │  │  │  Ring 3 — User │  │  │    │  ← User programs run here
│  │  │  └────────────────┘  │  │    │
│  │  └──────────────────────┘  │    │
│  └─────────────────────────────┘    │
└─────────────────────────────────────┘
```

Code running in Ring 3 (user programs) cannot:
- Execute privileged CPU instructions (like `hlt` or `cli`)
- Access Ring 0 memory
- Disable interrupts

If user code tries any of these, the CPU fires a **General Protection Fault** (#GP)
exception. The kernel's exception handler catches it. This is the hardware mechanism
that makes user programs safe to run — they physically cannot corrupt the kernel.

Most OSes (Linux, Windows) only use Ring 0 and Ring 3. Rings 1 and 2 exist but are
practically never used.

**3. 32-bit Registers and 4GB Addressing**
In Protected Mode, all general-purpose registers expand to 32 bits:

```
Real Mode:  AX  BX  CX  DX  SI  DI  SP  BP   (16-bit, max 65535)
Prot. Mode: EAX EBX ECX EDX ESI EDI ESP EBP  (32-bit, max 4,294,967,295)
```

The `E` prefix stands for **Extended**. With 32-bit addresses, we can reach up to
2³² = 4,294,967,296 bytes = **4 GB of RAM**.

Addressing also becomes simpler. In Real Mode we had the awkward `Segment × 16 +
Offset` formula. In Protected Mode with a flat memory model (see GDT section below),
addresses are straightforward linear values — address 0x200000 means physical byte
2,097,152 in RAM. No math needed.

---

## 6. The GDT — Global Descriptor Table

To enter Protected Mode, the CPU requires a **GDT** to be set up in RAM first.
This is not optional — the CPU will not let you switch modes without it.

### Why the GDT exists

When Intel designed Protected Mode, they needed a data structure that defined memory
regions and their access rules. Their solution: a table of **descriptors**, where each
descriptor is an 8-byte record describing one segment of memory.

The CPU is told where this table lives via a special register called the **GDTR**
(GDT Register). The CPU consults the GDT on every memory access to enforce protection.

### The 8-byte descriptor format

Each GDT entry (descriptor) is 8 bytes. The layout looks like this:

```
Byte 7    Byte 6    Byte 5    Bytes 4-2   Bytes 1-0
┌────────┬──────────┬────────┬──────────┬──────────┐
│Base    │Flags +   │Access  │Base      │Limit     │
│31-24   │Limit     │Byte    │23-0      │15-0      │
│        │19-16     │        │          │          │
└────────┴──────────┴────────┴──────────┴──────────┘
```

Notice the Base address and Limit are **split across non-contiguous bytes**. This
scrambled layout is a legacy of Intel needing the 386 (32-bit) to be backwards
compatible with 286 (24-bit) descriptor formats. We simply have to live with it.

**The Access Byte** (Byte 5) controls permissions:
```
Bit 7: Present (P)     — 1 = this segment is valid and in memory
Bit 6: DPL high        — Privilege ring (bits 6-5 together: 00=ring0, 11=ring3)
Bit 5: DPL low         ┘
Bit 4: Descriptor Type — 1 = code or data segment
Bit 3: Executable (E)  — 1 = code segment (can execute), 0 = data segment
Bit 2: Direction (D)   — for data: 0 = grows up; for code: 0 = non-conforming
Bit 1: RW              — for code: 1 = readable; for data: 1 = writable
Bit 0: Accessed (A)    — CPU sets this to 1 when segment is used; init to 0
```

**The Flags nibble** (upper 4 bits of Byte 6) controls the size and granularity:
```
Bit 3: Granularity (G) — 0 = limit in bytes, 1 = limit in 4KB pages
Bit 2: Size (DB)       — 0 = 16-bit segment, 1 = 32-bit segment
Bit 1: Long (L)        — 1 = 64-bit mode (we use 0)
Bit 0: Reserved        — always 0
```

### Our three GDT entries

**Entry 0: The Null Descriptor**
The CPU specification requires the first GDT entry to be all zeros. If the CPU
ever loads a segment register with selector 0 by accident, it immediately fires an
exception rather than accessing random memory. It's a hardware safety net.

```asm
gdt_null:
    dd 0x00000000    ; 8 bytes of zero
    dd 0x00000000
```

**Entry 1: Code Segment**
- Base = 0x00000000 (starts at address 0)
- Limit = 0xFFFFF with G=1 (4KB granularity → 0xFFFFF × 4096 = 4GB)
- Access = 10011010b = present, ring 0, code/data type, executable, readable
- Flags = 1100b = 4KB granularity, 32-bit

This segment covers all 4GB of memory. Our kernel code runs in this segment.

**Entry 2: Data Segment**
- Same base and limit as code (covers all 4GB)
- Access = 10010010b = same as code but bit 3 = 0 (data, not executable) and bit 1 = 1 (writable)

Covering all of memory 0→4GB with both code and data segments is called a
**flat memory model**. Addresses in our code directly equal physical RAM addresses.
We use paging (Component 7) for fine-grained memory management later.

### Segment Selectors

When we load a segment register (CS, DS, SS etc.) in Protected Mode, we don't load
an address — we load a **selector**. A selector is an index into the GDT multiplied
by 8 (since each entry is 8 bytes), plus a privilege level in the lower 2 bits.

```
Null descriptor:  index 0 × 8 = 0x00
Code segment:     index 1 × 8 = 0x08
Data segment:     index 2 × 8 = 0x10
```

So when we do `mov ax, 0x10` and `mov ds, ax`, we're telling the CPU: "the data
segment is described by GDT entry at offset 0x10 (entry 2)."

### Telling the CPU where the GDT is: LGDT

```asm
lgdt [gdt_descriptor]
```

`lgdt` loads a 6-byte structure into the hidden GDTR register inside the CPU:
- Bytes 0-1: size of the GDT in bytes, minus 1 (a hardware quirk)
- Bytes 2-5: 32-bit physical address of the GDT in RAM

```asm
gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; size minus 1
    dd gdt_start                  ; address
```

---

## 7. Switching to Protected Mode Step by Step

### Step 1: Load the GDT
```asm
lgdt [gdt_descriptor]
```
Must happen before the switch. The CPU will need it immediately.

### Step 2: Set the PE bit in CR0

**CR0** is one of the CPU's **Control Registers** — special registers that control
fundamental CPU behaviour, not general-purpose data. Bit 0 of CR0 is the **PE
(Protection Enable)** bit. Setting it switches the CPU to Protected Mode.

CR0 cannot be written with an immediate value — you must go through a general register:

```asm
mov eax, cr0      ; read CR0 into EAX
or  eax, 0x1      ; set bit 0 (PE)
mov cr0, eax      ; write back — CPU is NOW in Protected Mode
```

The instant that last instruction completes, the CPU is in Protected Mode. But we're
not done — we must immediately do a far jump.

### Step 3: The Far Jump (pipeline flush)

```asm
jmp 0x08:init_protected_mode
```

Why is this required? Modern CPUs **pre-fetch and pipeline** instructions — they read
ahead several instructions before executing them. When we set the PE bit, there may
be old 16-bit Real Mode instructions already in the pipeline. If the CPU executed
them in Protected Mode it would misinterpret the opcodes and crash.

A **far jump** (one that changes the CS register) forces the CPU to:
1. Discard everything in the instruction pipeline
2. Reload the CS register with our new Protected Mode code segment selector (0x08)
3. Begin fetching instructions fresh from the new address

`0x08` is the code segment selector (GDT entry 1). `init_protected_mode` is the label
where our 32-bit code begins. After this jump, we are fully, correctly in Protected Mode.

### Step 4: Reload all segment registers

```asm
mov ax, 0x10    ; data segment selector
mov ds, ax
mov ss, ax
mov es, ax
mov fs, ax
mov gs, ax
```

The far jump updated CS. But DS, SS, ES, FS, GS still hold their Real Mode values.
We must reload them all with the Protected Mode data segment selector (0x10). Until
we do this, any data access uses the wrong segment descriptor and will crash.

### Step 5: Set up a new stack

```asm
mov ebp, 0x90000
mov esp, ebp
```

We move the stack to address 0x90000 — well above our kernel at 0x1000 and below
the 1MB mark. The stack must be in a clean, unused region of RAM. Any `push`, `pop`,
`call`, or `ret` instruction uses ESP as the stack pointer, so this must be valid.

### Step 6: Jump to the kernel

```asm
call KERNEL_OFFSET    ; = call 0x1000
```

This jumps to the first byte of the kernel binary we loaded from disk. That byte
is the first instruction of `kernel_entry.asm`, which then calls `kernel_main()` in C.

---

## 8. The Kernel Entry Stub (kernel_entry.asm)

Why do we need this tiny assembly file at all? Why not jump directly to the C function?

**C has assumptions.** The C compiler generates code that assumes:
- A valid stack exists (we set that up in the bootloader above)
- The code is at a known address (the linker handles this)
- Segment registers point to valid descriptors (we set that up above)

Those are all satisfied by the time we get here. So the entry stub is actually very
simple — its only real purpose is to be a guaranteed landing point at address 0x1000:

```asm
[BITS 32]
[EXTERN _kernel_main]   ; defined in kernel.c (MinGW prefixes C names with _)

global _start

_start:
    call _kernel_main   ; hand off to C
    cli
.hang:
    hlt
    jmp .hang           ; if kernel_main ever returns, loop forever
```

The linker script (`linker.ld`) ensures `_start` is placed at exactly address 0x1000
so the `call 0x1000` from the bootloader lands here.

### Why `_kernel_main` and not `kernel_main`?

On Windows, the MinGW compiler follows the Microsoft/cdecl calling convention which
prefixes all C function names with an underscore in the compiled object file. So a
C function `void kernel_main(void)` becomes the symbol `_kernel_main` in the `.o`
file. We verified this with `objdump -t kernel.o`.

On Linux with ELF toolchains, this prefix doesn't exist — the symbol would just be
`kernel_main`. This is a Windows-specific quirk.

---

## 9. Writing Directly to VGA Memory (kernel.c)

Once in `kernel_main()`, we have a problem: we cannot use `printf()` or any standard
library — none of it exists. We have to write to the screen ourselves.

### How VGA text mode works

When the BIOS initialized video, it put the screen in **VGA Text Mode 3**:
- 80 columns × 25 rows of characters
- Each character occupies **2 bytes** in memory starting at physical address **0xB8000**

```
0xB8000: [char][attr][char][attr][char][attr]...
          col0        col1        col2
```

- Byte 0: ASCII code of the character
- Byte 1: Attribute byte — upper nibble = background color, lower nibble = foreground

```
Color values:
  0x0=Black  0x1=Blue   0x2=Green  0x3=Cyan
  0x4=Red    0x5=Magenta 0x6=Brown 0x7=Grey
  0x8=DkGrey 0x9=LtBlue 0xA=LtGreen 0xB=LtCyan
  0xC=LtRed  0xD=LtMagenta 0xE=Yellow 0xF=White

Attribute 0x0F = black background (0x0 << 4) | white foreground (0xF) = white on black
Attribute 0x02 = black background (0x0 << 4) | green foreground (0x2) = green on black
```

To write character 'A' in white on black at row 0, column 0:
```c
volatile unsigned char *vga = (volatile unsigned char *)0xB8000;
vga[0] = 'A';    // character
vga[1] = 0x0F;   // white on black
```

To write at row R, column C:
```c
int index = (R * 80 + C) * 2;
vga[index]     = character;
vga[index + 1] = color;
```

### Why `volatile`?

```c
volatile unsigned char *vga = (volatile unsigned char *)0xB8000;
```

The `volatile` keyword tells the C compiler: **"do not optimize away accesses to this
pointer."** Without it, the compiler sees that we write to `vga[]` but never read the
values back in C code — so it might conclude "these writes are useless" and delete
them entirely during optimization. But the hardware reads those memory locations to
draw pixels on screen. `volatile` prevents the compiler from removing what look like
"dead writes."

Any memory-mapped hardware register should always be accessed through a `volatile`
pointer.

---

## 10. The Linker Script (linker.ld)

The linker script tells GNU `ld` exactly how to construct the final binary.

```ld
ENTRY(_start)

SECTIONS {
    . = 0x1000;      /* place everything starting at address 0x1000 */

    .text : {
        KEEP(*(.text))   /* all executable code */
    }

    .data : {
        *(.data)         /* initialized global variables */
        *(.rodata)       /* string literals, const arrays */
    }

    .bss : {
        *(.bss)          /* uninitialized globals — zeroed at runtime */
    }
}
```

**`. = 0x1000`** sets the location counter to 0x1000. Every address the linker
calculates from this point on is relative to 0x1000. So when `kernel_main` is at,
say, offset 200 bytes into the binary, the linker assigns it address `0x1000 + 200 =
0x10C8`. Any call to `kernel_main` in the compiled code uses address `0x10C8`.

**`KEEP(*(.text))`** prevents the linker from garbage-collecting the `_start` symbol.
Since nothing in C "calls" `_start` (the bootloader does), the linker might think it's
unreferenced and throw it away. KEEP says: always include this.

**Object file sections** are a concept from ELF (Executable and Linkable Format) —
the file format GCC and NASM produce. Every compiled `.o` file contains named sections:
- `.text` — machine code (the actual instructions)
- `.data` — initialized variables (`int x = 5;` goes here)
- `.bss` — uninitialized variables (`int x;` goes here, zero-filled at startup)
- `.rodata` — read-only data (string literals like `"hello"` go here)

The linker merges all `.text` sections from all `.o` files into one `.text` block,
all `.data` sections into one `.data` block, and so on.

---

## 11. The Build Pipeline

```
boot2.asm ──────────── nasm -f bin ──────────────► boot.bin     (512 bytes, raw binary)

kernel_entry.asm ────── nasm -f elf32 ───────────► kernel_entry.o  (ELF object)
kernel.c ──────────────── gcc -m32 -ffreestanding ► kernel.o        (ELF object)

kernel_entry.o + kernel.o ─── ld + linker.ld ────► kernel.elf   (ELF executable at 0x1000)
kernel.elf ─────────────────── objcopy -O binary ► kernel.bin   (flat binary, no headers)

boot.bin + kernel.bin ──────── concatenate ──────► os.img       (bootable disk image)
os.img ─────────────────────── qemu-system-i386 ► running OS
```

### Key compiler flags

| Flag | Meaning |
|------|---------|
| `-m32` | Compile for 32-bit x86, not 64-bit |
| `-ffreestanding` | Don't assume a standard library or OS exists |
| `-fno-pie` | Don't generate position-independent code (we have a fixed load address) |
| `-c` | Compile only — produce a `.o` file, don't link yet |
| `-O0` | No optimization — easier to debug, code runs exactly as written |

### Why objcopy?

The linker produces an **ELF** file — a structured file format with headers that
describe the binary (entry point, section layout, symbol table, etc.). The bootloader
doesn't know how to parse ELF headers — it just jumps to address 0x1000 expecting raw
machine code. `objcopy -O binary` strips all ELF structure and produces a flat binary:
just the raw bytes, starting with the first instruction of `_start`.

---

## 12. What Each File Does

| File | Purpose |
|------|---------|
| `boot2.asm` | Bootloader: loads kernel from disk, sets up GDT, switches to Protected Mode, jumps to 0x1000 |
| `kernel_entry.asm` | First code at 0x1000: calls `kernel_main()`, hangs if it returns |
| `kernel.c` | The kernel: clears screen, prints status using direct VGA memory writes |
| `linker.ld` | Tells the linker to place everything at 0x1000, puts `_start` first |
| `Makefile` | Automates the full build pipeline |

---

## 13. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| Kernel load address | 0x1000 | Above IVT/BIOS data, below bootloader, clean round number |
| GDT model | Flat (0 → 4GB) | Simplest possible; paging handles fine-grained control later |
| Kernel entry language | C (with asm stub) | Assembly only for bootstrapping; C is practical for kernel logic |
| C symbol prefix | `_kernel_main` | MinGW on Windows prefixes C names with `_` — verified with objdump |
| Output format | `objcopy -O binary` | Bootloader needs raw bytes, not ELF headers |
| Disk read | BIOS INT 0x13, 15 sectors | Must happen before Protected Mode; 15 sectors gives us 7680 bytes of kernel space |

---

## 14. What We Proved By Getting This Working

- The bootloader successfully read sectors from disk into RAM at 0x1000
- The GDT was correctly structured and loaded into the GDTR register
- The CPU switched from 16-bit Real Mode to 32-bit Protected Mode without crashing
- The pipeline-flushing far jump worked and CS was correctly reloaded
- C code ran on bare metal — no OS, no standard library, no runtime beneath it
- Direct VGA memory writes produced visible text on screen

**Every OS in the world — Linux, Windows, macOS — does these exact same steps
at boot. We just did them ourselves from scratch.**

---

## 15. What Comes Next (Component 3 Preview)

We wrote to VGA memory directly in `kernel.c` but it's messy — no cursor tracking,
no scrolling, no way to print numbers. Component 3 builds a proper **VGA text driver**
with:
- A cursor position tracker (row and column)
- Automatic newline and scrolling when we reach the bottom
- A `kprintf()` function so we can print strings and numbers cleanly
- Color support baked in

This becomes the foundation for all visible output from the kernel going forward.
