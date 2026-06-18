# BobOS — Full Build Roadmap

Each module gets its own directory, full source, and a `DECISIONS.md` explaining
every design choice before code is written.  We test each module in QEMU before
moving on.

---

## Track 1 — OS Kernel (Modules 1–24)

### Phase 1: Bare Metal Foundation (1–9) ✅

| # | Module | What it proves | Directory | Status |
|---|--------|----------------|-----------|--------|
| 1 | **Bootloader** | First code the CPU runs; loads the kernel from disk | `01_bootloader/` | ✅ Done |
| 2 | **Kernel Entry** | Switches CPU to 32-bit Protected Mode; sets up C environment | `02_kernel_entry/` | ✅ Done |
| 3 | **VGA Text Driver** | Prints text to screen; `kprintf` for all future output | `03_vga_driver/` | ✅ Done |
| 4 | **GDT** | Defines memory segments; first hardware table the OS owns | `04_gdt/` | ✅ Done |
| 5 | **IDT & Interrupts** | Catches CPU exceptions and hardware IRQs | `05_idt_interrupts/` | ✅ Done |
| 6 | **Physical Memory Manager** | Bitmap allocator; tracks which 4KB RAM pages are free | `06_pmm/` | ✅ Done |
| 7 | **Virtual Memory / Paging** | Maps virtual → physical; enables the MMU | `07_paging/` | ✅ Done |
| 8 | **Heap Allocator** | `kmalloc` / `kfree` with splitting and coalescing | `08_heap/` | ✅ Done |
| 9 | **Keyboard Driver** | IRQ1 scancode → ASCII ring buffer | `09_keyboard/` | ✅ Done |

### Phase 2: Processes & I/O (10–15) ✅

| # | Module | What it proves | Directory | Status |
|---|--------|----------------|-----------|--------|
| 10 | **Processes & Scheduler** | PCBs, context switching, round-robin preemption | `10_processes/` | ✅ Done |
| 11 | **System Calls** | `int 0x80` gate; SYS_EXIT / SYS_WRITE / SYS_GETPID / SYS_YIELD | `11_syscalls/` | ✅ Done |
| 12 | **File System (BobFS)** | ATA PIO driver; custom flat FS on a raw disk image | `12_filesystem/` | ✅ Done |
| 13 | **Shell** | Interactive CLI process that ties everything together | `13_shell/` | ✅ Done |
| 14 | **User Mode (Ring 3)** | TSS, DPL=3 GDT entries, ring-3 process creation | `14_usermode/` | ✅ Done |
| 15 | **Per-Process Address Spaces** | Separate PDs per process, U/S isolation, graceful #PF kill | `15_address_spaces/` | ✅ Done |

### Phase 3: Real Programs (16–18) ✅

| # | Module | What it proves | Directory | Status |
|---|--------|----------------|-----------|--------|
| 16 | **ELF Loader** | Parse ELF32 binaries from BobFS; map segments into a fresh address space; `sys_exec`; user programs live on disk, not in the kernel image | `16_elf_loader/` | ✅ Done |
| 17 | **`fork` & `exec`** | `SYS_FORK` deep-copies address space; `SYS_EXEC` replaces it with an ELF from disk; `SYS_WAIT` for parent to reap child | `17_fork_exec/` | ✅ Done |
| 18 | **Signals** | `SYS_KILL`, `SYS_SIGNAL`; deliver SIGSEGV / SIGKILL asynchronously to a process; user-installed signal handlers called on return from kernel | `18_signals/` | ✅ Done |

### Phase 4: Advanced Memory (19–22) ✅

| # | Module | What it proves | Directory | Status |
|---|--------|----------------|-----------|--------|
| 19 | **VFS Layer** | Abstract filesystem interface (open / read / write / close / stat); BobFS becomes one backend; stdin/stdout/stderr as fd 0/1/2 | `19_vfs/` | ✅ Done |
| 20 | **Pipes & IPC** | `SYS_PIPE`; anonymous kernel ring buffer connecting two processes; foundation for shell pipelines `ls \| cat` | `20_pipes/` | ✅ Done |
| 21 | **Copy-on-Write Fork** | Mark shared pages read-only on fork; #PF handler copies the faulting page on first write; makes `fork` O(1) instead of O(address space size) | `21_cow/` | ✅ Done |
| 22 | **Demand Paging** | Reserve virtual pages without committing physical frames; allocate on first #PF; `SYS_MMAP` for anonymous mappings | `22_demand_paging/` | ✅ Done |

### Phase 5: Concurrency (23–24) ✅

| # | Module | What it proves | Directory | Status |
|---|--------|----------------|-----------|--------|
| 23 | **Shared Memory** | `SYS_SHMGET` / `SYS_SHMAT`; map the same physical pages into two address spaces; covert channel demo; foundation for Spectre B11 | `23_shared_memory/` | ✅ Done |
| 24 | **Threads** | Multiple execution contexts sharing one CR3 / address space; per-thread kernel stacks; `SYS_THREAD_CREATE` / `SYS_THREAD_EXIT`; scheduler treats threads and processes uniformly | `24_threads/` | ✅ Done |

---

## Track 2 — Browser Internals & Security (B1–B18)

Builds on the OS track.  Each module maps directly back to an OS concept you
already implemented.  See the OS↔Browser connection column.

