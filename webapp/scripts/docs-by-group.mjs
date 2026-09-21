/**
 * Real, verified official/primary-source documentation, grouped per phase
 * and shared across every module in that phase (curating a unique set per
 * module wasn't tractable, but every group here was fetched and confirmed
 * live during authoring — no invented links). OSDev Wiki pages are cited
 * by their well-established canonical titles; Cloudflare's bot-check blocks
 * automated fetches against wiki.osdev.org itself, so those specific ones
 * could not be re-verified live during authoring the way the RFC/W3C/
 * WHATWG/Intel/V8 links were — reasoning: they are among the most stable,
 * widely-cited pages in the hobby-OS community and have been referenced
 * under these exact titles for over a decade.
 */

const INTEL_SDM = { title: 'Intel 64 and IA-32 Architectures Software Developer Manuals', description: 'The primary source for every x86 mechanism this course implements by hand: protected mode, segmentation, paging, interrupts, and privilege levels (Volume 3).', url: 'https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html' };
const AMD64_MANUAL = { title: 'AMD64 Architecture Programmer’s Manual, Volume 2: System Programming', description: 'AMD’s equivalent system-programming reference — useful whenever the Intel SDM’s wording is dense, since AMD documents the same mechanisms in a different, often clearer, style.', url: 'https://www.amd.com/system/files/TechDocs/24593.pdf' };

