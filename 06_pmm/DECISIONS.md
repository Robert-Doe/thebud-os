# Component 6: Physical Memory Manager — Deep Dive

---

## 1. Where We Left Off

Component 5 gave us a fully working interrupt system. The CPU can now handle
hardware IRQs and exceptions, the timer ticks at ~18 Hz, and any crash prints
a register dump. But we have no concept of memory: when we need storage we just
declare global arrays and trust the compiler to place them somewhere. That
works for small things, but as soon as we want paging (Module 7) or a heap
(Module 8) we need to answer the question: **which pages of physical RAM are
free for us to use?**

Component 6 answers that question with the **Physical Memory Manager (PMM)** —
a module that tracks every 4 KB page of RAM and lets other modules request or
return pages with a simple `pmm_alloc_page()` / `pmm_free_page()` API.

---

## 2. What Is "Physical Memory"?

The CPU sees RAM as a flat array of bytes addressed from `0x00000000` upward.
Each byte has a unique **physical address**. We call this the **physical address
space**.

This is different from **virtual addresses** (what programs see after paging is
turned on, Module 7). At this stage paging is off, so virtual = physical: the
address we put on the bus is exactly the RAM cell we get back.

Not all of the physical address space is RAM you can freely use. On a typical
x86 PC at boot:

```
Physical Address      Content
─────────────────────────────────────────────────────
0x00000000–0x000003FF  Real-Mode Interrupt Vector Table (IVT)
0x00000400–0x000004FF  BIOS Data Area (BDA)
0x00000500–0x00007BFF  Available in real mode (our stack lives here)
0x00007C00–0x00007DFF  Bootloader (512 bytes loaded by BIOS)
0x00001000–~           Our kernel image
0x000A0000–0x000BFFFF  VGA text/graphics buffer
0x000C0000–0x000FFFFF  BIOS ROM and adapter ROMs
0x00100000–...         Extended memory — free RAM above 1 MB
```

The PMM must know about this layout so it never hands out a page that contains
the VGA buffer or the kernel itself.

---

## 3. The Page — The Fundamental Unit of Memory

Rather than tracking individual bytes (which would need a bitmap larger than the
RAM it describes) we divide physical memory into **pages** of exactly **4096
bytes (4 KB)**.

Why 4 KB?
- It is the page size the x86 MMU (Memory Management Unit) uses for paging.
- It matches the PMM, the paging hardware, and virtually every OS ever written
  on x86 — they all speak the same unit.
- 4 KB pages mean a 32 MB address space needs only 8,192 pages. At 1 bit per
  page, the entire bitmap fits in **1 KB**.

A page address is always a multiple of 4096. For example:
- Page 0 → physical 0x00000000
- Page 1 → physical 0x00001000
- Page 256 → physical 0x00100000  (exactly 1 MB)
- Page 8192 → physical 0x02000000 (32 MB)

Converting between page numbers and addresses:
```
address = page_number × 4096   (or equivalently: page_number << 12)
page_number = address / 4096   (or equivalently: address >> 12)
```

---

## 4. The Bitmap Data Structure

We represent the state of each page with a single bit:

```
bit = 1  →  page is FREE  (available to allocate)
bit = 0  →  page is USED  (kernel, reserved, or already allocated)
```

The bitmap is stored as an array of `uint8_t`:

```
bitmap[0]  covers pages 0-7
bitmap[1]  covers pages 8-15
bitmap[2]  covers pages 16-23
...
bitmap[i]  covers pages (i*8) through (i*8 + 7)
```

For any page number `p`:
```c
byte   = bitmap[p / 8]         /* or bitmap[p >> 3] */
bit    = (byte >> (p % 8)) & 1 /* or (byte >> (p & 7)) & 1 */
```

To **set free** (mark available):
```c
bitmap[p >> 3] |= (1u << (p & 7));
```

To **set used** (mark allocated):
```c
bitmap[p >> 3] &= ~(1u << (p & 7));
```

**Memory cost**: 128 MB / 4 KB = 32,768 pages → 32,768 bits = **4,096 bytes**
(4 KB). We spend 4 KB of RAM to track 128 MB of RAM — a 1:32,768 ratio.

