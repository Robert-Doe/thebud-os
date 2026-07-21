# B7 — JS Interpreter: Design Decisions

## 1. Tagged Union for JS Values (type + union)

**Decision:** Every `js_value_t` carries both a `val_type_t type` tag and a union of possible payloads.

**Why:** JavaScript is dynamically typed — the same variable can hold a number, then a string, then null. The type tag tells the engine which union member is valid. Without it, the engine would have no way to know whether the bits in the union represent a float, a pointer, or something else. Every major JavaScript engine (V8, SpiderMonkey, JavaScriptCore) uses some variant of this pattern (often called NaN-boxing or pointer-tagging for performance, but conceptually identical).

**Trade-off:** Every value carries the overhead of the tag. NaN-boxing encodes the tag into unused bits of IEEE 754 NaN values, eliminating that overhead, but makes the code harder to read. For this demo, clarity matters more than performance.

---

## 2. Tree-Walking Interpreter vs Bytecode / JIT

**Decision:** Evaluate the AST directly (tree-walking) rather than compiling to bytecode or machine code.

**Why:** A tree-walker needs no compiler backend, no register allocator, no bytecode format. It can be written in ~200 lines and still evaluates correct programs. The goal of B7 is to demonstrate the *type system* and the *type confusion vulnerability*, not execution performance.

**Trade-off:** Tree-walking is 10–100× slower than a bytecode interpreter and many orders of magnitude slower than a JIT. V8 uses a multi-tier pipeline: interpreter → Maglev (mid-tier JIT) → Turbofan (optimising JIT). B8 covers the JIT tier.

---

## 3. Type Confusion: What It Means When the Type Tag Can Be Forged

**Decision:** Demonstrate type confusion by constructing a `js_value_t` tagged `VAL_NUMBER` whose bit pattern is a valid pointer, then reading it as `.u.string` without the tag check.

**Why:** This is the *exact* operation a type confusion exploit performs: the attacker causes the engine to skip the type check for a value they control, so the engine treats attacker-chosen bits as a pointer to an object of a different type. The demo makes the mechanism concrete and dereference-safe by pointing at a local buffer rather than a wild address.

**Trade-off:** The demo slightly simplifies V8's actual mechanism (which involves the hidden Map pointer, shape transitions, and JIT-inlined type checks) but captures the essential pattern: wrong-type dereference of attacker-controlled bits.

---

## 4. Why JavaScript Engines Must Validate Types at Every Property Access

**Decision:** `eval_node` checks `v.type` before using the union for every value; `val_to_string` and friends abort if the type is wrong.

**Why:** JIT compilers cache type information to avoid repeated checks. When the cache is valid this is safe and fast. The vulnerability arises when the cache becomes stale (due to a bug in type feedback, shape transitions, or GC) and the JIT-emitted check is skipped. An engine that never caches types and always checks is safe but slow. The CVEs live at the intersection of "optimise by skipping checks" and "but the optimisation assumption can be violated."

**Trade-off:** Always checking is safe; caching and skipping is fast but exploitable if the cache can be corrupted. Modern V8 uses "trusted space" and sandbox isolation to limit what an attacker can do even after winning the type confusion race.

---

## 5. From Type Confusion to Arbitrary Read/Write — The CVE Pattern

**Decision:** The `confusion_demo_run()` function walks through the full escalation: tag mismatch → pointer dereference → data leak.

**Why:** CVE-2021-21220 and CVE-2022-1096 both follow this three-step pattern:
1. **Trigger**: cause the JIT to emit code that skips a type check for an attacker-controlled value.
2. **Infoleak**: use the confused object as a FixedArray with an attacker-controlled `.length`, enabling out-of-bounds reads that leak pointer values from the heap.
3. **Arbitrary access**: use the leaked pointers with the `addrof`/`fakeobj` primitives (see B10) to read/write anywhere in the V8 heap, then escalate to full process memory access.

**Trade-off:** The demo stops at the infoleak stage (step 2) because full exploitation requires the heap layout knowledge demonstrated in B9 and B10. The chain is completed in B12.
