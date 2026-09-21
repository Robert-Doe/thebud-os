/**
 * The full course map for theBud OS, transcribed from the real README.md
 * (Track 1: OS Kernel, modules 01-28) and ROADMAP.md (Track 2: Browser
 * Internals & Security, B01-B18) tables, plus the 5 standalone hardware
 * prerequisites in custom_lessons_bobos_prerequisites/. Each OS module
 * directory is a complete, independently-buildable kernel that carries
 * forward every file from the modules before it — so `depends` here is
 * simply "the previous module" for the kernel track, and the specific
 * kernel modules named in ROADMAP.md's "OS connection" column for the
 * browser track.
 */

export const PREREQUISITES = [
  { id: '01-registers', title: 'CPU Registers', file: '01-registers.html' },
  { id: '02-endianness', title: 'Endianness', file: '02-endianness.html' },
  { id: '03-memory', title: 'Memory', file: '03-memory.html' },
  { id: '04-instructions', title: 'Instructions', file: '04-instructions.html' },
  { id: '05-os-bootstrap', title: 'OS Bootstrap', file: '05-os-bootstrap.html' },
];

// dir is relative to the course root (thebud-os/).
const PHASE_GROUPS = {
  'Phase 1: Bare Metal Foundation': 'bare-metal',
  'Phase 2: Processes & I/O': 'processes-io',
  'Phase 3: Real Programs': 'real-programs',
  'Phase 4: Advanced Memory': 'advanced-memory',
  'Phase 5: Concurrency': 'concurrency',
  'Phase 6: Networking, Drivers & POSIX': 'net-drivers-posix',
  'Phase B1: Process Model & Isolation': 'process-isolation',
  'Phase B2: Rendering Pipeline': 'rendering',
  'Phase B3: Memory Safety': 'memory-safety',
  'Phase B4: Exploit Chain': 'exploit-chain',
  'Phase B5: Protocol Security': 'protocol-security',
};
export function groupForPhase(phase) {
  return PHASE_GROUPS[phase] ?? 'bare-metal';
}

