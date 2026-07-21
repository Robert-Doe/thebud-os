# Component 4: GDT in C — Deep Dive

---

## 1. Where We Left Off

The bootloader in every previous module defined a GDT as raw assembly bytes:

```asm
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
```

This works, but it is a black box. Those bytes are the result of Intel's scrambled
descriptor layout hand-packed into NASM directives. There is no structure, no named
fields, and no way to programmatically build or modify an entry. To add a user-mode
segment later (Ring 3, for Module 10), you would have to hand-craft another blob of
bytes and hope you got every bit right.

Component 4 moves GDT setup into the kernel as proper C code:
- The GDT is a typed C array of `struct gdt_entry` (8 bytes, packed).
- `gdt_set_entry()` takes human-readable parameters and fills the scrambled layout.
- `gdt_load()` uses inline assembly to call `lgdt` and reload every segment register.
- The kernel uses `sgdt` to read the GDTR back and display what the CPU loaded.

After this, adding new segments (Ring 3 code/data, TSS) is one function call.

---

## 2. Why the GDT Layout Is Scrambled

Each GDT entry is exactly 8 bytes, but the fields are not laid out logically. The
base address is split across three separate locations, and so is the limit:

```
Byte 7    Byte 6         Byte 5    Bytes 4–2   Bytes 1–0
┌────────┬──────────────┬─────────┬───────────┬──────────┐
│Base    │Flags[7:4]    │ Access  │ Base      │ Limit    │
│[31:24] │+Limit[19:16] │  Byte   │ [23:0]    │ [15:0]   │
└────────┴──────────────┴─────────┴───────────┴──────────┘
```

**Why is it like this?** The 80286 (1982) used a 24-bit address bus and 24-bit GDT
descriptors. When Intel designed the 80386 (1985) with 32-bit addresses, they needed
the new 8-byte descriptors to remain backward-compatible with 286 descriptors. The
only way to do that without breaking existing 286 software was to extend the existing
fields rather than redesign the layout. The result is the scrambled format we still
use today on every x86 CPU.

We live with it by hiding the complexity in `gdt_set_entry()` — that function handles
the bit-shifting and masking so the rest of the code never sees it.

---

## 3. `__attribute__((packed))` — Why It Is Non-Negotiable

```c
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit;
    uint8_t  base_high;
} __attribute__((packed));
```

Without `__attribute__((packed))`, the C compiler is free to insert **padding bytes**
between struct fields to align them on natural boundaries. On a 32-bit system, the
compiler might pad `base_mid` (1 byte) up to 4 bytes for alignment, making the struct
12 bytes instead of 8.

The CPU's GDTR contains the *byte address* of the `gdt[]` array in RAM. When the CPU
reads a descriptor, it takes the base address plus `(selector / 8) * 8` — it treats
memory as tightly-packed 8-byte records. If our struct has padding, the CPU reads
bytes that don't belong to the descriptor and interprets garbage as permissions and
limits. The result is either an immediate General Protection Fault or, worse, silent
misbehavior.

`__attribute__((packed))` is the GCC extension that forces `sizeof(struct gdt_entry)
== 8` regardless of alignment preferences.

---

## 4. `gdt_set_entry()` — Hiding the Scrambled Layout

```c
static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t flags)
{
    gdt[i].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[i].base_low    = (uint16_t)(base  & 0xFFFF);
    gdt[i].base_mid    = (uint8_t)((base  >> 16) & 0xFF);
    gdt[i].base_high   = (uint8_t)((base  >> 24) & 0xFF);
    gdt[i].access      = access;
    gdt[i].flags_limit = (uint8_t)(((flags & 0x0F) << 4) |
                                    ((limit >> 16) & 0x0F));
}
```

The tricky line is `flags_limit`. This byte holds two things at once:
- **High nibble (bits 7–4):** the flags (granularity=1, 32-bit=1, long=0, reserved=0)
- **Low nibble (bits 3–0):** limit bits 16–19 (the upper 4 bits of the 20-bit limit)

For our flat segments, limit = 0xFFFFF:
```
limit[19:16] = 0xF
flags        = 0xC  (binary 1100)
flags_limit  = (0xC << 4) | 0xF = 0xCF
```

