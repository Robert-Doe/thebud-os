# Component 1: Bootloader — Deep Dive

---

## 1. How Did We Get Here? A Brief History

### The CPU knows nothing at birth

When you press the power button, the CPU wakes up in a state of complete ignorance.
It does not know what operating system you have. It does not know where your files are.
It does not even know how to talk to your hard drive. It only knows one thing:

> "Start executing instructions at this fixed memory address."

On x86 machines, that address is **0xFFFFFFF0** — a location hardwired into the CPU
design. Whatever bytes sit there get executed first. Those bytes belong to a chip on
your motherboard called the **BIOS** (or its modern replacement, UEFI).

### What is the BIOS and why does it exist?

**BIOS** stands for **Basic Input/Output System**. It was introduced by IBM in 1975 for
the IBM PC. The idea was simple: rather than making every operating system know how to
talk directly to every piece of hardware (keyboard, screen, disk), the BIOS would act
as a middleman — a small program burned into a chip that knows how to do the basics.

The BIOS lives on a ROM (Read-Only Memory) chip on your motherboard. It never goes
away when power is cut because it's not on your hard drive — it's physically etched
into silicon.

**What the BIOS does when you power on:**

```
1. POST (Power-On Self Test)
   — Tests that RAM is working
   — Tests that the CPU is functional
   — Tests that basic hardware responds

2. Hardware Initialization
   — Sets up the keyboard controller
   — Sets up basic video (so it can show error messages)
   — Detects connected storage devices (hard drives, USB sticks, etc.)

3. Boot Device Search
   — Looks through your "boot order" list (configured in BIOS settings)
   — For each device, reads the first 512 bytes
   — Checks if the last 2 bytes are 0x55, 0xAA
   — If yes: this device is bootable — load and jump to it

4. Hand off to Bootloader
   — Copies those 512 bytes into RAM at address 0x7C00
   — Jumps the CPU to 0x7C00
   — The BIOS's job is now done
```

### Why 0x7C00? Why 512 bytes? Why 0x55 0xAA?

These are not logical choices — they are **historical accidents** that got frozen into
the standard because so much software was built around them.

**0x7C00:** On the original IBM PC with 32KB of RAM, engineers needed to place the
bootloader somewhere that left maximum room for both the BIOS code and the bootloader's
own stack. They chose 0x7C00 (31,744 in decimal) — near the top of the first 32KB.
In 1981 this made sense. Today CPUs have gigabytes of RAM, but the address stayed
because changing it would break every bootloader ever written.

**512 bytes:** Early floppy disks had sectors of exactly 512 bytes. The BIOS was
designed to read exactly one sector. 512 bytes is therefore the maximum size of a
bootloader that fits in the first sector. It's tiny — you can't fit much in 512 bytes,
which is why modern bootloaders (GRUB, etc.) use tricks to load additional sectors.

**0x55 0xAA (the "magic number"):** IBM needed a way to tell a bootable disk from a
random disk full of data. They picked two arbitrary bytes as a "signature." Any disk
whose first sector ends with 0x55, 0xAA is declared bootable. This is called a
**magic number** — a special value that means something by convention, not by logic.
Note: x86 is little-endian, so 0xAA55 stored in memory appears as 55 AA when reading
byte-by-byte.

---

## 2. What is Real Mode?

When the CPU first starts, it runs in **16-bit Real Mode**. This is not a choice we
make — it is mandatory. Every x86 CPU since the 8086 (1978) starts in Real Mode for
backwards compatibility. Intel has never broken this rule in 45+ years.

### Where Real Mode comes from

In 1978, Intel released the **8086** processor. It had a 16-bit data bus and could
address up to **1MB of RAM** using a scheme called segmented addressing. At the time,
1MB was enormous — most computers had 16KB or 64KB.

The addressing scheme worked like this:

```
Physical Address = Segment Register × 16 + Offset

Example:
  DS = 0x1000 (Data Segment register)
  SI = 0x0050 (offset)
  Physical address = 0x1000 × 16 + 0x0050
                   = 0x10000 + 0x0050
                   = 0x10050
```

You have four segment registers: **CS** (Code), **DS** (Data), **ES** (Extra), **SS**
(Stack). Each is 16 bits. The offset is also 16 bits. Together they can reach 20-bit
addresses — exactly 1MB.

### Why Real Mode is dangerous

Real Mode has **no memory protection**. Any program can read or write any memory
address — including the memory used by the BIOS, other programs, or the OS itself.
There are no privilege levels. A bug in any program can corrupt everything.

This is why Real Mode is only used for the boot process. As soon as we can, we will
switch to **Protected Mode** (Component 2), which gives us:
- 32-bit addresses (4GB of RAM)
- Memory protection (programs can't touch each other's memory)
- Privilege rings (user programs can't execute privileged instructions)

---

## 3. What is Assembly Language?

Our bootloader is written in **Assembly language** (specifically NASM — Netwide
Assembler syntax). Here's why we can't use C here:

**C requires a runtime environment.** Before C code runs, something must:
- Set up the stack pointer
- Zero out the BSS segment (uninitialized global variables)
- Set up the C standard library

None of that exists yet — *we* have to create it. The only language that works without
any setup is assembly, because assembly instructions translate directly to CPU
instructions with no assumptions.

### How assembly works

Every line in assembly is either:
1. A **CPU instruction** — tells the CPU to do something (`mov`, `add`, `jmp`, etc.)
2. A **directive** — tells the assembler (NASM) something (`[BITS 16]`, `db`, `dw`)
3. A **label** — a named address you can jump to or reference (`start:`, `.loop:`)

The NASM assembler converts your `.asm` text file into a flat binary — raw bytes that
the CPU can execute directly. No file headers, no metadata — just machine code bytes.

---

## 4. Line-by-Line Explanation of boot.asm

```asm
[BITS 16]
```
A directive telling NASM: "encode all instructions as 16-bit." Without this, NASM
would generate 32-bit machine code and the CPU in Real Mode would misinterpret it.

```asm
[ORG 0x7C00]
```
A directive telling NASM: "assume this code will be loaded at address 0x7C00."
This matters because any time you reference a label (like `msg_boot`), NASM needs to
know the actual memory address that label will have at runtime. If the code is at
0x7C00, the label at offset 200 bytes in will be at address 0x7C00 + 200 = 0x7CC8.

```asm
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax
```
`xor ax, ax` is a classic trick — XORing anything with itself gives zero, but it
generates a smaller/faster instruction than `mov ax, 0`. We zero out all segment
registers so our memory addressing is simple and predictable. The BIOS may have left
them in unknown states.

You cannot `mov ds, 0` directly — the x86 architecture only allows loading segment
registers from another register, not an immediate value. That's why we go through AX.

```asm
mov sp, 0x7C00
```
Sets the **Stack Pointer** to 0x7C00. The stack grows *downward* in memory, so this
puts the top of the stack just below our bootloader code. When we `call print_string`,
the CPU pushes the return address onto the stack. Without a valid stack, `call`/`ret`
would crash.

```asm
mov si, msg_boot
call print_string
```
`SI` is the **Source Index** register — conventionally used for string/array pointers.
We put the address of our message string in SI, then call our print function.

```asm
cli
hlt
```
`cli` = **Clear Interrupt flag** — disables hardware interrupts so nothing can wake the
CPU. `hlt` = **Halt** — stops the CPU from executing instructions. Together they create
a clean, permanent stop. (Without `cli`, a hardware interrupt could theoretically fire
after `hlt` and resume execution somewhere unexpected.)

### The print_string function

```asm
mov ah, 0x0E
```
**BIOS Video Services** live at interrupt `INT 0x10`. The `AH` register selects which
video function to call. Function `0x0E` is "Teletype Output" — it prints one character
and advances the cursor, just like an old teletype machine.

```asm
lodsb
```
One of the most elegant x86 instructions: **Load String Byte**. It:
1. Reads the byte at the memory address in `DS:SI`
2. Stores it in `AL`
3. Automatically increments `SI` by 1

So we don't need to manually increment our string pointer — `lodsb` does it for us.

```asm
test al, al
jz .done
```
`test al, al` performs a bitwise AND of AL with itself and sets the **Zero Flag** if
the result is zero — without actually changing AL. `jz .done` = "Jump if Zero" —
jumps to `.done` if AL is 0. This is how we detect the **null terminator** that ends
our string (the `0` at the end of `msg_boot`).

```asm
int 0x10
```
Fires a **software interrupt** — the CPU looks up interrupt vector 0x10 in the **IVT**
(Interrupt Vector Table, located at address 0x0000), finds the address of the BIOS
video routine, and calls it. The BIOS then reads AH (function 0x0E) and AL (character)
and draws the character on screen.

### The boot signature

```asm
times 510 - ($ - $$) db 0
dw 0xAA55
```
`$` = current address. `$$` = address of the start of this section (0x7C00).
`$ - $$` = how many bytes of code/data we've written so far.
`510 - ($ - $$)` = how many zero bytes to add to reach byte 510.
`dw 0xAA55` = write a 16-bit word (2 bytes): 0xAA55.

In memory this appears as `55 AA` (little-endian — low byte first). Total: 512 bytes.
The BIOS sees `55 AA` at position 510-511 and confirms: bootable.

---

## 5. What is QEMU and Why Are We Using It?

**QEMU** (Quick Emulator) is a program that emulates an entire computer in software —
CPU, RAM, disk, keyboard, screen. Instead of burning our OS to a USB stick and
rebooting a physical machine every time we make a change, QEMU lets us:

- Launch our OS in under a second
- Restart it instantly after a crash
- Use a virtual "floppy disk" (`-fda boot.img`) that's just a file on our real disk
- Debug with GDB (later)

The flag `-fda boot.img` tells QEMU: "pretend this file is a floppy disk in drive A."
QEMU's BIOS then reads the first 512 bytes of `boot.img`, sees `55 AA`, and boots it.

---

## 6. Decisions We Made for This Component

| Decision | What we chose | Why |
|----------|--------------|-----|
| Assembler | NASM | Cleaner Intel syntax, better error messages than GAS |
| Boot method | BIOS (not UEFI) | UEFI requires a FAT32 partition + PE executable; BIOS is just 512 bytes |
| Architecture | x86 16-bit | Mandatory starting point — all x86 CPUs begin in Real Mode |
| Testing | QEMU | No need to reboot a physical machine; instant feedback |
| Language | Assembly only | No C runtime exists yet; assembly needs no setup |

---

## 7. What We Proved By Getting This Working

- The CPU successfully handed control from the BIOS to our code at 0x7C00
- Our stack is set up correctly (the `call`/`ret` for `print_string` worked)
- We can use BIOS interrupts to output text
- Our binary is exactly 512 bytes with the correct boot signature
- QEMU correctly simulates the BIOS boot process

**This is the foundation everything else builds on.**

---

## 8. What Comes Next (Component 2 Preview)

Right now we're stuck in 16-bit Real Mode with 1MB of RAM and no memory protection.
In Component 2, we will:

1. Load our kernel from disk into RAM (using BIOS disk services)
2. Switch the CPU to **32-bit Protected Mode** — this unlocks 4GB of addressing,
   memory protection, and privilege rings
3. Jump into C code for the first time — the assembly does the setup, then hands off

The switch to Protected Mode requires setting up the **GDT (Global Descriptor Table)**
— a table that tells the CPU how memory is structured. The CPU refuses to enter
Protected Mode without it.