export const OS_MODULES = [
  { num: 1, id: 'bootloader', title: 'Bootloader', proves: 'First code the CPU runs; BIOS real-mode disk load', dir: '01_bootloader', phase: 'Phase 1: Bare Metal Foundation', hasLive: true, hasConcepts: true },
  { num: 2, id: 'kernel_entry', title: 'Kernel Entry', proves: 'Real Mode → 32-bit Protected Mode; C runtime environment', dir: '02_kernel_entry', phase: 'Phase 1: Bare Metal Foundation', hasLive: true },
  { num: 3, id: 'vga_driver', title: 'VGA Text Driver', proves: 'Direct framebuffer output; the base of every diagnostic print', dir: '03_vga_driver', phase: 'Phase 1: Bare Metal Foundation', hasLive: true },
  { num: 4, id: 'gdt', title: 'GDT', proves: 'Segment descriptors defined and loaded from C, not raw asm bytes', dir: '04_gdt', phase: 'Phase 1: Bare Metal Foundation', hasLive: true },
  { num: 5, id: 'idt_interrupts', title: 'IDT & Interrupts', proves: '48 vectors: CPU exceptions + remapped PIC hardware IRQs', dir: '05_idt_interrupts', phase: 'Phase 1: Bare Metal Foundation', hasLive: true },
  { num: 6, id: 'pmm', title: 'Physical Memory Manager', proves: 'Bitmap allocator tracking every free 4 KB page', dir: '06_pmm', phase: 'Phase 1: Bare Metal Foundation' },
  { num: 7, id: 'paging', title: 'Paging', proves: 'Virtual → physical translation; the MMU turned on', dir: '07_paging', phase: 'Phase 1: Bare Metal Foundation' },
  { num: 8, id: 'heap', title: 'Heap Allocator', proves: '`kmalloc`/`kfree` with splitting and coalescing', dir: '08_heap', phase: 'Phase 1: Bare Metal Foundation' },
  { num: 9, id: 'keyboard', title: 'Keyboard Driver', proves: 'IRQ1 scancode → ASCII ring buffer; first real-time input', dir: '09_keyboard', phase: 'Phase 1: Bare Metal Foundation' },
  { num: 10, id: 'processes', title: 'Processes & Scheduler', proves: 'PCBs, context switching, preemptive round robin', dir: '10_processes', phase: 'Phase 2: Processes & I/O' },
  { num: 11, id: 'syscalls', title: 'System Calls', proves: '`int 0x80` gate as the only user↔kernel boundary', dir: '11_syscalls', phase: 'Phase 2: Processes & I/O' },
  { num: 12, id: 'filesystem', title: 'File System (BobFS)', proves: 'Hand-written ATA PIO driver + flat FS on a raw disk image', dir: '12_filesystem', phase: 'Phase 2: Processes & I/O' },
  { num: 13, id: 'shell', title: 'Shell', proves: 'Interactive process composing every prior subsystem', dir: '13_shell', phase: 'Phase 2: Processes & I/O' },
  { num: 14, id: 'usermode', title: 'User Mode (Ring 3)', proves: 'TSS + DPL=3 segments; genuine privilege separation', dir: '14_usermode', phase: 'Phase 2: Processes & I/O' },
  { num: 15, id: 'address_spaces', title: 'Per-Process Address Spaces', proves: 'One page directory per process; real isolation', dir: '15_address_spaces', phase: 'Phase 2: Processes & I/O' },
  { num: 16, id: 'elf_loader', title: 'ELF Loader', proves: 'User programs as independent ELF binaries loaded from disk', dir: '16_elf_loader', phase: 'Phase 3: Real Programs' },
  { num: 17, id: 'fork_exec', title: 'fork & exec', proves: 'Deep-copy `fork`, `exec` replaces an address space, `wait` reaps children', dir: '17_fork_exec', phase: 'Phase 3: Real Programs' },
  { num: 18, id: 'signals', title: 'Signals', proves: 'Asynchronous `SIGKILL`/`SIGSEGV` delivery and user handlers', dir: '18_signals', phase: 'Phase 3: Real Programs' },
  { num: 19, id: 'vfs', title: 'VFS Layer', proves: '`vfs_ops` vtable abstraction; BobFS becomes one backend among several', dir: '19_vfs', phase: 'Phase 4: Advanced Memory' },
  { num: 20, id: 'pipes', title: 'Pipes & IPC', proves: 'Anonymous ring-buffer IPC; the foundation of shell pipelines', dir: '20_pipes', phase: 'Phase 4: Advanced Memory' },
  { num: 21, id: 'cow', title: 'Copy-on-Write Fork', proves: 'Lazy, refcounted page sharing — makes `fork` O(1)', dir: '21_cow', phase: 'Phase 4: Advanced Memory' },
  { num: 22, id: 'demand_paging', title: 'Demand Paging', proves: '`mmap` reserves virtual space; physical frames allocated on first fault', dir: '22_demand_paging', phase: 'Phase 4: Advanced Memory' },
  { num: 23, id: 'shared_memory', title: 'Shared Memory', proves: 'Key-based `shmget`/`shmat`; a covert-channel building block', dir: '23_shared_memory', phase: 'Phase 5: Concurrency' },
  { num: 24, id: 'threads', title: 'Threads', proves: 'Lightweight execution contexts sharing one address space', dir: '24_threads', phase: 'Phase 5: Concurrency' },
  { num: 25, id: 'scheduler', title: 'Scheduler', proves: 'Four-level strict-priority preemption', dir: '25_scheduler', phase: 'Phase 6: Networking, Drivers & POSIX' },
  { num: 26, id: 'network', title: 'Network Stack', proves: 'NE2000 NIC driver; polled TX/RX ring buffers', dir: '26_network', phase: 'Phase 6: Networking, Drivers & POSIX' },
  { num: 27, id: 'drivers', title: 'Device Driver Framework', proves: '`driver_ops` vtable + probe/register lifecycle', dir: '27_drivers', phase: 'Phase 6: Networking, Drivers & POSIX' },
  { num: 28, id: 'posix', title: 'POSIX Compliance', proves: '`errno`-based POSIX wrapper layer over the native syscall ABI', dir: '28_posix', phase: 'Phase 6: Networking, Drivers & POSIX' },
];

