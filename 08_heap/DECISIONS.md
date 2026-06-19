# Component 8: Heap Allocator — Deep Dive

---

## 1. Where We Left Off

Modules 6 and 7 gave us two memory primitives:
- **PMM** (`pmm_alloc_page`): hands out 4 KB physical pages — too coarse for
  general use (you wouldn't allocate a 4 KB page just to store a 12-byte struct).
- **Paging**: maps virtual → physical, with the first 4 MB identity-mapped.

We still have no way to allocate **arbitrary-sized** objects at runtime. Every
time a kernel subsystem needs a small dynamic buffer it would have to waste an
entire 4 KB PMM page. Component 8 fixes that with a **heap allocator**:
`kmalloc(size)` / `kfree(ptr)`.

The heap sits between the PMM (which manages pages) and the rest of the kernel
(which wants bytes). It takes a pool of pages from the PMM once at init time and
then sub-divides them into arbitrarily-sized allocations.

---

## 2. What a Heap Allocator Does

A heap is a region of memory managed by the allocator. The allocator:
- Tracks which parts of the region are in use and which are free.
- `kmalloc(n)` — finds a free region of at least `n` bytes, marks it used,
  returns a pointer to it.
- `kfree(p)` — marks the region at `p` free again for future allocations.

The key challenge is doing this without using any external memory for bookkeeping
— the metadata must live **inside the heap pool itself**.

---

## 3. The Block Header

Every region of memory in the heap — whether free or used — starts with an 8-byte
**block header**:

```c
struct block_header {
    uint32_t size;    /* bytes of DATA (not including this header) */
    uint32_t flags;   /* bit 0: 1 = used, 0 = free                 */
};
```

Headers are stored inline, immediately before the data they describe:

```
┌──────────────────────────────────────────────┐
│ header (8B) │ ← → data (size bytes) → ← ← ← │
└──────────────────────────────────────────────┘
              ↑ kmalloc returns this pointer
```

To traverse the heap, start at `heap_start` and step forward by
`sizeof(header) + block->size` to reach the next block. This is a classic
**implicit free list** — every block is reachable by linear scan from the start.

The last entry in the pool is a **sentinel** — a header with `size=0, flags=USED`.
It is never allocated. It tells the traversal loop "you've reached the end."

---

## 4. The Sentinel

Without an end marker, the traversal loop would walk past the end of the pool
into unknown memory — undefined behaviour at best, a crash at worst.

The sentinel is placed at the very end of the pool during `heap_init()`:

```
┌─────────┬──────────────────────────┬─────────┐
│ hdr (8B)│    free data pool        │ sentinel│
│         │   (pool_size - 2×8 B)   │  (8B)  │
└─────────┴──────────────────────────┴─────────┘
```

Traversal stops when it reaches a header with `size == 0 && flags == USED`.
No allocation can produce such a header (size is always > 0, and a free zero-size
block would be useless), so the sentinel is unambiguous.

---

## 5. kmalloc — First-Fit Search and Splitting

```
Walk blocks from heap_start:
  if sentinel: break (out of memory)
  if block is FREE and block->size >= requested_size:
    if block can be split (leftover >= header + MIN_SPLIT):
      insert a new FREE header partway through the block
    mark the block USED
    return pointer to data (block + 1)
  move to next block
return NULL
```

**First-fit**: return the first block that is large enough, not the best fit.
Simple and fast; acceptable fragmentation for a kernel heap.

**Splitting**: if a free block is much larger than requested, splitting it avoids
wasting the excess. After splitting:

```
Before: [ hdr:512B FREE ][ ... 512 bytes ... ]
After:  [ hdr:16B  USED ][ 16 bytes ][ hdr:488B FREE ][ 488 bytes ]
                          ↑ caller gets this
```

The `MIN_SPLIT` threshold (16 bytes) prevents creating header-sized free blocks
that are too small to be useful.

**4-byte alignment**: `kmalloc` rounds the requested size up to the next multiple
of 4. This ensures all returned pointers are 4-byte aligned — required for safe
`uint32_t` access and generally expected by C code.

---

## 6. kfree — Mark Free + Forward Coalesce

```
Get header = (struct block_header*)ptr - 1
Mark header FREE
next = block after this one
while next is FREE and not sentinel:
    merge: current->size += sizeof(header) + next->size
    advance next
```

**Why coalesce?** Without merging, repeated alloc/free of small objects creates
many tiny free blocks that individually can't satisfy a large allocation — even
though the total free space would be enough. This is **fragmentation**.

**Forward coalescing** merges a newly freed block with the immediately following
free block(s). The `while` loop handles chains of consecutive free blocks.

Example — before freeing a3 (a2 and a4 already freed):
```
[a1:USED][a2:FREE][a3:USED][a4:FREE][a5:USED][sentinel]
```
After `kfree(a3)` with forward coalesce:
```
[a1:USED][a2:FREE merged with a3 merged with a4][a5:USED][sentinel]
```
One large free block instead of three small ones.

**Backward coalescing** (merging with the previous block) is harder because the
implicit free list has no backward pointers. We skip it in Module 8; it would
require a doubly-linked list or boundary tags. The forward coalesce alone is
sufficient to demonstrate the concept and prevents the most common fragmentation
pattern (freeing in order).

---

## 7. Why a Pool of PMM Pages, Not a Bump Allocator

The simplest possible allocator is a **bump allocator**: keep a pointer, advance
it by `size` on each allocation, never free anything. Used in early boot where
nothing ever needs to be freed.

We don't use it because `kfree` must work: the keyboard driver (Module 9) and
process scheduler (Module 10) will allocate and free structs. A bump allocator
would exhaust the heap immediately.

We also don't call `pmm_alloc_page()` on every `kmalloc()` because:
- A `kmalloc(12)` that allocated a full 4 KB page would waste 4084 bytes.
- Mapping new virtual pages on every small allocation would require calling back
  into the paging module — coupling two modules unnecessarily.

The pool approach is the right tradeoff: pay the PMM cost once at heap init,
then sub-divide cheaply for the lifetime of the kernel.

---

## 8. Heap Placement in the Address Space

The heap pool comes from `pmm_alloc_page()`. The PMM's first-fit allocator
returns pages starting at the first free page above `kernel_end`, which is
somewhere in the 0x00100000–0x003FFFFF range — well within the identity-mapped
first 4 MB from Module 7.

This means the physical addresses returned by the PMM are also valid virtual
addresses (virtual == physical). No extra page-table entries are needed for
the heap. This is a direct consequence of Module 7's identity mapping decision.

---

## 9. The 16-Page Pool Choice

`heap_init(16)` gives us 16 × 4 KB = 64 KB of heap space. After subtracting
the two header bytes (first block header + sentinel): 64 KB − 16 B ≈ 65,520
bytes available.

This is more than enough for all the remaining modules (keyboard driver, process
structs, shell buffers). The number is easy to increase — just pass a larger
`num_pages` to `heap_init()`.

---

## 10. File Structure

| File              | Purpose |
|-------------------|---------|
| `heap.h`          | API: `heap_init`, `kmalloc`, `kfree`, diagnostic getters |
| `heap.c`          | Block header, sentinel, kmalloc (first-fit+split), kfree (coalesce) |
| `kernel.c`        | 4-part demo: basic alloc, write/read, free/reuse, coalescing |
| `paging.c/h`      | Carried forward from Module 7 |
| `pmm.c/h`         | Carried forward from Module 6 |
| All prior modules | Carried forward unchanged |

---

## 11. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| Allocator type | Free-list (implicit), first-fit | Simple, correct, teaches the core concept; no external data structures needed |
| Metadata location | Inline block headers | No separate array needed; headers travel with their blocks |
| Sentinel | Size=0, flags=USED at end | Unambiguous end marker; no bounds check arithmetic needed in loop |
| Header size | 8 bytes (two uint32_t) | Minimal; 4-byte aligned; leaves 4 bytes spare for future use |
| Minimum split | 16 bytes | Prevents creating headers smaller than MIN_SPLIT + header = 24 bytes |
| Coalescing | Forward only | Simpler than doubly-linked; prevents the most common fragmentation pattern |
| Alignment | Round up to 4 bytes | Required for safe uint32_t access; expected by C compiler |
| Pool source | PMM pages at init | Pay cost once; avoid per-alloc page mapping; pool fits in identity-mapped range |
| Pool size | 16 pages (64 KB) | Sufficient for all remaining modules; easy to change |

---

## 12. What We Proved By Getting This Working

- Inline block headers require no external data structures.
- The sentinel reliably terminates traversal at the pool boundary.
- First-fit find + split produces correctly sized allocations.
- Returned pointers are distinct and non-overlapping.
- Allocated memory is genuinely writable and retains values.
- `kfree` + forward coalesce merges adjacent free blocks back into one.
- After freeing everything, the heap returns to a single block — zero fragmentation.
- The PMM + paging + heap trio cooperates correctly end-to-end.

---

## 13. What Comes Next (Component 9 Preview)

Component 9 adds the **Keyboard Driver** — reading keystrokes from the keyboard
controller via IRQ1 (vector 33 after PIC remapping). The driver will:
- Register an IRQ1 handler with `irq_register(1, keyboard_handler)`.
- Read the scancode from I/O port 0x60 on each interrupt.
- Translate scancodes to ASCII characters via a lookup table.
- Buffer keypresses in a small ring buffer (`kmalloc`'d from the heap).
- Expose `keyboard_getchar()` so the shell (Module 13) can read input.

This is the first module where the heap is genuinely used by another subsystem —
the ring buffer is a small dynamic allocation whose size is known only at runtime.