---

## 5. Why Bit = 1 Means FREE (not USED)

Many OS textbooks use the opposite convention (1 = used). We use 1 = free
because:

1. **Safe initialisation**: the bitmap lives in `.bss`, which is zeroed to 0 by
   the C runtime before `kernel_main` runs. A zeroed bitmap means "all pages
   used" — the safe default before `pmm_init()` tells us what is actually free.
   No page can accidentally be handed out before the PMM is initialised.

2. **Fast alloc scan**: `pmm_alloc_page()` scans byte-by-byte looking for a
   non-zero byte. If a byte is 0 all 8 pages in it are used — we skip it in
   one comparison. This makes the common case (mostly-used memory) fast.

---

## 6. The 1 MB Reserved Floor

The PMM never allocates pages 0–255 (physical 0x00000000–0x000FFFFF), even
if the kernel ends below 1 MB. This region contains:

- The Real-Mode IVT (addresses 0x000–0x3FF) — the CPU uses it for BIOS
  interrupts we might still need to call.
- The BIOS Data Area (0x400–0x4FF).
- Our bootloader at 0x7C00.
- The VGA frame buffer at 0xA0000–0xBFFFF — writing to these pages changes
  what appears on screen.
- BIOS ROMs at 0xC0000–0xFFFFF — these are not RAM at all; writing to these
  physical addresses writes to ROM (ignored) or causes undefined behaviour.

Protecting the first 1 MB is a hard floor enforced in `pmm_init()`:
```c
if (free_start < 256) free_start = 256;
```

---

## 7. Getting `kernel_end` from the Linker

The PMM needs to know where the kernel image ends so it does not allocate any
page the kernel occupies. We use a **linker symbol** to get this:

In `linker.ld`:
```
kernel_end = .;
```

The dot (`.`) is the linker's location counter — it holds the address of the
current position in the output binary. Placing this after all sections captures
the first byte past the end of the kernel.

In C:
```c
extern char kernel_end;         /* NOT a variable — a symbol */
uint32_t end = (uint32_t)&kernel_end;   /* take its ADDRESS */
```

The key subtlety: `kernel_end` is a symbol, not a variable. It has no storage.
Its **address** is the end-of-kernel value. Writing `kernel_end` (without `&`)
would try to read a `char` from that address, which is the first byte of free
memory — wrong. We always use `&kernel_end`.

---

## 8. `pmm_alloc_page()` — How the Scan Works

```
for each byte i in bitmap[]:
    if bitmap[i] == 0: skip (all 8 pages used)
    for each bit b in bitmap[i]:
        if bit is set:
            page = i * 8 + b
            clear the bit (mark used)
            return page * PAGE_SIZE
return NULL (out of memory)
```

This is a **first-fit** allocator: it always returns the lowest-numbered free
page. That means:

- Allocations march upward through physical memory.
- After freeing and re-allocating, the freed pages are reused before moving
  higher (freed pages come back at lower addresses than newer allocations).
- There is no fragmentation problem at this level — every allocation is exactly
  one page. Fragmentation only becomes relevant in the heap (Module 8).

**Time complexity**: O(n) in the number of pages in the worst case (all pages
used except the last). In practice, most memory is free at this stage of the
kernel, so the first non-zero byte is found very quickly.

---

## 9. `pmm_free_page()` — Returning a Page

```c
void pmm_free_page(void *addr) {
    uint32_t page = (uint32_t)addr / PMM_PAGE_SIZE;
    if (page >= total_pages) return;       /* out of range */
    if (page_is_free(page))   return;      /* double-free guard */
    page_set_free(page);
    used_count--;
}
```

Two guards:
1. **Range check** — the caller might pass a wild pointer.
2. **Double-free check** — freeing an already-free page is silently ignored.
   In a more advanced kernel you would panic here (double-free is a serious
   bug), but silent ignore is safer for early development.

---

## 10. Why We Don't Use the BIOS E820 Memory Map (Yet)

A production OS would call BIOS interrupt `0x15, EAX=0xE820` in real mode to
get an accurate map of all usable memory regions, then pass that map to the
kernel so the PMM knows exactly what is usable and what is memory-mapped
hardware.