// dir is relative to the course root (thebud-os/), under browser/.
export const BROWSER_MODULES = [
  { num: 1, id: 'process_model', title: 'Multi-Process Browser Architecture', proves: 'Browser process (kernel) + renderer process (user); IPC channel between them; why Chrome has 10+ processes', dir: 'browser/B01_process_model', phase: 'Phase B1: Process Model & Isolation', osConnection: [10, 17] },
  { num: 2, id: 'renderer_sandbox', title: 'Renderer Sandbox', proves: 'Seccomp-style syscall whitelist on the renderer; all disallowed calls go through the broker; sandbox escape = finding a hole in the whitelist', dir: 'browser/B02_renderer_sandbox', phase: 'Phase B1: Process Model & Isolation', osConnection: [11] },
  { num: 3, id: 'same_origin_policy', title: 'Same-Origin Policy Engine', proves: 'Origin parsing and enforcement owned by the browser process; renderer asks, browser decides; why the renderer never makes trust decisions', dir: 'browser/B03_same_origin_policy', phase: 'Phase B1: Process Model & Isolation', osConnection: [11, 15] },
  { num: 4, id: 'site_isolation', title: 'Site Isolation', proves: 'One renderer process per origin; cross-origin iframes cannot share memory; process registry maps origin → CR3', dir: 'browser/B04_site_isolation', phase: 'Phase B1: Process Model & Isolation', osConnection: [15, 21] },
  { num: 5, id: 'html_tokenizer', title: 'HTML Tokenizer & DOM', proves: 'Tokenizer → DOM tree; parser differentials that create mutation XSS; why two parsers seeing the same bytes build different trees', dir: 'browser/B05_html_tokenizer', phase: 'Phase B2: Rendering Pipeline', osConnection: [3] },
  { num: 6, id: 'css_cascade', title: 'CSS Cascade & Layout', proves: 'Style computation, box tree, pixel coordinates; CSS timing side-channels (`@font-face`, scroll-to-text-fragment)', dir: 'browser/B06_css_cascade', phase: 'Phase B2: Rendering Pipeline', osConnection: [7] },
  { num: 7, id: 'js_interpreter', title: 'JS Interpreter', proves: 'Tree-walking interpreter; JS heap separate from DOM; type confusion: convincing the engine an integer is a pointer', dir: 'browser/B07_js_interpreter', phase: 'Phase B2: Rendering Pipeline', osConnection: [8, 10] },
  { num: 8, id: 'jit_compiler', title: 'JIT Compiler', proves: 'Single-pass x86 code emitter into an executable page; JIT spray; W^X enforcement; why executable pages must never be writable', dir: 'browser/B08_jit_compiler', phase: 'Phase B2: Rendering Pipeline', osConnection: [7, 22] },
  { num: 9, id: 'garbage_collector', title: 'Garbage Collector', proves: 'Mark-and-sweep GC; use-after-free: raw pointer to a freed object; root cause of most browser CVEs', dir: 'browser/B09_garbage_collector', phase: 'Phase B3: Memory Safety', osConnection: [6, 8] },
  { num: 10, id: 'type_confusion', title: 'Type Confusion', proves: 'Type tag on every JS value; deliberately break it; reproduce the class of bug behind V8 CVE-2021-21220, CVE-2022-1096', dir: 'browser/B10_type_confusion', phase: 'Phase B3: Memory Safety', osConnection: [7], dependsBrowser: [7] },
  { num: 11, id: 'spectre', title: 'Spectre PoC', proves: 'SharedArrayBuffer + `performance.now()` cache timing channel between two origins; reproduce why SAB was disabled in 2018; test timer mitigations', dir: 'browser/B11_spectre', phase: 'Phase B4: Exploit Chain', osConnection: [23] },
  { num: 12, id: 'exploit_chain', title: 'Renderer Exploit → Sandbox Escape', proves: 'Controlled type confusion → arbitrary r/w in renderer → IPC escape attempt to browser process; full exploit chain walkthrough', dir: 'browser/B12_exploit_chain', phase: 'Phase B4: Exploit Chain', osConnection: [], dependsBrowser: [2, 9, 10] },
  { num: 13, id: 'csp', title: 'Content Security Policy', proves: 'Parse and enforce CSP headers; find three bypasses in a misconfigured policy; why `unsafe-inline` and wildcards collapse the policy', dir: 'browser/B13_csp', phase: 'Phase B5: Protocol Security', osConnection: [], dependsBrowser: [3, 5] },
  { num: 14, id: 'cors', title: 'CORS & Fetch', proves: 'CORS preflight and response header enforcement from scratch; `null` origin danger; CORB and CORP on top', dir: 'browser/B14_cors', phase: 'Phase B5: Protocol Security', osConnection: [], dependsBrowser: [3] },
  { num: 15, id: 'http_parser', title: 'HTTP/1.1 Parser', proves: 'Request/response parser with connection reuse; HTTP request smuggling: disagreement between your parser and a proxy on where a request ends', dir: 'browser/B15_http_parser', phase: 'Phase B5: Protocol Security', osConnection: [12] },
  { num: 16, id: 'tls_handshake', title: 'TLS Handshake', proves: 'TLS 1.3 state machine (crypto primitives from a library); certificate validation; SNI; what a MitM can and cannot see', dir: 'browser/B16_tls_handshake', phase: 'Phase B5: Protocol Security', osConnection: [16] },
  { num: 17, id: 'cookies', title: 'Cookie & Session Model', proves: '`Set-Cookie` parsing; `SameSite` enforcement; `HttpOnly` / `Secure` flags; CSRF attack against missing `SameSite`; fix with correct flag', dir: 'browser/B17_cookies', phase: 'Phase B5: Protocol Security', osConnection: [], dependsBrowser: [3, 14] },
  { num: 18, id: 'websockets', title: 'WebSockets & postMessage', proves: 'WebSocket upgrade handshake and frame protocol; `postMessage` origin enforcement; what happens when origin checks are skipped or wildcarded', dir: 'browser/B18_websockets', phase: 'Phase B5: Protocol Security', osConnection: [20] },
];

