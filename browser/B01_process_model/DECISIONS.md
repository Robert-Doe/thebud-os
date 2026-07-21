# B01 — Multi-Process Browser Architecture: Design Decisions

## 1. Separate Processes for Browser Kernel and Renderer (Not Threads)

**Decision:** The browser kernel and renderer run as separate OS processes connected by pipes, not as threads within one process.

**Why:** Threads share an address space. A memory-corruption bug in the renderer (buffer overflow, use-after-free from malicious HTML) can overwrite kernel data structures if they live in the same process. Separate processes have separate virtual address spaces enforced by the MMU, so a compromised renderer cannot read or modify kernel memory at all — the OS enforces the boundary, not our code.

**Trade-off:** Process creation and context switching are more expensive than thread creation. IPC through pipes adds latency compared to in-process function calls. Chromium accepts this cost because the security gain is worth it; the rendering pipeline is already bandwidth-heavy, so the relative overhead of IPC is small.

---

## 2. IPC Over Pipes (Not Shared Memory)

**Decision:** This demo uses POSIX pipes (`pipe()`, `read()`, `write()`) for all message passing between the renderer and browser kernel.

**Why:** Pipes provide a clear, auditable communication channel. Every message passes through the kernel's scheduler, giving us a natural place to log and inspect traffic. The read/write API is sequential and easy to reason about. Shared memory, by contrast, would require the browser kernel to trust that the renderer has written well-formed data to a shared buffer — a renderer exploit could then corrupt in-flight messages.

**Trade-off:** Pipes are slower than shared memory for bulk data (image bitmaps, texture uploads). Real Chromium uses a combination: small control messages go through a Mojo IPC channel (similar to pipes), while large payloads (compositor frames, audio buffers) go through shared memory with careful size validation before the kernel touches the data.

---

## 3. The Broker Pattern — Renderer Never Makes Direct Syscalls for Privileged Ops

**Decision:** The renderer asks the browser kernel for every privileged operation (network access, file open, cookie read). The kernel is the sole issuer of those syscalls.

**Why:** This is the broker pattern from the Chromium sandbox design. If the renderer were allowed to call `open()`, `socket()`, or `connect()` directly, a compromised renderer would have full access to the filesystem and network under the user's account. By routing through the kernel, the kernel can apply URL policy, certificate checking, and CSP rules before any I/O happens. The renderer's syscall surface is reduced to reading and writing its own pipe ends.

**Trade-off:** Every privileged operation requires a round-trip across the IPC channel. This adds latency to, for example, the first paint of a resource-heavy page. Modern browsers mitigate this with speculative prefetching — the browser process begins network requests before the renderer even asks for them.

---

## 4. Principle of Least Privilege — Renderer Has No Direct Network Access

**Decision:** The renderer process is denied network access. `SOCKET` and raw `NAVIGATE` requests are blocked; the kernel performs network I/O on behalf of the renderer after applying security policy.

**Why:** Least privilege means each component can only access resources it legitimately needs for its job. The renderer's job is layout and painting, not networking. If it could open sockets directly, a script-injection vulnerability would give an attacker a full outbound network channel to exfiltrate data. The renderer only needs to receive already-fetched resource bytes from the kernel; it does not need the ability to choose where to connect.

**Trade-off:** The renderer cannot optimise its own network scheduling (e.g., it cannot open a speculative TCP connection). All network optimisation logic must live in the browser kernel, which must be smart enough to predict what the renderer will need next (this is what resource hints — `<link rel=prefetch>` — are for).

---

## 5. The Browser Process Is the "Kernel" of the Browser

**Decision:** We name the trusted process `browser_kernel` because it plays an analogous role to an OS kernel.

**Why:** An OS kernel mediates all access to hardware resources (disk, network, screen). The browser kernel mediates all access to browser resources (cookies, localStorage, network, the GPU surface). Renderer processes are analogous to user-space processes: they get a restricted environment and must ask the kernel for anything privileged. This mental model clarifies why Chromium spends enormous effort hardening the browser process — it is the trusted computing base for every site running inside the browser.

**Trade-off:** Unlike an OS kernel, the browser kernel runs in user space and therefore has the same privileges as the user who launched the browser. A browser-kernel exploit is therefore still a full user-account compromise. This is why modern browsers add a second layer — OS-level sandboxing (seccomp on Linux, job objects on Windows) that restricts even the renderer's ability to make raw syscalls.
