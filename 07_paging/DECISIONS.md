# Component 7: Virtual Memory / Paging — Deep Dive

---

## 1. Where We Left Off

Component 6 gave us the PMM: a way to hand out 4 KB physical pages on demand.
But the CPU still addresses RAM directly — every pointer in the kernel is a raw
physical address. That works while there is only one program, but it becomes a
problem the moment we want multiple processes: two processes could both try to
use the same physical address and corrupt each other.

Component 7 turns on **paging** — the CPU's hardware mechanism that adds an
indirection layer between the addresses code uses (virtual) and the addresses
that go out on the memory bus (physical). After paging is enabled:

- Every load/store goes through a hardware address translation.
- Two processes can have the same virtual address but point to different physical pages.
- The kernel can protect its own pages from user-mode access.
- An unmapped virtual address triggers a page fault (#PF) that the OS can handle.

We start with the simplest valid configuration: a single identity map for the
first 4 MB, so existing kernel code continues to work at the same addresses.

---

## 2. The Two Problems Paging Solves

### Problem 1 — Address Space Isolation
Without paging, process A at address 0x5000 and process B at address 0x5000 are
the **same physical memory cell**. Either can overwrite the other. With paging,
each process has its own page directory. 0x5000 in process A's address space maps
to physical page X; 0x5000 in process B's space maps to physical page Y. They
never interact.

### Problem 2 — Protection
The CPU enforces the flags in every page table entry. A page marked not-writable
fires a #PF if code tries to write to it. A page marked supervisor-only fires a
#PF if Ring 3 code accesses it. These checks happen in **hardware** on every
single memory access — the OS does not need to police them in software.

---

## 3. The Two-Level Page Table

x86 uses a two-level table to translate 32-bit virtual addresses:

```
Virtual address (32 bits):
┌──────────────┬──────────────┬──────────────┐
│  Dir [31:22] │ Table [21:12]│ Offset [11:0]│
└──────────────┴──────────────┴──────────────┘
     10 bits         10 bits        12 bits
```

**Level 1 — Page Directory (PD)**:
One table per address space. 1024 entries × 4 bytes = 4 KB. Each entry (PDE)
points to a Page Table.

**Level 2 — Page Table (PT)**:
One per 4 MB region. 1024 entries × 4 bytes = 4 KB. Each entry (PTE) points to
a 4 KB physical page.

**Translation steps**:
1. CPU extracts bits 31–22 (10 bits) → PD index (0–1023).
2. PD entry gives the physical address of a Page Table.
3. CPU extracts bits 21–12 (10 bits) → PT index (0–1023).
4. PT entry gives the physical address of the 4 KB page.
5. CPU adds bits 11–0 (12-bit offset within the page) → final physical address.

**Coverage**: 1024 PD entries × 1024 PT entries × 4 KB = **4 GB** total virtual
address space. Exactly the full 32-bit range.

---

## 4. Page Directory Entry (PDE) and Page Table Entry (PTE) Format

Both are 32-bit values with the same bit layout:

```
Bit 31-12  Physical address (page table for PDE; physical frame for PTE)
           Must be 4 KB aligned (bits 11-0 are always 0 in the address itself)
Bit 11-9   Available for OS use (we leave 0)
Bit 8      Global (ignore in this module)
Bit 7      Page Size (PDE only): 0 = 4 KB PT, 1 = 4 MB page (we always use 0)
Bit 6      Dirty: CPU sets when the page is written (PTE only)
Bit 5      Accessed: CPU sets when the entry is read
Bit 4      Cache Disable: 0 = normal cached access
Bit 3      Write-Through: 0 = write-back caching
Bit 2      User/Supervisor: 0 = kernel only (Ring 0), 1 = user accessible
Bit 1      Writable: 0 = read-only, 1 = read/write
Bit 0      Present: 0 = entry invalid (access → #PF), 1 = valid entry
```

For our kernel pages we use `PAGE_PRESENT | PAGE_WRITABLE` = `0x3`.

A PDE for a page table at physical address `pt`:
```c
pde = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITABLE;
```

A PTE for identity-mapping virtual page N to physical page N:
```c
pte = (N * 0x1000) | PAGE_PRESENT | PAGE_WRITABLE;
```

---

## 5. Identity Mapping — Why and How

An **identity map** sets virtual address == physical address for a range of memory.

**Why we need it**: when paging is turned on, the CPU immediately starts
translating every address — including the address of the very next instruction.
If that instruction's virtual address is not mapped, the CPU fires a #PF and the
machine crashes. Our kernel code lives at physical 0x1000; if we mapped virtual
0x1000 to some other physical page, the CPU would fetch garbage code after
enabling paging.

The identity map solution: for the first 4 MB (the region containing all our
current kernel code, data, stack, VGA buffer, and PMM bitmap) we make the page
table say "virtual page N → physical page N". After paging is on, every address
the kernel currently holds remains valid.

**Coverage**: 4 MB = 1024 pages. One page table (1024 PTEs) covers it exactly.
Our kernel binary + stack + VGA buffer are all well within the first 4 MB:
- Kernel code: starts at 0x1000, well under 0x400000
- VGA buffer: 0xB8000 < 0x400000 ✓
- Stack: ~0x90000 < 0x400000 ✓
- PMM bitmap: in BSS, just after kernel_end, still < 0x400000 ✓

---

## 6. Zeroing Page Tables Before Use

The PMM hands out pages without zeroing them — a fresh physical page may contain
stale data from a previous allocation. For a page table this is dangerous: a
stale non-zero PDE with the Present bit accidentally set (`bit 0 = 1`) would make
the CPU follow a garbage physical address the moment any virtual address in that
4 MB range is accessed.

We must zero every page table and the page directory before installing them.
Since we have no `memset` yet, `zero_page()` uses a simple loop:

```c
static void zero_page(uint32_t *p) {
    for (int i = 0; i < 1024; i++) p[i] = 0;
}
```

Called immediately after each `pmm_alloc_page()` in `paging_init()`.

---

## 7. Loading CR3 and Setting the PG Bit

**CR3 (Page Directory Base Register)**:
A special CPU register that holds the physical address of the active page
directory. When the CPU translates a virtual address, it starts by reading CR3
to find the page directory. Writing CR3 also flushes the TLB (see §8).

```c
__asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys));
```

**Enabling paging — bit 31 of CR0**:
Setting the PG bit (bit 31) of CR0 turns paging on. From this exact instruction
forward, every memory access is translated through the page tables.

```c
__asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
cr0 |= 0x80000000u;
__asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
```

We read CR0 first because it contains other important flags (PE = bit 0, etc.)
that we must not clobber. We OR in just bit 31 and write back.

**The critical moment**: the instruction immediately after `mov %0, %%cr0`
executes in paged mode. The CPU fetches it by translating its virtual address.
Because we identity-mapped that virtual address before flipping the PG bit, the
fetch succeeds and execution continues normally.

---

## 8. The TLB (Translation Lookaside Buffer)

Walking the two-level page table on every single memory access would be very
slow: two extra RAM reads (PDE + PTE) per access. The CPU caches recent
translations in a small hardware cache called the **TLB**.

- TLB hit: translation is served from cache in ~1 cycle.
- TLB miss: CPU walks the page tables (2 RAM reads), caches the result.

The TLB must be invalidated when:
- You change a page table entry (the cached old translation would be wrong).
- You switch to a different page directory (a different process).

**Writing CR3** flushes the entire TLB. We do this once in `paging_init()`.
In later modules (process switching, page remapping) we will need to flush again
either by re-writing CR3 or using the `invlpg` instruction (flushes one entry).

---

## 9. What Happens to Unmapped Addresses

Any virtual address whose PDE is 0 (not present) triggers a **Page Fault (#PF,
vector 14)**. Our IDT is already installed with a handler for vector 14 — it
calls `kernel_panic()` and dumps all registers.

This is actually useful during development: if a bug causes us to access an
unmapped address (NULL dereference, out-of-bounds pointer), we get a clean crash
with the exact EIP and CR2 (the faulting address, which the CPU stores in CR2 on
a #PF) rather than silent memory corruption.

In Module 7, only the first 4 MB (PDE[0]) is mapped. Accessing any virtual
address from 0x00400000 upward produces a page fault.

---

## 10. Why Not Map All 128 MB Now?

We could allocate 32 page tables (32 × 4 KB = 128 KB from the PMM) and fill
PDE[0]-PDE[31] to cover all 128 MB. We don't, because:

1. **Module focus**: Module 7's goal is to understand and prove that paging
   works. Mapping only 4 MB achieves that goal with less code.
2. **Later modules don't need it**: the heap (Module 8) and keyboard driver
   (Module 9) all live in the first 4 MB.
3. **Demand mapping**: in a real OS you map pages lazily — only when first
   accessed. Wiring up all 128 MB at boot is wasteful.

When Module 8 (heap) needs more address space, we will map additional regions
by allocating more page tables from the PMM.

---

## 11. File Structure

| File              | Purpose |
|-------------------|---------|
| `paging.h`        | Public API: `paging_init()`, flag constants, diagnostic getters |
| `paging.c`        | Two-level table setup, CR3 load, CR0 PG bit, `zero_page()` |
| `kernel.c`        | Initialises all subsystems; demo proves paging works post-init |
| `linker.ld`       | Same as Module 6 (kernel_end symbol) |
| `pmm.c/h`         | Carried forward from Module 6 |
| `vga.c/h`         | Carried forward from Module 3 |
| `gdt.c/h`         | Carried forward from Module 4 |
| `pic.c/h`, `io.h` | Carried forward from Module 5 |
| `idt.c/h`         | Carried forward from Module 5 |
| `isr.c/h/asm`     | Carried forward from Module 5 |

---

## 12. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| Two-level table | 1024 PDE × 1024 PTE | Matches x86 hardware; 4 GB address space with 2 × 4 KB tables |
| Identity map | First 4 MB only | Enough to cover all current kernel code/data; simple; one page table |
| PTE flags | PRESENT + WRITABLE for all | Kernel needs read+write everywhere; USER bit left 0 (ring 0 only) |
| Zero page tables | Manual loop | PMM doesn't zero; stale Present bits would follow garbage addresses |
| CR3 load | `mov %0, %%cr3` inline asm | No C instruction for CR3; volatile prevents GCC reordering |
| CR0 PG bit | Read-modify-write | Must preserve other CR0 flags (PE, WP, etc.) |
| PG bit first or CR3 first | CR3 first | CPU uses CR3 immediately after PG is set; it must be valid before paging starts |
| Unmapped addresses | Left as not-present (PDE = 0) | Access → #PF; IDT handles it with panic dump |
| Map 4 MB not 128 MB | 1 page table | Module focus; later modules add more mappings as needed |

---

## 13. What We Proved By Getting This Working

- A two-level page table fits entirely in 8 KB (two PMM pages).
- Zeroing page tables before use prevents phantom present entries.
- Writing CR3 then setting the PG bit is the correct enabling sequence.
- Identity mapping the first 4 MB makes paging transparent to all existing code.
- The VGA driver, PMM bitmap, and kernel stack all continue to work after paging
  is enabled, because their physical addresses are within the identity-mapped range.
- The #PF handler (IDT vector 14) is live — any unmapped access will show a
  clean register dump rather than a silent triple-fault.
- The PMM correctly shows two extra pages used (for the PD and the PT).

---

## 14. What Comes Next (Component 8 Preview)

Component 8 builds the **Heap Allocator** — `kmalloc()` and `kfree()` for the
kernel. The heap sits in virtual memory above the kernel binary and grows
upward. When the heap needs more backing memory it calls `pmm_alloc_page()` to
get a physical page and then maps it into the heap's virtual address range using
the paging system we just built.

This is the first time Module 6 (PMM) and Module 7 (paging) work together as a
pair: the heap asks the PMM for a page, then asks the paging system to wire that
physical page into a virtual address where the heap can use it.
