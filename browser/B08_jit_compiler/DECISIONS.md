# B8 — JIT Compiler: Design Decisions

## 1. Single-Pass Emitter (No IR, No Optimization)

**Decision:** The JIT emits x86 instructions directly from expressions — no intermediate representation, no register allocation, no optimization passes.

**Why:** The goal is to demonstrate JIT spray and W^X, not to build a production compiler. A single-pass emitter can be understood in one reading and still produces real, executable machine code. Every concept (constant embedding, instruction encoding, page permissions) is visible without layers of abstraction.

**Trade-off:** A production JIT (V8's Turbofan, LLVM) uses SSA form, multiple optimization passes, and register allocation. Those layers are where most JIT performance gains come from — but they are also where most JIT bugs hide (incorrect type assumptions, missed deopt points). B8 shows the security properties of the emitter layer; the optimization layer is a separate concern.

---

## 2. JIT Spray: Why Constant Encoding Creates Attacker-Controlled Executable Bytes

**Decision:** Demonstrate that `ADD EAX, 0x90909090` places four `0x90` (NOP) bytes directly inside an executable code page, under attacker control.

**Why:** The x86 instruction set encodes immediate constants inline in the instruction stream. When the JIT compiles `x + ATTACKER_CONSTANT`, the attacker's value appears verbatim in executable memory. By choosing the constant carefully (e.g., NOP sleds, or sequences that look like useful gadgets when the CPU enters at a misaligned offset), the attacker plants executable payloads inside legitimate JIT pages.

**Trade-off:** Modern architectures with 32-bit immediates make this straightforward. On AArch64, constants are split or loaded indirectly, reducing the attack surface but not eliminating it. Constant blinding (XOR with a random mask that is itself unknown to the attacker) is the standard mitigation; V8 and SpiderMonkey both implement it.

---

## 3. W^X: Never Executable AND Writable Simultaneously

**Decision:** Demonstrate the full page-permission lifecycle: allocate RW, write shellcode, mprotect to RX, execute, show that write now fails.

**Why:** W^X is a hardware-enforced policy (NX bit on x86, XN bit on ARM). Without it, any write primitive trivially becomes a code-execution primitive. With it, the attacker must additionally find a way to mark pages executable (usually requiring kernel-level access or a mprotect call they control) or must resort to code-reuse attacks (ROP). This is one of the most impactful mitigations in the history of exploit development.

**Trade-off:** JIT compilers are the primary *intentional* exception to W^X — they must write code and then execute it. The transition window (briefly RW before switching to RX) is a target. Modern JITs minimize this window and never leave pages permanently RWX.

---

## 4. Why JIT Compilers Are High-Value Targets

**Decision:** Explain that attacker-controlled JIT input (JavaScript source) directly influences the bytes of emitted machine code.

**Why:** Most software takes untrusted *data* as input; JIT compilers take untrusted data and emit *executable code* from it. The attacker has direct influence over what ends up in executable memory — both the code's logic (via JIT bugs) and its byte content (via constant encoding). This makes JIT compilers uniquely powerful attack surfaces. The history of browser exploitation is dominated by JIT vulnerabilities (V8, SpiderMonkey, JSC).

**Trade-off:** JIT compilation is also why browsers are fast. Removing the JIT (as Apple did with iOS at certain points) is a significant regression. The mitigations (constant blinding, guard pages, sandboxing) are the best available without abandoning JIT entirely.

---

## 5. Modern Mitigations: Constant Blinding, Guard Pages, Randomized NOPs

**Decision:** Show constant blinding implementation alongside the spray demo.

**Why:** Three mitigations directly address JIT spray:
1. **Constant blinding**: XOR every immediate with a random mask before emission; XOR again at runtime to recover the value. The attacker cannot predict the bytes in JIT memory.
2. **Guard pages**: Unmapped pages between JIT regions. A NOP sled that crosses a guard page crashes instead of sliding into the next region.
3. **Randomized NOPs**: Insert random 1–3 byte sequences (NOPs or equivalent) at random offsets, disrupting predictable spray alignment.

**Trade-off:** Constant blinding adds 1–2 extra instructions per constant and minor code-size overhead. Guard pages waste virtual address space. Randomized NOPs add code size and slight performance cost. All are worth it for preventing spray.
