# B02 — Renderer Sandbox: Design Decisions

## 1. Whitelist vs. Blacklist (Only Allow Known-Safe)

**Decision:** The sandbox uses a whitelist: every syscall is BLOCKED by default unless it appears in the allowed table.

**Why:** A blacklist — blocking only known-dangerous calls — fails as soon as an attacker finds a syscall not on the list. The set of "dangerous" syscalls is large, overlapping, and grows as the kernel gains new features. A whitelist inverts the assumption: the renderer only has access to the minimal set of calls it provably needs. Everything else is denied without the developer needing to think about it. This is the same philosophy behind seccomp-bpf in Chromium.

**Trade-off:** The whitelist must be maintained carefully. When new renderer functionality is added, a developer must explicitly add the syscall to the whitelist — otherwise the feature breaks in sandbox mode. Chromium's seccomp-bpf filter runs in WARN mode before it runs in ENFORCE mode precisely to catch missing whitelist entries before they ship.

---

## 2. Broker for Privileged Operations (Not Full Block)

**Decision:** `open()` is not simply blocked — it is forwarded to a separate broker process that applies its own path policy.

**Why:** The renderer has legitimate reasons to open files: cached resources, fonts, WebAssembly bytecaches. Blocking `open()` outright would break these. The broker provides a controlled gateway: it receives the path, checks it against a whitelist of allowed prefixes (`/tmp/`, `/var/cache/browser/`), and either calls `open()` itself and returns the file descriptor (via `SCM_RIGHTS` in a real implementation) or denies the request. This way the renderer never gains `open()` capability but can still access the files it genuinely needs.

**Trade-off:** The broker is itself an attack surface. If an attacker controls the renderer and can send arbitrary `OPEN:` messages to the broker, they can open any file the broker permits. The broker must be hardened independently, and its own allowed-paths list is a critical security boundary.

---

## 3. Why `open()` Specifically Needs Broker Mediation

**Decision:** `open()` is singled out as BROKER while `read()` and `write()` are ALLOW.

**Why:** `open()` is the syscall that traverses the filesystem namespace and returns a new file descriptor. A renderer that can call `open()` freely can read `/etc/passwd`, `/proc/self/mem`, private key files, or browser profile data — the entire user's filesystem. `read()` and `write()` operate on already-open file descriptors; the risk is the fd itself, not the operation. By blocking `open()` at the whitelist level and allowing `read()`/`write()` through to the fd-check layer, we separate "can you name a new resource" from "can you access one you already have."

**Trade-off:** We still need to control which file descriptors the renderer inherits at startup. If the browser process leaks an fd for a sensitive file before exec()ing the renderer, the renderer can `read()` it without ever calling `open()`. This is why Chromium closes all non-essential fds in the renderer before it enters the sandbox (using `O_CLOEXEC` and an explicit close loop).

---

## 4. Comparison to Real seccomp-bpf

**Decision:** This demo simulates the sandbox as a library call rather than using real kernel-level enforcement.

**Why:** The conceptual model is identical to Linux seccomp-bpf: install a BPF filter that inspects each syscall's number and arguments and returns a disposition (ALLOW / TRAP / KILL). The broker pattern maps to `SECCOMP_RET_TRAP` + a `SIGSYS` handler that forwards to a broker. Using a simulated version lets the demo run on any POSIX system without root, and makes the whitelist table and dispatch logic visible in readable C rather than BPF bytecode.

**Trade-off:** The simulated sandbox is not enforced by the kernel — a renderer in the same process could bypass it by calling the real `open()` syscall directly. Real seccomp-bpf intercepts at the kernel system-call entry point, so bypass is impossible even for hostile code. The conceptual lesson (whitelist + broker) transfers directly; the enforcement mechanism requires kernel support.

---

## 5. Intentional Bug: Write to fd=42 Bypasses the fd Check

**Decision:** The `SYS_WRITE` whitelist entry is marked ALLOW, but the fd validation inside `do_write()` only checks `fd == 1` and `fd == 42`. A write to fd=42 succeeds even though it should be denied.

**Why:** This illustrates the real-world class of "whitelist gap" bugs. The developer thought "write is safe if it goes to stdout" but forgot to handle the case where the renderer inherits other open fds. An inherited fd=42 could be a socket connected to an internal service. A compromised renderer that learns the fd number can exfiltrate data by writing to it directly — the sandbox approved `write` in general, so the operation passes.

**Trade-off:** The fix is to add explicit fd range validation: only allow writes to fd < 3 (stdin/stdout/stderr), or even more narrowly to exactly fd=1. This is the kind of subtle policy error that real sandbox audits look for. In Chromium's seccomp-bpf filter, `write()` is allowed but only on specific fd ranges; other fds go through the broker or are blocked.
