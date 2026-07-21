# B10 — Type Confusion CVE Reproduction: Design Decisions

## 1. Why Type Tags Must Be Immutable from JavaScript

**Decision:** In the engine, `obj->type` can only be set at allocation time. JavaScript code has no direct way to change it — it must go through the engine's property set operations, which enforce type consistency.

**Why:** If JavaScript could directly write the type tag of an object (e.g., `obj.__type__ = 3`), every V8 security assumption would collapse. JavaScript must not be able to forge type tags. The type tag lives in the C++ object's header, inaccessible from JavaScript except through a JIT bug that treats the object's backing storage as readable/writable at an offset that overlaps the header.

**Trade-off:** Keeping type tags truly immutable requires that the JIT never emits code that writes to an object's header field based on user-controlled data. This is harder than it sounds: JIT optimisations that inline object construction, copy-on-write shapes, and deoptimisation checkpoints all touch object headers. The CVEs found bugs in exactly these transitions.

---

## 2. The JIT Optimization That Skips Type Checks

**Decision:** Implement both `engine_get_element_safe` (always checks) and `engine_get_element_confused` (skips check) to show the difference explicitly.

**Why:** The JIT's type-check elimination works like this: if the JIT has seen an object accessed as an Array 1000 times, it emits code that assumes the object is still an Array and skips the tag check — because the check costs a branch and a memory load. This is correct as long as the assumption holds. The bug is when the assumption can be violated *after* the JIT has committed the no-check code.

**Trade-off:** Eliding type checks is a major source of JIT performance (10–30% on typical workloads). The alternative — always checking — is safe but slow. Modern V8 uses "type feedback" to deoptimize back to the interpreter when an assumption is violated; the CVEs were cases where the deoptimisation was not triggered quickly enough.

---

## 3. From Type Confusion to Infoleak: Getting a Pointer into JavaScript

**Decision:** Demonstrate the `addrof()` primitive explicitly — show that type confusion allows reading a C++ heap pointer as a JavaScript number.

**Why:** `addrof()` is the first critical step after type confusion. In JavaScript, all numbers are doubles (64-bit IEEE 754). Object pointers are 64-bit integers. With type confusion, the attacker can read a slot that holds a pointer and have the engine interpret it as a number — the pointer value becomes visible as a JavaScript number. This breaks ASLR because the attacker now knows where real objects live in memory.

**Trade-off:** V8's pointer compression (introduced in V8 8.0, 2020) encodes heap pointers as 32-bit offsets within a 4 GB cage. This means leaking a pointer gives an offset, not an absolute address. The attacker still needs a separate leak to find the cage base. This raised the bar but did not eliminate infoleak primitives.

---

## 4. From Infoleak to Arbitrary Write: addrof/fakeobj Primitives

**Decision:** Implement both primitives as simple pointer casts and demonstrate that combining them achieves arbitrary memory write.

**Why:** `fakeobj()` is the inverse of `addrof()`: write an attacker-controlled integer into a slot that the engine reads back as an object pointer. Now the engine treats attacker-chosen memory as a legitimate JavaScript object. The attacker structures that memory to look like an Array with a huge length, then reads/writes elements — hitting arbitrary memory. Combined: addrof leaks a pointer, fakeobj turns a crafted address into an object, OOB element access reads/writes anything.

**Trade-off:** These two primitives together constitute "arbitrary memory access" — the attacker can read or write any address in the V8 heap. In V8's sandbox model, this is still limited to the V8 heap cage. Escaping to OS-level memory requires an additional vulnerability (see B12).

---

## 5. Mitigation: Pointer Compression, Type Check Hardening, Sandbox

**Decision:** Explain the three-layer mitigation strategy that V8 deployed after the 2021/2022 wave of type confusion CVEs.

**Why:**
1. **Pointer compression**: Pointers are 32-bit offsets in a 4 GB heap cage. An infoleak gives an offset, not an absolute address, limiting ASLR bypass.
2. **Type check hardening**: V8 added explicit type checks even in JIT fast-paths, with hardened guard conditions that cannot be skipped by the branch predictor.
3. **V8 sandbox** (introduced 2022): Even with arbitrary R/W inside V8's heap, the attacker cannot access memory outside the sandbox without a separate vulnerability. External pointers (to OS resources) are replaced with indices into a sandboxed table.

**Trade-off:** The V8 sandbox adds ~5% overhead from indirection through the external pointer table. Pointer compression recovers some of that with smaller data sizes. Type check hardening has ~2-3% overhead on microbenchmarks. All three are enabled in production Chrome.
