# B11 — Spectre: Design Decisions

## 1. Why Out-of-Order Execution + Cache Is Observable

**Decision:** Use RDTSC before and after a memory access to distinguish L1 cache hits (~4 cycles) from DRAM accesses (~200+ cycles).

**Why:** Modern CPUs execute instructions out of order and speculatively. When a speculation proves wrong, the CPU rolls back *architectural* state (registers, flags) — but it does not roll back the *microarchitectural* state (cache contents, branch predictor state, TLB entries). The cache is observable from the same process via timing: if probe_array[secret * 512] is warm, the speculative code loaded it, revealing the value of `secret` even though the CPU "never executed" that code architecturally.

**Trade-off:** Cache timing is inherently noisy. Prefetchers, cache eviction, OS scheduling, and other processes all cause jitter. Multiple rounds with majority-vote scoring (the `scores[]` array) increase reliability. Real-world Spectre exploits needed hundreds to thousands of measurements per byte. In a browser, this noise is amplified by JavaScript's ~100µs timer resolution — but still achievable with enough samples.

---

## 2. Flush+Reload: The Fundamental Technique

**Decision:** Implement flush+reload as three explicit steps: flush probe_array, call victim_fn (speculative access), time each probe slot.

**Why:** Flush+Reload is the most powerful cache side-channel technique:
- **Flush**: `CLFLUSH` evicts the target cache line, ensuring we start from a known cold state.
- **Victim executes**: The victim's speculative access loads probe_array[secret * 512] into cache.
- **Reload + Time**: Iterate all 256 probe slots, time each. The one below the threshold was loaded.

The 512-byte stride ensures each probe slot is in a different cache line (cache lines are typically 64 bytes), preventing false positives from adjacent-line prefetching.

**Trade-off:** Flush+Reload requires the attacker and victim to share memory (shared library, shared memory segment). In the browser case, they share the renderer process. Prime+Probe (no shared memory required) is an alternative that works across processes but is noisier.

---

## 3. Branch Predictor Training — Why Speculation Follows Past Behavior

**Decision:** Train the branch predictor by calling victim_fn with valid indices TRAIN_ROUNDS times before issuing the malicious index.

**Why:** The CPU's branch predictor maintains a history table indexed by the branch's address. After seeing `if (x < array_size)` taken 100 times in a row, the predictor confidently predicts "taken" on the next call — even with a malicious OOB `x`. The CPU speculatively executes the body, accessing probe_array[secret * 512], before discovering x >= array_size and rolling back.

**Trade-off:** Branch predictor training is the most fragile part of Spectre. If the predictor history is flushed (by a context switch, a deliberate IBPB instruction, or enough other branches), the attack fails to speculate. IBRS/IBPB (Indirect Branch Restriction Speculation) microcode mitigations do exactly this — flush the predictor on privilege level changes.

---

## 4. Why JavaScript in Browsers Can Mount Spectre (SAB Timer + Cache)

**Decision:** Include a detailed explanation of the SharedArrayBuffer timer technique.

**Why:** The key insight is that high-resolution timing is sufficient to distinguish cache hits from misses. JavaScript's standard `performance.now()` was 5µs resolution in 2018 — not enough. But a SharedArrayBuffer shared with a Worker thread provides a counting timer: Worker thread increments a counter in a tight loop; main thread reads the counter. This gives ~5ns resolution — more than enough. An attacker can mount Spectre entirely from JavaScript without any native code or browser bug.

**Trade-off:** The SharedArrayBuffer timer trick required SharedArrayBuffer (for shared memory across workers) and atomics (for the counter). Disabling SAB (January 2018) immediately removed the reliable high-resolution timer. Later mitigations (COOP + COEP headers) force cross-origin isolation, re-enabling SAB only for sites that opt into same-process constraints — meaning cross-origin content is never co-resident, so even a working Spectre PoC can't read cross-origin data.

---

## 5. Mitigations: Site Isolation, Timer Reduction, Jitter, SSBD

**Decision:** Cover all four mitigation classes with their trade-offs.

**Why:**
1. **Site isolation**: Cross-origin content in separate OS processes. Even with Spectre, you can only read your own process. Cost: ~10% more RAM, more IPC overhead.
2. **Timer reduction**: `performance.now()` reduced to 100µs resolution, then 5µs with cross-origin isolation. Harder to distinguish cache hit from miss.
3. **Jitter**: Random ±0–20µs added to `performance.now()`. Attacker needs more samples to average out jitter.
4. **SSBD (Speculative Store Bypass Disable)**: Prevents speculative reads of stale store data. Costs 10-30% on some workloads.

**Trade-off:** Site isolation is the most reliable mitigation (architectural, not just making the attack harder). Timer reduction and jitter raise the bar but don't prevent well-engineered attacks. SSBD is effective but expensive — Chrome opts into SSBD per renderer process.

---

## 6. Why Spectre Is "Unfixable" at the Microarchitecture Level

**Decision:** Explain this directly rather than implying a software patch will suffice.

**Why:** Spectre exploits a fundamental contract of the architecture: "program execution is deterministic and observable state matches architectural state." Out-of-order execution with caching breaks this — microarchitectural state diverges from architectural state during speculation. Fixing Spectre requires either:
- Not speculating at security boundaries (breaks the performance contract).
- Partitioning the cache at security boundaries (requires hardware redesign, ~30% perf cost).
- Process isolation (today's pragmatic choice).

Intel and AMD have shipped hardware mitigations in newer CPUs (eIBRS, BHI mitigations) that close specific Spectre gadgets, but the fundamental class of vulnerability remains as long as CPUs use speculative execution with a shared cache.
