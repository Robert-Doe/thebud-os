# Component 3: VGA Text Driver — Deep Dive

---

## 1. Where We Left Off

Component 2 got C code running on bare metal. To prove it worked, `kernel_main()`
wrote directly to VGA memory at 0xB8000 using raw pointer arithmetic:

```c
vga_putchar(row, col, 'A', 0x0F);
```

Every call required the caller to track row and column manually. There was no scrolling,
no `\n` handling, no way to print numbers. Any output beyond a few hardcoded strings
would have required writing new low-level buffer code each time.

Component 3 solves this permanently. We build a **VGA text driver** — a small module
that encapsulates all VGA state and provides a clean API for every future component.
After this, every part of the kernel that wants to print anything just calls `kprintf()`.

---

## 2. What a "Driver" Means Here

In a full OS, a driver is a kernel module with a standardized interface (open/read/write/ioctl),
device discovery, interrupt registration, and power management. That's all future work.

Here, "driver" means something simpler: **a module that owns a piece of hardware and
exposes a clean API.** Nothing outside `vga.c` touches VGA memory directly. This is
the same principle — ownership and encapsulation — just at a smaller scale.

The discipline matters even this early. If two modules write to `0xB8000` independently
without coordinating cursor position, they'll overwrite each other's output. Centralizing
all VGA access in one module prevents that class of bug entirely.

---

## 3. VGA Text Mode Recap (Why This Works)

When the BIOS initialized video at boot, it configured the display in **VGA Text Mode 3**:

| Property         | Value                        |
|------------------|------------------------------|
| Screen size      | 80 columns × 25 rows         |
| Buffer address   | Physical 0xB8000             |
| Bytes per cell   | 2 (one character, one color) |
| Total buffer size| 80 × 25 × 2 = 4000 bytes     |

The VGA hardware (or its emulated equivalent in QEMU) **continuously reads** this
4000-byte region of RAM and refreshes the monitor. Writing to 0xB8000 *is* drawing to
the screen — there is no GPU, no frame buffer, no draw call. The hardware does the
pixel rendering from our bytes automatically.

Each 2-byte cell:
```
Byte 0: ASCII character code (0x41 = 'A', 0x20 = space, etc.)
Byte 1: Attribute byte — bits 7-4: background color, bits 3-0: foreground color
```

Row R, column C maps to byte index:
```
index = (R * 80 + C) * 2
```

---

## 4. Driver Architecture

```
┌───────────────────────────────────────────────────────────┐
│                       kernel.c                            │
│   kprintf("Value: %d\n", 42);                             │
└─────────────────────┬─────────────────────────────────────┘
                      │ calls
┌─────────────────────▼─────────────────────────────────────┐
│                       vga.c                               │
│                                                           │
│  State: cur_row, cur_col, cur_color                       │
│                                                           │
│  kprintf() → vga_putchar() → buf_write() → 0xB8000       │
│                scroll() ──────────────────────────────►   │
└───────────────────────────────────────────────────────────┘
```

### State owned by the driver

```c
static int cur_row   = 0;
static int cur_col   = 0;
static unsigned char cur_color = VGA_COLOR(VGA_BLACK, VGA_WHITE);
```

`static` at file scope means these variables are **not visible outside `vga.c`**.
Nothing else can accidentally change the cursor position. This is enforced by the
C compiler — not a convention, not a comment.

### Why `volatile` on the VGA buffer pointer

```c
static volatile unsigned char *vga_buf = (volatile unsigned char *)0xB8000;
```

`volatile` is explained in Component 2 but worth repeating: the C compiler tracks
data flow. It sees that we write to `vga_buf[i]` but never read back from it in C.
It concludes those writes are useless and can be removed (dead store elimination).
But the *hardware* reads those memory locations to draw on screen — eliminating the
write would mean nothing appears. `volatile` tells the compiler: "this memory is
observed by something outside C; never optimize away accesses to it."

---

## 5. Scrolling — The Key New Feature

When the cursor reaches row 25, there is no "row 25" — the screen has only 25 rows
(0 through 24). We must scroll: move every row up by one and blank the bottom row.

```c
static void scroll(void) {
    int row, col;
    for (row = 0; row < VGA_ROWS - 1; row++) {
        for (col = 0; col < VGA_COLS; col++) {
            int dst = (row * VGA_COLS + col) * 2;
            int src = ((row + 1) * VGA_COLS + col) * 2;
            vga_buf[dst]     = vga_buf[src];
            vga_buf[dst + 1] = vga_buf[src + 1];
        }
    }
    for (col = 0; col < VGA_COLS; col++) {
        buf_write(VGA_ROWS - 1, col, ' ', cur_color);
    }
}
```

