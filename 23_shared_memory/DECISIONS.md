# Module 23 — Shared Memory: Design Decisions

## 1. Key-based lookup — matches POSIX sysV shm_get semantics

**Decision:** `shm_get(key, size)` searches `shm_table[]` for an existing entry with the same key before allocating.  If found, it returns the existing id without re-allocating frames.

**Why:** Keys are the POSIX mechanism for unrelated processes to agree on a shared segment without passing file descriptors.  The same key seen by parent and child (after fork) refers to the same physical frames.  If we always allocated on every call, two processes using the same key would get different physical memory.

**Trade-off:** Key collision is not handled — the first process to call shm_get(42, ...) owns key 42.  A production system would use uid-scoped keys or a pathname-based key derivation (ftok).

## 2. Fixed max 4 pages per segment — avoids variable-length allocation

**Decision:** `struct shm_segment` holds `phys_frames[4]` — at most 4 physical pages (16 KB) per segment.

**Why:** Variable-length arrays in a fixed-size PCB (or global table) require heap allocation, adding complexity.  4 pages covers the common use cases: a shared ring buffer, a semaphore array, or a small control block.  The array is in BSS, so unused entries cost only address space.

**Trade-off:** Programs needing a large shared heap (e.g., 1 MB) cannot use this mechanism directly.  They would need multiple segments or a resizable design, deferred to future modules.

## 3. paging_map_user_phys separate from paging_set_user_page

**Decision:** A new function `paging_map_user_phys(cr3, virt, phys)` installs a caller-supplied physical address into a page table.  `paging_set_user_page` (existing) allocates a fresh frame.

**Why:** Shared memory requires mapping the same frame into multiple page directories — the frame address is determined by shm_get, not by the MMU.  Merging the two behaviors into one function would require an extra parameter and conditional logic that obscures what each caller actually needs.

**Trade-off:** Two separate functions with overlapping structure.  The code duplication is minor (both walk the PD and PTE structures).

## 4. pmm_incref on each frame so they survive partial process exit

**Decision:** `shm_get` calls `pmm_incref(frame)` on every allocated frame.  When a process exits, `paging_free_address_space` calls `pmm_decref` on each user frame — the shm frames survive because their refcount is > 1.

**Why:** Without incref, the first process to exit would decref the frame to 0 and free it back to the PMM.  The second process would then hold a dangling PTE pointing at a reallocated frame — a silent use-after-free.  pmm_incref prevents this at the cost of one counter increment per frame per segment.

**Trade-off:** The shm frames are never freed (no shm_dt/shm_rm equivalent in this module).  They exist until the OS reboots.  A cleanup API is deferred to a later module.

## 5. After fork, child does NOT inherit the shm mapping — must call shmat explicitly

**Decision:** `process_fork` copies `vm_regions[]` (demand-paged anonymous regions) but does NOT copy shm page table entries into the child.  The child must call `SYS_SHMAT` with the same shm_id to attach.

**Why:** POSIX specifies that shm attachments are not inherited across fork.  Child inheriting the mapping silently would mean the child could access shared memory it never explicitly requested -- surprising behavior.  Requiring an explicit `shmat` also gives the child control over which virtual address to use for the mapping.

**Trade-off:** The demo explicitly calls `SYS_SHMAT` in both parent and child, which is more verbose but correct and educational.

## 6. Shared memory + timing = cache side-channel foundation

**Decision:** This module deliberately creates a scenario where two processes can observe each other's memory writes through a shared physical frame.

**Why:** This is the physical substrate for Spectre variant 1 and Flush+Reload attacks.  The parent writes a secret value (0xCAFE); an attacker process that maps the same frame can read it directly (in this simplified model) or, in a hardened OS, infer it through cache timing.  Making this explicit in the module teaches the reader where hardware covert channels come from and why shared memory is a security boundary.

**Trade-off:** We expose the mechanism without providing the mitigation.  The discussion of cache isolation, KPTI, and memory fencing belongs in a dedicated security module.