This is exactly the `db 11001111b` you see in the bootloader's hand-crafted GDT. Now
you know where that byte comes from.

---

## 5. The Access Byte — Field by Field

```
Bit 7  Present (P)    = 1  → this segment is valid
Bit 6  DPL high       ─┐
Bit 5  DPL low        ─┘  = 00 → Ring 0 (kernel privilege)
Bit 4  Descriptor type = 1  → code or data segment (not a system segment)
Bit 3  Executable (E) = 1  → code segment (can execute)    0 → data segment
Bit 2  DC             = 0  → code: non-conforming; data: grows upward
Bit 1  RW             = 1  → code: readable; data: writable
Bit 0  Accessed (A)   = 0  → CPU will set this when the segment is first used
```

Code segment:  1 00 1 1 0 1 0 = 0x9A
Data segment:  1 00 1 0 0 1 0 = 0x92

These match the bytes in the bootloader exactly, now expressed as understandable
bit fields rather than magic numbers.

---

## 6. Why the Limit Is 0xFFFFF and Not 0xFFFFFFFF

The GDT limit field is only **20 bits wide** — it fits in the entry's 16-bit
`limit_low` field (bits 0–15) plus the 4-bit upper portion in `flags_limit` (bits
16–19). The maximum raw limit value is therefore 0xFFFFF (20 ones).

The **Granularity bit (G = 1)** scales this by 4096:

```
Effective limit = (0xFFFFF + 1) × 4096 = 0x100000 × 4096 = 4,294,967,296 = 4 GB
```

So passing `limit = 0x000FFFFF` with `flags = GDT_FLAGS` (G=1, DB=1) tells the CPU
the segment runs from base to base + 4GB. We cover the entire 32-bit address space.

---

## 7. Reloading CS — Why a Normal `mov` Won't Work

After calling `lgdt`, the CPU knows where the new GDT is. But the segment registers
still hold the *old* values from the bootloader's GDT — which has now been superseded.
We need to reload them all.

DS, ES, FS, GS, SS can be reloaded with ordinary `mov` instructions:

```c
__asm__ volatile (
    "mov %0, %%ax  \n"
    "mov %%ax, %%ds \n"
    ...
    : : "i"(GDT_SEL_DATA) : "ax"
);
```

**CS is different.** The x86 architecture does not allow `mov cs, ax`. CS is the
code segment register — it determines where the CPU is currently fetching instructions
from. The CPU architects made it write-protected against `mov` to prevent accidental
corruption. The only instructions that can change CS are:

- Far jumps (`jmp segment:offset`)
- Far calls (`call segment:offset`)
- Far returns (`lret`)
- Interrupts / `iret`

We use the **far return trick**:

```c
__asm__ volatile (
    "pushl %0   \n"   /* push new CS selector (0x08) */
    "pushl $1f  \n"   /* push return address (label '1:') */
    "lret       \n"   /* far return: pops EIP, then CS */
    "1:         \n"   /* execution continues here in new CS */
    : : "i"(GDT_SEL_CODE)
);
```

`lret` (long/far return) pops two values from the stack: first the new EIP (which is
the address of label `1:`), then the new CS (which is `GDT_SEL_CODE = 0x08`). The CPU
atomically updates both registers, flushes the pipeline, and continues executing at
label `1:` — now in the new code segment.

---

## 8. Inline Assembly — `__asm__ volatile`

GCC's inline assembly syntax:

```c
__asm__ volatile (
    "instruction \n"      /* assembly template */
    : output operands     /* what the asm writes to C variables */
    : input operands      /* what C values the asm reads */
    : clobber list        /* registers/memory the asm destroys */
);
```

Key rules we follow:
- `volatile` prevents GCC from reordering or eliminating the instruction.
- `"m"(gdtr)` means "a memory operand at the address of gdtr" — the assembler
  generates the correct addressing mode.
- `"=m"(buf)` means "write the result to this memory location."
- `"i"(GDT_SEL_CODE)` means "an immediate constant."
- `"ax"` in the clobber list tells GCC "I used register AX; do not assume it
  still holds the value it had before this asm block."

