# Module 21 — Copy-on-Write Fork: Design Decisions

## 1. Lazy CoW: pages are shared read-only after fork, copied only on first write

**Decision:** `paging_clone_address_space` no longer allocates new physical frames for user pages.  Instead it copies the PTE values from parent to child, clears the write bit in both PTEs, and calls `pmm_incref` on each shared frame.  The physical copy is deferred until the #PF handler fires on a write.

**Why:** This is the classic Unix optimization that makes fork cheap.  In the fork+exec pattern (the most common use of fork) the child immediately calls exec and never writes the inherited pages at all — the copy is never needed.  Even in fork+modify patterns, only written pages pay the copy cost.

**Trade-off:** Every write to a shared page now incurs a #PF, a mode switch, a frame allocation, and a memcpy.  For write-heavy workloads immediately after fork, CoW can be slower than eager copy.  In practice exec is called immediately, so the trade-off is almost always a win.

## 2. Reference count stored in PMM: central location prevents double-free

**Decision:** `pmm_refcounts[]` in pmm.c tracks a per-frame reference count.  `pmm_incref` and `pmm_decref` are the only callsites that modify it.  `pmm_decref` frees the frame when the count reaches zero.

**Why:** If each page table owned its own refcount, freeing an address space would require scanning all other page tables to find co-owners — O(n processes × n pages).  Central storage makes incref/decref O(1).  It also prevents the double-free bug where two page-table entries point at the same frame and both try to call `pmm_free_page`.

**Trade-off:** An extra 16 KB of BSS (16384 bytes for 64 MB / 4 KB frames).  Negligible on any real machine.

## 3. On sole-owner write fault: restore write bit without copying

**Decision:** In `paging_cow_fault`, if `pmm_getref(frame) <= 1`, the process is the only owner of this frame.  The write bit was cleared by fork but the frame is private — we can just restore `PAGE_WRITABLE` without allocating a new frame.

**Why:** This happens when the parent writes to a page that the child has already exited and freed.  Without this check, the parent would wastefully allocate a new frame and copy data that is already private.  The optimisation also makes the common single-process case (exec'd child) allocation-free on write.

**Trade-off:** None of significance.  The check is a single comparison.

## 4. CoW only for user pages (PDE index > 0): kernel pages are never written by user

**Decision:** `paging_clone_address_space` applies the CoW write-protection only to PTEs in PDE[1+].  PDE[0] (the kernel region) is still copied as supervisor-only PTEs pointing to the same physical frames — unchanged from Module 17.

**Why:** User processes cannot write kernel pages (U/S bit is clear in PDE[0]).  Applying CoW to kernel pages would be pointless (no write fault ever fires on them from ring-3) and potentially dangerous (a kernel write that triggers a #PF inside the fault handler would cause a double fault).

**Trade-off:** None — this is strictly correct behavior.

## 5. Fork is now O(PT count) instead of O(frame count)

**Decision:** The loop in `paging_clone_address_space` allocates one new PT per PDE entry, but allocates zero new data frames.

**Why:** Before CoW, fork was O(N) where N is the number of mapped pages (could be hundreds for a large process).  With CoW it is O(K) where K is the number of page tables (at most 1023).  In our demos K=1 (one user PDE), so fork is effectively O(1) for data frames.

**Trade-off:** The page-table structures are still fully copied (not CoW'd themselves).  This could be further optimised with a CoW page directory, but the complexity is not justified at the educational level.

## 6. paging_free_address_space uses pmm_decref instead of pmm_free_page

**Decision:** When a process exits, each user frame is released via `pmm_decref` rather than `pmm_free_page`.  Only when the refcount reaches zero does `pmm_decref` actually return the frame to the free pool.

**Why:** If two processes share a CoW frame and one exits, the frame must not be freed — the other process still has a PTE pointing at it.  `pmm_decref` handles this automatically.  Using `pmm_free_page` directly would corrupt the other process's address space.

**Trade-off:** `pmm_decref` is a slightly more expensive call than `pmm_free_page` (one extra comparison).  Negligible.
