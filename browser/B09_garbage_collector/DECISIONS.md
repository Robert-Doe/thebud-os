# B9 — Garbage Collector: Design Decisions

## 1. Mark-and-Sweep vs Reference Counting

**Decision:** Implement mark-and-sweep rather than reference counting.

**Why:** Reference counting cannot collect cycles. Two objects that reference each other will never reach a count of zero, even if no external roots exist. JavaScript programs create cycles constantly (DOM nodes referencing event listeners that close over the node, etc.). Mark-and-sweep handles cycles naturally: it traces from roots and anything not reached is unreachable regardless of whether it is in a cycle. V8 uses a generational mark-and-sweep (incremental, concurrent) for exactly this reason.

**Trade-off:** Reference counting has a major advantage: objects are freed deterministically at the point they become unreachable, which means no "stop the world" pause. Swift, Rust's `Rc`, and Python all use reference counting (with cycle detectors as a fallback). Mark-and-sweep requires either pausing the program to scan the heap or complex concurrent/incremental machinery. Our demo pauses; real GCs (V8's Orinoco) do not.

---

## 2. Why Raw C Pointers Escape the GC — The Root Cause of UAF

**Decision:** Show explicitly that `char *raw_ptr = A->data` is invisible to the GC's mark phase.

**Why:** The GC only knows about references it can see: the `roots[]` array and the `refs[]` slots inside each object. A raw C pointer (`char *`, `void *`) stored in a local variable, a struct field the GC doesn't scan, or as a byte pattern inside a data buffer is completely invisible. When the GC frees A and raw_ptr is still live, the pointer is now dangling. The GC cannot "fix up" raw pointers — it doesn't know they exist.

**Trade-off:** This is the fundamental tension of mixing a garbage-collected language (JavaScript) with a C/C++ runtime. The GC can guarantee safety for JS-level accesses, but C++ code in the engine itself uses raw pointers everywhere for performance. Any raw pointer that escapes the GC's knowledge is a potential UAF. Rust's ownership model eliminates this class by making lifetime management part of the type system.

---

## 3. Heap Grooming: Controlling the Allocator to Place Attacker's Object at Freed Address

**Decision:** Explain heap grooming as a technique separate from the UAF itself, showing it is a second attacker-controlled step.

**Why:** A UAF alone is not enough — the attacker also needs to control what occupies the freed memory. Heap grooming achieves this by:
1. Spraying many identically-sized allocations after triggering the free.
2. The allocator (using a free-list or slab allocator) places one of those allocations at the freed address.
3. If the sprayed objects are attacker-controlled (e.g., JavaScript ArrayBuffers with attacker-chosen content), the stale pointer now reads/writes attacker data.

**Trade-off:** This requires the attacker to know the object size and to have a way to allocate many objects of that size from JavaScript. PartitionAlloc separates allocation pools by type, breaking the "spray JS object into freed C++ object" step.

---

## 4. Why Most Browser CVEs Are UAF

**Decision:** Dedicate a section of the demo to explaining the structural reason for UAF dominance.

**Why:** In a pure GC language (like Java with no JNI), UAF is impossible — the GC prevents collection while any reference exists. In a pure C/C++ program, raw pointers are the norm and UAF is a constant risk. Browsers occupy an uncomfortable middle ground: JavaScript is GC-safe, but the JavaScript engine, DOM, layout engine, and GPU compositor are all C++ with raw pointers. Every C++/JS boundary is a potential point where a raw pointer escapes GC tracking. There are thousands of such boundaries in Chromium.

**Trade-off:** Rewriting the entire engine in a memory-safe language (Rust, Swift) would eliminate UAF but is a multi-decade project. Chrome's MiraclePtr (wrapping raw pointers with quarantine checks) and memory-safe Rust components are incremental steps toward this goal.

---

## 5. Mitigation: PartitionAlloc (Type-Separated Heaps)

**Decision:** Explain PartitionAlloc as the primary structural mitigation rather than patching individual UAF bugs.

**Why:** PartitionAlloc segregates objects by type (DOM nodes in one partition, JS strings in another, ArrayBuffers in a third). A freed DOM node can only be replaced by another DOM node from the same partition. This means:
- An attacker cannot spray JS ArrayBuffers into freed DOM node memory.
- Type confusion across partition boundaries is prevented at the allocator level.
- Even without a fix for the UAF itself, the exploitation step (grooming with a different type) fails.

**Trade-off:** PartitionAlloc adds allocator complexity and some memory overhead (each partition has its own free lists). It does not prevent UAF within the same type partition — but same-type UAF is much harder to exploit because the attacker cannot choose arbitrary field layouts.