/**
 * Cross-cutting deep-dive lesson clusters (lessons/NN_something, plus the
 * one exception at 04_gdt/lessons, per lessons/README.md). Titles and page
 * lists are auto-discovered from the real files at generation time
 * (each page's own <title> tag) — only the "what this relates to" mapping
 * is hand-curated here, since it isn't derivable from the filesystem.
 */
export const LESSON_CLUSTERS = [
  { id: 'gdt_segmentation', dir: '04_gdt/lessons', relatesToOS: [4] },
  { id: '01_kernel_userspace', dir: 'lessons/01_kernel_userspace', relatesToOS: [2, 11, 14] },
  { id: '02_browser_xray', dir: 'lessons/02_browser_xray', relatesToBrowser: [1, 2, 4, 7, 8] },
  { id: '03_pcb_threads_scheduling', dir: 'lessons/03_pcb_threads_scheduling', relatesToOS: [10, 24, 25] },
  { id: '04_memory_addressing', dir: 'lessons/04_memory_addressing', relatesToOS: [6, 7, 15, 21, 22] },
  { id: '05_bootloader', dir: 'lessons/05_bootloader', relatesToOS: [1] },
  { id: '06_vga_driver', dir: 'lessons/06_vga_driver', relatesToOS: [3] },
  { id: '07_idt_interrupts', dir: 'lessons/07_idt_interrupts', relatesToOS: [5] },
  { id: '08_heap', dir: 'lessons/08_heap', relatesToOS: [8] },
  { id: '09_keyboard', dir: 'lessons/09_keyboard', relatesToOS: [9] },
  { id: '10_filesystems', dir: 'lessons/10_filesystems', relatesToOS: [12] },
  { id: '11_shell', dir: 'lessons/11_shell', relatesToOS: [13] },
  { id: '12_elf_loader', dir: 'lessons/12_elf_loader', relatesToOS: [16] },
  { id: '13_signals', dir: 'lessons/13_signals', relatesToOS: [18] },
  { id: '14_ipc_pipes', dir: 'lessons/14_ipc_pipes', relatesToOS: [20] },
  { id: '15_networking', dir: 'lessons/15_networking', relatesToOS: [26] },
  { id: '16_drivers', dir: 'lessons/16_drivers', relatesToOS: [27] },
  { id: '17_posix', dir: 'lessons/17_posix', relatesToOS: [28] },
  { id: '18_origin_security', dir: 'lessons/18_origin_security', relatesToBrowser: [3, 14] },
  { id: '19_html_css_engine', dir: 'lessons/19_html_css_engine', relatesToBrowser: [5, 6] },
  { id: '20_garbage_collection', dir: 'lessons/20_garbage_collection', relatesToBrowser: [9] },
  { id: '21_exploit_primitives', dir: 'lessons/21_exploit_primitives', relatesToBrowser: [10, 11, 12] },
  { id: '22_content_security', dir: 'lessons/22_content_security', relatesToBrowser: [13] },
  { id: '23_http_tls', dir: 'lessons/23_http_tls', relatesToBrowser: [15, 16] },
  { id: '24_cookies', dir: 'lessons/24_cookies', relatesToBrowser: [17] },
  { id: '25_websockets', dir: 'lessons/25_websockets', relatesToBrowser: [18] },
];