### Phase B1: Process Model & Isolation (B1–B4)

| # | Module | What it proves | OS connection | Status |
|---|--------|----------------|---------------|--------|
| B1 | **Multi-Process Browser Architecture** | Browser process (kernel) + renderer process (user); IPC channel between them; why Chrome has 10+ processes | Module 10, 17 | ⬜ |
| B2 | **Renderer Sandbox** | Seccomp-style syscall whitelist on the renderer; all disallowed calls go through the broker; sandbox escape = finding a hole in the whitelist | Module 11 | ⬜ |
| B3 | **Same-Origin Policy Engine** | Origin parsing and enforcement owned by the browser process; renderer asks, browser decides; why the renderer never makes trust decisions | Module 11, 15 | ⬜ |
| B4 | **Site Isolation** | One renderer process per origin; cross-origin iframes cannot share memory; process registry maps origin → CR3 | Module 15, 21 | ⬜ |

### Phase B2: Rendering Pipeline (B5–B8)

| # | Module | What it proves | OS connection | Status |
|---|--------|----------------|---------------|--------|
| B5 | **HTML Tokenizer & DOM** | Tokenizer → DOM tree; parser differentials that create mutation XSS; why two parsers seeing the same bytes build different trees | Module 3 (output) | ⬜ |
| B6 | **CSS Cascade & Layout** | Style computation, box tree, pixel coordinates; CSS timing side-channels (`@font-face`, scroll-to-text-fragment) | Module 7 (memory layout) | ⬜ |
| B7 | **JS Interpreter** | Tree-walking interpreter; JS heap separate from DOM; type confusion: convincing the engine an integer is a pointer | Module 8, 10 | ⬜ |
| B8 | **JIT Compiler** | Single-pass x86 code emitter into an executable page; JIT spray; W^X enforcement; why executable pages must never be writable | Module 7, 22 | ⬜ |

### Phase B3: Memory Safety (B9–B10)

| # | Module | What it proves | OS connection | Status |
|---|--------|----------------|---------------|--------|
| B9 | **Garbage Collector** | Mark-and-sweep GC; use-after-free: raw pointer to a freed object; root cause of most browser CVEs | Module 6, 8 | ⬜ |
| B10 | **Type Confusion** | Type tag on every JS value; deliberately break it; reproduce the class of bug behind V8 CVE-2021-21220, CVE-2022-1096 | Module 7, B7 | ⬜ |

### Phase B4: Exploit Chain (B11–B12) — Payoff Modules

| # | Module | What it proves | OS connection | Status |
|---|--------|----------------|---------------|--------|
| B11 | **Spectre PoC** | SharedArrayBuffer + `performance.now()` cache timing channel between two origins; reproduce why SAB was disabled in 2018; test timer mitigations | Module 23 | ⬜ |
| B12 | **Renderer Exploit → Sandbox Escape** | Controlled type confusion → arbitrary r/w in renderer → IPC escape attempt to browser process; full exploit chain walkthrough | B2, B9, B10 | ⬜ |

### Phase B5: Protocol Security (B13–B18)

| # | Module | What it proves | OS connection | Status |
|---|--------|----------------|---------------|--------|
| B13 | **Content Security Policy** | Parse and enforce CSP headers; find three bypasses in a misconfigured policy; why `unsafe-inline` and wildcards collapse the policy | B3, B5 | ⬜ |
| B14 | **CORS & Fetch** | CORS preflight and response header enforcement from scratch; `null` origin danger; CORB and CORP on top | B3 | ⬜ |
| B15 | **HTTP/1.1 Parser** | Request/response parser with connection reuse; HTTP request smuggling: disagreement between your parser and a proxy on where a request ends | Module 12 (disk I/O) | ⬜ |
| B16 | **TLS Handshake** | TLS 1.3 state machine (crypto primitives from a library); certificate validation; SNI; what a MitM can and cannot see | Module 16 (ELF loading = parsing) | ⬜ |
| B17 | **Cookie & Session Model** | `Set-Cookie` parsing; `SameSite` enforcement; `HttpOnly` / `Secure` flags; CSRF attack against missing `SameSite`; fix with correct flag | B3, B14 | ⬜ |
| B18 | **WebSockets & postMessage** | WebSocket upgrade handshake and frame protocol; `postMessage` origin enforcement; what happens when origin checks are skipped or wildcarded | Module 20 (pipes) | ⬜ |

---

## Recommended Stopping Points

| Goal | Stop at |
|------|---------|
| Understand OS fundamentals deeply | Module 17 (fork/exec) |
| Full OS mental model | Module 24 (threads) |
| Browser security foundation | B4 (site isolation) |
| Full exploit chain understanding | B12 (sandbox escape) |
| Complete browser security picture | B18 (WebSockets) |

---

## Tools

- **NASM** — assembler (bootloader + ISR stubs)
- **GCC / ld** (i686, via WSL) — C compiler and linker
- **QEMU** — `qemu-system-i386`; floppy (`-fda`) + disk (`-hda`)
- **GNU Make** — build system
- **dd** — raw disk image creation

## Architecture Target (OS Track)

- x86 32-bit, i686
- BIOS boot (not UEFI)
- Flat binary kernel loaded by our own bootloader at 0x1000