---

## 9. Reading Back the GDTR with `sgdt`

```c
uint8_t buf[6];
__asm__ volatile ("sgdt %0" : "=m"(buf));
```

`sgdt` (Store GDT Register) copies the CPU's internal GDTR register — 6 bytes: a
2-byte limit followed by a 4-byte base address — into the memory operand. We then
extract the fields manually with byte shifts.

This is useful both for verification (proving `lgdt` worked) and for debugging. If
the displayed base address matches the address of our `gdt[]` array, the CPU is using
our table.

---

## 10. The Null Descriptor — A Hardware Safety Net

```c
gdt_set_entry(0, 0, 0, 0, 0);   /* all zeros */
```

The CPU specification requires GDT[0] to be entirely zero. The reason: the null
selector (0x00) is used as a sentinel value meaning "no segment loaded." If any
segment register is accidentally loaded with 0x00 (e.g., by a bug that clears a
register), the CPU immediately fires a General Protection Fault (#GP) when the next
memory access through that segment occurs. This converts a silent corruption into a
loud, catchable fault.

Without the null descriptor, loading 0x00 would cause the CPU to read GDT[0] as if
it were a real segment and use whatever garbage happens to be there — an extremely
difficult bug to diagnose.

---

## 11. What Changed vs. the Bootloader's GDT

| Property             | Bootloader GDT          | Kernel GDT (this module) |
|----------------------|-------------------------|--------------------------|
| Defined in           | Assembly (`boot.asm`)   | C (`gdt.c`)              |
| Layout               | Hand-crafted byte blobs | Typed `struct gdt_entry` |
| Fields               | Anonymous bit patterns  | Named, documented fields |
| Loading              | `lgdt [label]` in asm   | `gdt_load()` with inline asm |
| CS reload            | Happens in bootloader   | `lret` trick in `gdt_load()` |
| Extensibility        | Manual byte editing     | Call `gdt_set_entry()` once |

The effect is identical — flat 0→4GB, code at 0x08, data at 0x10 — but the kernel's
version is readable, maintainable, and ready for extension.

---

## 12. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| `__attribute__((packed))` | Required on both structs | CPU reads descriptors as tightly-packed 8-byte records; padding would corrupt every field |
| Limit value | 0x000FFFFF | 20-bit field maximum; with G=1 granularity this covers 4GB |
| Flags | G=1, DB=1 | 4KB granularity and 32-bit segments — required for Protected Mode |
| CS reload method | `lret` trick | Only valid way to update CS from C inline assembly; far jump is equivalent but `lret` doesn't require a data label |
| Clobber list on data reload | `"ax"` | We use AX as a scratch register; must declare it so GCC doesn't assume it holds a stale value |
| `sgdt` for verification | Included in kernel_main | Proves the lgdt instruction actually executed and the CPU stored our address |
| `static` on gdt[] and gdtr | Yes | Nothing outside `gdt.c` should ever touch the raw GDT; all access goes through `gdt_init()` |

---

## 13. What We Proved By Getting This Working

- A GDT can be defined entirely in C using packed structs, with no assembly data blobs.
- `gdt_set_entry()` correctly packs the scrambled Intel descriptor format.
- `lgdt` executed without causing a CPU fault, meaning our struct size and address
  are valid.
- The `lret` trick successfully reloaded CS without a General Protection Fault.
- `sgdt` confirmed the GDTR base address matches the address of our C array.
- All segment registers now point to descriptors we defined and understand completely.

---

## 14. What Comes Next (Component 5 Preview)

Component 5 builds the **IDT (Interrupt Descriptor Table)** — the CPU's table of
handlers for every possible exception and hardware interrupt. The IDT follows almost
exactly the same pattern as the GDT:

- A packed C struct for each 8-byte IDT entry (called a "gate descriptor")
- A builder function `idt_set_gate()` analogous to `gdt_set_entry()`
- An `idt_load()` function using `lidt` (analogous to `lgdt`)
- A `idt_init()` public function that wires up all 256 entries

With the IDT in place, the CPU can handle divide-by-zero errors, page faults,
and hardware IRQs (keyboard, timer) without crashing into undefined behavior.