This is a memory copy — 4000 bytes of VGA buffer minus the last row, shifted up by
one row's worth of bytes (160 bytes). It's O(rows × cols) but runs only when we
overflow the last row, which is infrequent enough that the cost doesn't matter.

**Alternative considered: circular buffer.** Instead of physically moving memory, we
could track a "top row" offset and treat the buffer as a ring. This would make
scrolling O(1). We rejected it because:

1. It requires the cursor-to-buffer mapping to account for the ring offset on every
   single character write (making the common case more expensive).
2. It's harder to understand.
3. For 25 rows of text, physically shifting 4000 bytes is imperceptibly fast on any
   real CPU. The optimization buys nothing observable.

---

## 6. Special Character Handling

`vga_putchar()` handles three special cases:

| Character | Behavior |
|-----------|----------|
| `\n` (newline) | Set `cur_col = 0`, increment `cur_row`, scroll if needed |
| `\r` (carriage return) | Set `cur_col = 0`, leave `cur_row` unchanged |
| `\t` (tab) | Advance to the next column that is a multiple of 8, writing spaces |

Tab stops at multiples of 8 match the traditional Unix terminal convention. The
implementation writes real space characters rather than just moving the cursor — this
ensures the cells are blank-colored rather than containing whatever was there before.

All other bytes (including non-printable ones) are written directly to the buffer as
the character code with the current color. The VGA font chip will render whatever
character its ROM maps to that code point.

---

## 7. kprintf — Minimal Kernel printf

### Why not just `printf()`?

`printf()` lives in the C standard library (libc). We have no libc — the standard
library depends on an OS to exist (`malloc`, `write()`, file descriptors, etc.).
When we compiled with `-ffreestanding`, we explicitly told GCC to not assume any of
that exists.

We implement just the subset we need: `%c`, `%s`, `%d`, `%u`, `%x`, `%X`, `%%`.

### Variadic arguments: `stdarg.h`

```c
#include <stdarg.h>

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ...
    char c = (char)va_arg(args, int);
    ...
    va_end(args);
}
```

`<stdarg.h>` is part of **GCC itself**, not glibc. The compiler provides it as
compiler built-ins (`__builtin_va_list`, etc.) regardless of `-ffreestanding`. It is
safe to use in a kernel — we confirmed this by checking the GCC manual and observing
the compiled output contains no library function calls.

### Integer formatting without `sprintf`

There is no `itoa()` or `sprintf()` available. We wrote `print_uint()`:

```c
static void print_uint(unsigned int n, unsigned int base, const char *digits) {
    char buf[16];
    int i = 0;

    if (n == 0) { vga_putchar('0'); return; }

    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i > 0) {
        vga_putchar(buf[--i]);
    }
}
```

Repeatedly dividing by the base extracts digits in **least-significant-first order**,
so they're stored in a local buffer, then printed in reverse. `buf[16]` is enough for
any 32-bit value: 10 decimal digits, 8 hex digits, plus sign and null.

For signed integers, we handle `INT_MIN` (-2147483648) correctly by casting the
negated value to `unsigned int` before passing to `print_uint`. In two's complement,
`-INT_MIN` would overflow a signed int, but casting to unsigned first makes it safe.

### `%d` reads `int`, not `char` or `short`

C's variadic argument rules (the "default argument promotions") require that any
integer argument narrower than `int` is promoted to `int` before being passed to a
variadic function. So `va_arg(args, int)` is always correct for `%c` and `%d`.
Using `va_arg(args, char)` would be undefined behavior.

---

## 8. The Color API

```c
#define VGA_COLOR(bg, fg)  (((unsigned char)(bg) << 4) | (unsigned char)(fg))

void vga_set_color(unsigned char color);
```

Color is global driver state, not a per-call parameter. This matches how terminals
work: you set a color, then print. Passing color to every individual character write
would have been verbose and redundant.

The 16 VGA color constants (VGA_BLACK through VGA_WHITE) are defined in `vga.h` so
any module that includes the header can use them by name rather than magic numbers.

---

## 9. File Structure

