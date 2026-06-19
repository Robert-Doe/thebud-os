# Module 22 — Demand Paging: Design Decisions

## 1. Region table in PCB — no malloc, no global VA space tree

**Decision:** Each process PCB holds `vm_regions[MAX_REGIONS]`, a fixed-size array of `struct vm_region`.  `SYS_MMAP` records the reservation there; the physical frame is not allocated until the first fault.

**Why:** A proper VM area tree (Linux's `vm_area_struct` red-black tree) requires dynamic allocation, a comparator, and O(log n) lookups.  For at most MAX_REGIONS=8 entries a linear scan is O(8) — effectively constant time — and requires zero allocator calls.  Embedding the table in the PCB also means fork can copy all regions by copying the struct (no pointer chasing).

**Trade-off:** The process can only have 8 anonymous regions.  Removing this limit requires a heap-allocated tree, deferred to a future module.

## 2. Physical frame allocated on first fault, not at mmap() time

**Decision:** `do_mmap` records only a `vm_region` entry (base, length, prot).  No page table entries are created.  `process_fault_handler` calls `paging_set_user_page(cr3, fault_addr)` on the first access to any page in the region.

**Why:** This is the definition of demand paging: pages are populated lazily.  A process that reserves 64 MB but only touches 1 MB should only consume 1 MB of physical RAM.  Allocating on mmap() would defeat the purpose and be no better than malloc.

**Trade-off:** Each first access to a new page causes a fault (kernel transition + allocation).  Subsequent accesses are fault-free.  The cost is one-time per page.

## 3. mmap base starts at 0xC00000 — above user stack (0x800000), below kernel

**Decision:** `next_mmap_base` begins at 0xC00000 and grows upward.  User code lives at 0x400000, user stack at 0x800000.  Kernel is mapped by PDE[0] (0-4MB supervisor-only).

**Why:** 0xC00000 is above the fixed user stack page and below any kernel addresses.  This leaves a clean 4 MB gap between the stack and the mmap region, preventing accidental aliasing of stack growth with mmap allocations.

**Trade-off:** The mmap area is bounded above by the kernel boundary.  In practice, with 64 MB of physical RAM and a 32-bit address space, this gives ~52 MB of potential mmap space — sufficient for educational use.

## 4. Anonymous mappings only — no file-backed mmap

**Decision:** `SYS_MMAP` creates only anonymous (zero-filled on demand) pages.  There is no fd argument, no offset, no file mapping.

**Why:** File-backed mmap requires integrating the VFS layer (open file, read block on fault) and page cache invalidation — a significant subsystem.  Anonymous mapping is the simpler and more commonly used form (malloc, stack growth, shared memory).  File-backed mmap is deferred to Module 25+.

**Trade-off:** Programs that use mmap to load shared libraries (dynamic linking) cannot be supported yet.  This is acceptable because BobOS uses statically-linked ELF binaries.

## 5. On fork, child inherits region table — demand pages are faulted in independently

**Decision:** `process_fork` copies `current->vm_regions[]` into the child's PCB.  If the child accesses an mmap'd address that was never faulted in by the parent, the child's fault handler will allocate a fresh frame for the child.  CoW applies to pages that were already faulted in before the fork.

**Why:** The region table describes the virtual address space layout, which must be the same in parent and child immediately after fork.  Frames that exist at fork time are CoW-shared (Module 21).  Frames that did not exist yet are independently demand-paged in by whichever process first touches them — clean and correct.

**Trade-off:** If both parent and child fault in the same previously-unmapped page, they each get a private zero-filled frame with no sharing.  This is correct but slightly wasteful compared to a full zero-page optimization (Linux maps all unfaulted pages to a shared read-only zero page and CoW's on write).