We skip this in Module 6 because:
- Our bootloader already exits real mode before entering C, so we cannot call
  BIOS interrupts from the kernel.
- QEMU's default is 128 MB of clean, contiguous RAM starting at 0x00100000
  (1 MB), which makes a hardcoded assumption safe for our purposes.
- The E820 map requires parsing a variable-length structure passed via a pointer
  — adding that complexity now would distract from learning the PMM itself.

**Decision**: hard-code 128 MB (`128 * 1024 * 1024`) for Module 6. Module 7
(paging) or Module 8 (heap) could add E820 if needed; it is a self-contained
change to `pmm_init()`.

---

## 11. NULL as "Out of Memory"

`pmm_alloc_page()` returns `(void *)0` (NULL) when no pages are free.

This is safe because physical address 0x00000000 (page 0) is always marked
USED (it is in the first-1MB reserved floor). So the allocator will never
legitimately return address 0. Returning 0 unambiguously means "out of memory"
with no ambiguity.

---

## 12. File Structure

| File              | Purpose |
|-------------------|---------|
| `pmm.h`           | Public API: `pmm_init`, `pmm_alloc_page`, `pmm_free_page`, stats |
| `pmm.c`           | Bitmap, bit helpers, alloc/free implementation |
| `linker.ld`       | Same as Module 5, plus `kernel_end = .` symbol |
| `kernel.c`        | Initialises PMM, runs 3-step allocation demo |
| `vga.c/h`         | Carried forward from Module 3 |
| `gdt.c/h`         | Carried forward from Module 4 |
| `pic.c/h`, `io.h` | Carried forward from Module 5 |
| `idt.c/h`         | Carried forward from Module 5 |
| `isr.c/h/asm`     | Carried forward from Module 5 |

---

## 13. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| Page size | 4 KB | Matches x86 MMU hardware; universal standard |
| Bit meaning | 1 = FREE, 0 = USED | BSS is zero → safe default before init; fast byte-skip scan |
| Bitmap in BSS | `static uint8_t bitmap[]` | No heap yet; compiler places it safely; zeroed automatically |
| Max tracked RAM | 128 MB (32,768 pages, 4 KB bitmap) | Matches QEMU default; bitmap is tiny |
| 1 MB floor | `free_start = max(free_start, 256)` | Never return VGA/BIOS pages; safe hard floor |
| `kernel_end` | Linker symbol, take its address | Only way to know kernel size at link time |
| Memory size | Hardcoded 128 MB | No E820 yet; QEMU has 128 MB by default; keeps focus on PMM |
| Double-free | Silently ignore | Safer than panic for early development |
| NULL = OOM | Return `(void*)0` | Page 0 is always reserved so 0 unambiguously means failure |
| First-fit | Scan from byte 0 | Simple, correct, no fragmentation at page granularity |

---

## 14. What We Proved By Getting This Working

- A flat bitmap tracks thousands of pages in just 4 KB.
- The bit = 1 / BSS-zero trick avoids any explicit initialisation loop.
- The 1 MB floor reliably protects the hardware-reserved region.
- The linker can export symbols that C code reads as addresses.
- `pmm_alloc_page()` returns page-aligned addresses that increase through RAM.
- Freed pages are reused on the very next allocation (first-fit reuse, Demo C).
- `used_count` stays consistent through alloc/free cycles.
- The IDT is still live — any bug that causes a CPU exception (e.g., accessing
  a NULL pointer) prints a panic rather than silently corrupting state.

---

## 15. What Comes Next (Component 7 Preview)

Component 7 builds **Virtual Memory / Paging** — the CPU feature that maps
virtual addresses (what code uses) to physical addresses (what RAM actually is).

With paging we can:
- Give every process its own private virtual address space.
- Map the kernel at a high virtual address while it lives at a low physical one.
- Mark pages non-writable to protect the kernel from accidental overwrites.

Paging requires **page tables** — data structures that describe the mapping.
Each page table itself occupies a physical page. That is exactly what the PMM
provides: `pmm_alloc_page()` hands the paging module the physical pages it
needs to build its own tables. The two modules fit together precisely.