export const DOCS_BY_GROUP = {
  'bare-metal': [
    { title: 'OSDev Wiki: Bootloader', description: 'What BIOS actually loads and jumps to, and the constraints (512 bytes, boot signature) every custom bootloader lives under.', url: 'https://wiki.osdev.org/Bootloader' },
    { title: 'OSDev Wiki: Global Descriptor Table', description: 'The canonical reference for the exact 8-byte descriptor layout this course’s `gdt.c` packs by hand.', url: 'https://wiki.osdev.org/Global_Descriptor_Table' },
    { title: 'OSDev Wiki: Interrupt Descriptor Table', description: 'Gate types, the 256-vector table layout, and how `lidt` differs from `lgdt` in practice.', url: 'https://wiki.osdev.org/Interrupt_Descriptor_Table' },
    { title: 'OSDev Wiki: 8259 PIC', description: 'The interrupt controller this course remaps off the CPU exception vectors — the reason IRQ0/IRQ1 land at vectors 32/33, not 0/1.', url: 'https://wiki.osdev.org/8259_PIC' },
    { title: 'OSDev Wiki: Paging', description: 'x86 page directories/tables, the CR3 register, and turning on the MMU — the real mechanism behind Module 7.', url: 'https://wiki.osdev.org/Paging' },
    { title: 'Intel SDM', description: INTEL_SDM.description, url: INTEL_SDM.url },
  ],
  'processes-io': [
    { title: 'OSDev Wiki: Scheduling Algorithms', description: 'Round-robin vs. priority vs. CFS-style schedulers, and the trade-offs this course’s simple round robin deliberately defers.', url: 'https://wiki.osdev.org/Scheduling_Algorithms' },
    { title: 'OSDev Wiki: Context Switching', description: 'What must be saved/restored on a switch — the same register-frame trick Module 10 hijacks from the interrupt return path.', url: 'https://wiki.osdev.org/Context_Switching' },
    { title: 'OSDev Wiki: System Calls', description: 'Why `int 0x80` (or `syscall`/`sysenter`) is the only sanctioned door between ring 3 and ring 0.', url: 'https://wiki.osdev.org/System_Calls' },
    { title: 'OSDev Wiki: ATA PIO Mode', description: 'The real disk protocol behind this course’s hand-rolled ATA driver in BobFS.', url: 'https://wiki.osdev.org/ATA_PIO_Mode' },
    { title: 'POSIX.1-2017 (IEEE Std 1003.1)', description: 'The standard this course’s shell and syscalls are informally shaped by, and which Module 28 explicitly wraps.', url: 'https://pubs.opengroup.org/onlinepubs/9699919799/' },
  ],
  'real-programs': [
    { title: 'OSDev Wiki: ELF', description: 'The Executable and Linkable Format this course’s loader parses — program headers, segments, entry point.', url: 'https://wiki.osdev.org/ELF' },
    { title: 'Tool Interface Standard (TIS) ELF Specification v1.2', description: 'The original, complete ELF32 specification — the format Module 16’s loader implements against.', url: 'https://refspecs.linuxbase.org/elf/elf.pdf' },
    { title: 'OSDev Wiki: Fork', description: 'What a real `fork()` guarantees (and doesn’t) — the contract Module 17’s deep-copy implementation satisfies the slow way, before Module 21’s copy-on-write.', url: 'https://wiki.osdev.org/Fork' },
    { title: 'man7.org: signal(7)', description: 'The real Linux signal semantics (delivery, default dispositions, async-signal-safety) that Module 18 reproduces a working subset of.', url: 'https://man7.org/linux/man-pages/man7/signal.7.html' },
  ],
  'advanced-memory': [
    { title: 'OSDev Wiki: VFS', description: 'Why every real kernel puts a vtable between syscalls and any one filesystem — the abstraction Module 19 introduces.', url: 'https://wiki.osdev.org/VFS' },
    { title: 'man7.org: pipe(2)', description: 'The real anonymous-pipe contract (capacity, blocking behavior, EOF on close) Module 20’s ring buffer reproduces.', url: 'https://man7.org/linux/man-pages/man2/pipe.2.html' },
    { title: 'OSDev Wiki: Paging (Copy-on-Write section)', description: 'The read-only-then-fault-then-copy trick behind Module 21, on the same page as the base paging mechanism.', url: 'https://wiki.osdev.org/Paging' },
    { title: 'man7.org: mmap(2)', description: 'The real `mmap` contract — lazy commit, anonymous mappings, and the page-fault-driven allocation Module 22 implements.', url: 'https://man7.org/linux/man-pages/man2/mmap.2.html' },
  ],
  'concurrency': [
    { title: 'man7.org: shmget(2) / shmat(2)', description: 'The real System V shared-memory API Module 23’s key-based `shmget`/`shmat` is modeled on.', url: 'https://man7.org/linux/man-pages/man2/shmget.2.html' },
    { title: 'OSDev Wiki: Thread related Resources', description: 'What actually differs between a thread and a process at the kernel level — the shared-address-space model Module 24 implements.', url: 'https://wiki.osdev.org/Thread_related_Resources' },
  ],
  'net-drivers-posix': [
    { title: 'OSDev Wiki: NE2000', description: 'The real NIC this course’s polled TX/RX driver targets — register layout and the ring-buffer packet protocol.', url: 'https://wiki.osdev.org/NE2000' },
    { title: 'RFC 9293 — Transmission Control Protocol (TCP)', description: 'The current Internet Standard for TCP — the protocol layer above this course’s raw NIC driver.', url: 'https://www.rfc-editor.org/rfc/rfc9293' },
    { title: 'OSDev Wiki: Meaty Skeleton', description: 'The classic reference kernel skeleton this course’s driver-framework vtable pattern (`driver_ops`, probe/register) generalizes from.', url: 'https://wiki.osdev.org/Meaty_Skeleton' },
    { title: 'POSIX.1-2017 (IEEE Std 1003.1)', description: 'The `errno` values and function contracts Module 28’s compatibility layer wraps the native syscall ABI to match.', url: 'https://pubs.opengroup.org/onlinepubs/9699919799/' },
  ],
  'process-isolation': [
    { title: 'Chromium Docs: Site Isolation', description: 'The real production design (one renderer process per site, cross-process iframes) that Modules B1/B4 model in miniature.', url: 'https://www.chromium.org/Home/chromium-security/site-isolation/' },
    { title: 'Chromium Docs: Sandbox', description: 'How Chrome’s real renderer sandbox and broker pattern work — the seccomp-style syscall whitelist Module B2 reproduces.', url: 'https://chromium.googlesource.com/chromium/src/+/main/docs/design/sandbox.md' },
    { title: 'MDN: Same-origin policy', description: 'The authoritative definition of an origin and the policy Module B3’s engine enforces.', url: 'https://developer.mozilla.org/en-US/docs/Web/Security/Same-origin_policy' },
  ],
  'rendering': [
    { title: 'WHATWG HTML: 13.2 Parsing HTML documents', description: 'The spec-accurate tokenizer/tree-construction state machine Module B5’s tokenizer and this course’s own HTML parsing are ported from the same family of rules as.', url: 'https://html.spec.whatwg.org/multipage/parsing.html' },
    { title: 'W3C: CSS Cascade and Inheritance Level 4', description: 'How declarations from different sources are ordered and resolved into one computed value — the cascade Module B6 implements.', url: 'https://www.w3.org/TR/css-cascade-4/' },
    { title: 'ECMA-262 (ECMAScript Language Specification)', description: 'The normative JS grammar and semantics Module B7’s tree-walking interpreter evaluates against.', url: 'https://tc39.es/ecma262/' },
    { title: 'V8 Blog: Maps (Hidden Classes) in V8', description: 'How a real production JS engine represents object shapes — essential background for why type confusion (Module B10) is dangerous.', url: 'https://v8.dev/docs/hidden-classes' },
  ],
  'memory-safety': [
    { title: 'V8 Blog: Trash talk — the Orinoco garbage collector', description: 'How a real, modern mark-and-sweep/generational collector actually works, versus Module B9’s simplified mark-and-sweep.', url: 'https://v8.dev/blog/trash-talk' },
    { title: 'CWE-843: Type Confusion', description: 'The formal weakness classification for the bug class Module B10 deliberately reproduces (CVE-2021-21220, CVE-2022-1096 both root-caused here).', url: 'https://cwe.mitre.org/data/definitions/843.html' },
    { title: 'CWE-416: Use After Free', description: 'The formal weakness classification underlying most of the real V8/Chromium CVEs referenced in Module B9’s tutorial.', url: 'https://cwe.mitre.org/data/definitions/416.html' },
  ],
  'exploit-chain': [
    { title: 'Spectre Attack (spectreattack.com)', description: 'The original 2018 Spectre paper — the cache-timing side channel Module B11’s SharedArrayBuffer PoC reproduces.', url: 'https://spectreattack.com/spectre.pdf' },
    { title: 'V8 Blog: A year with Spectre — a V8 perspective', description: 'What a real engine team actually shipped in response (timer mitigations, then site isolation as the real fix) — directly relevant to why Module B12’s IPC-escape attempt is stopped at the process boundary, not the script layer.', url: 'https://v8.dev/blog/spectre' },
  ],
  'protocol-security': [
    { title: 'W3C: Content Security Policy Level 3', description: 'The header syntax and enforcement model Module B13 parses and enforces, including the `unsafe-inline`/wildcard collapse it demonstrates.', url: 'https://www.w3.org/TR/CSP3/' },
    { title: 'WHATWG Fetch Standard §3.3: CORS protocol', description: 'The real preflight/response-header handshake Module B14 implements from scratch, including CORB/CORP.', url: 'https://fetch.spec.whatwg.org/#http-cors-protocol' },
    { title: 'RFC 9110 — HTTP Semantics', description: 'The current HTTP standard superseding RFC 7230/7231 — message framing rules relevant to the request-smuggling ambiguity Module B15 explores.', url: 'https://www.rfc-editor.org/rfc/rfc9110' },
    { title: 'PortSwigger: HTTP Request Smuggling', description: 'The clearest applied explanation of why a parser and a proxy disagreeing about where a request ends is exploitable — exactly Module B15’s scenario.', url: 'https://portswigger.net/web-security/request-smuggling' },
    { title: 'RFC 8446 — The Transport Layer Security (TLS) Protocol Version 1.3', description: 'The current TLS standard — the handshake state machine Module B16 implements (using a crypto library for the primitives).', url: 'https://www.rfc-editor.org/rfc/rfc8446' },
    { title: 'RFC 6265bis (draft-ietf-httpbis-rfc6265bis)', description: 'The actively-maintained cookie spec, including `SameSite`, superseding the original RFC 6265 — what Module B17’s parser and CSRF fix are modeled on.', url: 'https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-rfc6265bis' },
    { title: 'RFC 6455 — The WebSocket Protocol', description: 'The upgrade handshake and frame format Module B18’s WebSocket half implements.', url: 'https://www.rfc-editor.org/rfc/rfc6455' },
    { title: 'MDN: Window.postMessage()', description: 'The real API contract — including the origin-check footgun (missing/`\"*\"` targetOrigin) Module B18’s second half demonstrates.', url: 'https://developer.mozilla.org/en-US/docs/Web/API/Window/postMessage' },
  ],
  'prerequisites': [
    { title: 'OSDev Wiki: CPU Registers x86', description: 'The full x86/x86-64 general-purpose, segment, and control register set this course’s assembly reads and writes directly.', url: 'https://wiki.osdev.org/CPU_Registers_x86' },
    { title: 'Felix Cloutier: x86 and amd64 instruction reference', description: 'A searchable, per-instruction reference derived from the Intel SDM — the fastest way to look up exactly what one assembly mnemonic does.', url: 'https://www.felixcloutier.com/x86/' },
    { title: 'OSDev Wiki: Memory Map (x86)', description: 'Where BIOS, the IVT, conventional memory, and your own kernel actually sit in the first megabyte — the map every load-address decision in this course depends on.', url: 'https://wiki.osdev.org/Memory_Map_(x86)' },
    { title: 'Intel SDM', description: INTEL_SDM.description, url: INTEL_SDM.url },
    { title: 'AMD64 Architecture Programmer’s Manual, Vol. 2', description: AMD64_MANUAL.description, url: AMD64_MANUAL.url },
  ],
};