| File             | Purpose |
|------------------|---------|
| `vga.h`          | Public API: constants, color macros, function declarations |
| `vga.c`          | Driver implementation: state, cursor, scroll, kprintf |
| `kernel.c`       | Kernel entry: exercises every driver feature |
| `kernel_entry.asm` | Lands at 0x1000, calls `kernel_main()`, hangs on return |
| `linker.ld`      | Places binary at 0x1000, `_start` first |
| `boot.asm`       | Bootloader: loads kernel from disk, switches to Protected Mode |
| `Makefile`       | Assembles, compiles, links, and builds `os.img` |

---

## 10. Toolchain: Why We Use WSL for C Compilation

Component 2's Makefile listed `ld -m elf_i386` but the machine has MinGW32, whose
`ld.exe` only supports the `i386pe` emulation (Windows PE format). Our kernel needs
the ELF linker so:

1. `--oformat binary` (strip all headers, produce raw bytes) works correctly.
2. The linker script's `. = 0x1000` is interpreted as a virtual memory address, not
   a file offset — which is the ELF linker's default behavior.
3. All ELF section directives (`.text`, `.data`, `.bss`) map cleanly.

With the PE linker, an `--oformat binary` strip would still include the MZ/PE header
bytes at the start of the file, so the bootloader's `call 0x1000` would land in the
middle of a PE header rather than our `_start` function.

**Solution**: WSL (Ubuntu) ships with the standard GNU toolchain that targets ELF
natively. We compile C files and link inside WSL, but assemble NASM on Windows (NASM
produces portable ELF objects regardless of host OS, and it isn't installed in WSL).

**Symbol naming consequence**: Linux GCC does not add an underscore prefix to C
function names. The NASM stub in component 2 referenced `_kernel_main` (MinGW
convention); here it references `kernel_main` (ELF/Linux convention). This is noted
in `kernel_entry.asm`.

## 11. Build Pipeline

```
boot.asm ─────────────── nasm -f bin ────────────► boot.bin       (512 bytes)

kernel_entry.asm ──────── nasm -f elf32 ──────────► kernel_entry.o
vga.c ─────────────────── gcc -m32 -ffreestanding ► vga.o
kernel.c ──────────────── gcc -m32 -ffreestanding ► kernel.o

kernel_entry.o            ┐
vga.o            ─────────┤ ld -T linker.ld ──────► kernel.bin    (flat binary at 0x1000)
kernel.o                  ┘

boot.bin + kernel.bin ──── concatenate ───────────► os.img
os.img ─────────────────── qemu-system-i386 ──────► running OS
```

The link order matters: `kernel_entry.o` must be first so the linker places
`_start` at 0x1000. If `vga.o` came first, the bootloader's `call 0x1000`
would land in the middle of VGA driver code and immediately crash.

---

## 11. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| Scroll algorithm | Physical memory shift | Simpler than a ring buffer; 4000-byte copy is imperceptibly fast |
| Tab stops | Every 8 columns | Standard Unix terminal convention |
| kprintf color model | Global state via `vga_set_color()` | Matches terminal semantics; avoids color parameter on every call |
| Variadic args | `<stdarg.h>` from GCC | Safe in `-ffreestanding`; compiler-provided, no libc dependency |
| `%d` negative case | Cast to unsigned before negation | Handles INT_MIN without signed overflow (undefined behavior) |
| Integer buffer size | `char buf[16]` | Sufficient for any 32-bit decimal (10 digits) or hex (8 digits) |
| Width/precision in kprintf | Not implemented | Not needed yet; adds complexity for no current benefit |
| Copy bootloader per module | Yes | Each directory is self-contained and runnable independently |

---

## 12. What We Proved By Getting This Working

- The kernel can track cursor state across many `kprintf()` calls without the caller
  knowing anything about rows and columns.
- Scrolling works: when output exceeds 25 lines, earlier lines roll off the top and
  new lines appear at the bottom.
- `kprintf()` correctly formats strings, signed integers, unsigned integers, and
  hexadecimal values without any standard library.
- `volatile` prevented the compiler from eliding our VGA writes under `-O0` (and
  would prevent it under higher optimization levels too).
- `<stdarg.h>` works correctly in a freestanding environment.

---

## 13. What Comes Next (Component 4 Preview)

Component 4 revisits the **GDT (Global Descriptor Table)** — we defined one in the
bootloader to get into Protected Mode, but it was a quick assembly blob. Now we move
GDT setup into proper C code inside the kernel itself:

- Define the GDT as a C struct array
- Write a `gdt_install()` function that builds the entries and calls `lgdt`
- Understand segment selectors and privilege rings more deeply
- Lay the groundwork for the IDT (Component 5), which must be set up similarly

Every kernel debug message during that process will print through the VGA driver
we just built.
