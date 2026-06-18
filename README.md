# theBud OS

**An x86 operating system built by hand, register by register — bootloader to a POSIX-flavored kernel with processes, paging, a filesystem, and a network stack — because you cannot reason about privilege escalation, use-after-free, or sandbox escapes in a machine you have never booted yourself.**

## What this is

theBud OS starts from the one fact the CPU actually knows at power-on — "execute whatever is at this fixed address" — and builds every layer above it by hand: a real-mode bootloader, the jump to 32-bit protected mode, a GDT, an IDT with hardware interrupt handling, a physical memory manager, paging, a heap, preemptive multitasking, syscalls, a filesystem, a shell, ring-3 user mode, ELF loading, `fork`/`exec`, signals, a VFS, pipes, copy-on-write, demand paging, shared memory, threads, a priority scheduler, a network driver, and a POSIX compatibility shim. No existing kernel, no borrowed bootloader — every one of those mechanisms is implemented in this repo and explained in a per-module `DECISIONS.md`.

That is the point, and it's the second half of a two-part **AppSec Education** series (the first half, `A-finity`, is the compiler that produces the ELF binaries this kernel loads and runs). Almost every serious class of vulnerability — privilege escalation, TOCTOU races, use-after-free, sandbox escapes, side channels — is a violation of an invariant some OS subsystem is supposed to enforce. You cannot reason like a vulnerability researcher about a broken invariant in a subsystem you have never implemented. Building the GDT, the IDT, the page tables, and the syscall gate by hand is what makes "ring 3 cannot execute `cli`" or "a page fault on a CoW page triggers a copy" into mechanisms you understand instead of facts you've memorized.

Every module's `DECISIONS.md` records the design chosen, why, and the specific production trade-off being deferred (e.g. deep-copy `fork` in Module 17, replaced by real copy-on-write in Module 21; four fixed scheduler priorities instead of Linux's 140-level CFS). Reading them in order is a running log of how a kernel actually gets built, corner by corner.

## Module map — Track 1: OS Kernel

| # | Module | What it proves |
|---|--------|-----------------|
| 01 | [Bootloader](./01_bootloader) | First code the CPU runs; BIOS real-mode disk load |
| 02 | [Kernel Entry](./02_kernel_entry) | Real Mode → 32-bit Protected Mode; C runtime environment |
| 03 | [VGA Text Driver](./03_vga_driver) | Direct framebuffer output; the base of every diagnostic print |
| 04 | [GDT](./04_gdt) | Segment descriptors defined and loaded from C, not raw asm bytes |
| 05 | [IDT & Interrupts](./05_idt_interrupts) | 48 vectors: CPU exceptions + remapped PIC hardware IRQs |
| 06 | [Physical Memory Manager](./06_pmm) | Bitmap allocator tracking every free 4 KB page |
| 07 | [Paging](./07_paging) | Virtual → physical translation; the MMU turned on |
| 08 | [Heap Allocator](./08_heap) | `kmalloc`/`kfree` with splitting and coalescing |
| 09 | [Keyboard Driver](./09_keyboard) | IRQ1 scancode → ASCII ring buffer; first real-time input |
| 10 | [Processes & Scheduler](./10_processes) | PCBs, context switching, preemptive round robin |
| 11 | [System Calls](./11_syscalls) | `int 0x80` gate as the only user↔kernel boundary |
| 12 | [File System (BobFS)](./12_filesystem) | Hand-written ATA PIO driver + flat FS on a raw disk image |
| 13 | [Shell](./13_shell) | Interactive process composing every prior subsystem |
| 14 | [User Mode (Ring 3)](./14_usermode) | TSS + DPL=3 segments; genuine privilege separation |
| 15 | [Per-Process Address Spaces](./15_address_spaces) | One page directory per process; real isolation |
| 16 | [ELF Loader](./16_elf_loader) | User programs as independent ELF binaries loaded from disk |
| 17 | [fork & exec](./17_fork_exec) | Deep-copy `fork`, `exec` replaces an address space, `wait` reaps children |
| 18 | [Signals](./18_signals) | Asynchronous `SIGKILL`/`SIGSEGV` delivery and user handlers |
| 19 | [VFS Layer](./19_vfs) | `vfs_ops` vtable abstraction; BobFS becomes one backend among several |
| 20 | [Pipes & IPC](./20_pipes) | Anonymous ring-buffer IPC; the foundation of shell pipelines |
| 21 | [Copy-on-Write Fork](./21_cow) | Lazy, refcounted page sharing — makes `fork` O(1) |
| 22 | [Demand Paging](./22_demand_paging) | `mmap` reserves virtual space; physical frames allocated on first fault |
| 23 | [Shared Memory](./23_shared_memory) | Key-based `shmget`/`shmat`; a covert-channel building block |
| 24 | [Threads](./24_threads) | Lightweight execution contexts sharing one address space |
| 25 | [Scheduler](./25_scheduler) | Four-level strict-priority preemption |
| 26 | [Network Stack](./26_network) | NE2000 NIC driver; polled TX/RX ring buffers |
| 27 | [Device Driver Framework](./27_drivers) | `driver_ops` vtable + probe/register lifecycle |
| 28 | [POSIX Compliance](./28_posix) | `errno`-based POSIX wrapper layer over the native syscall ABI |

## Also in this repo

- **[`browser/`](./browser)** — a second, in-progress track (B01–B18) that builds a toy multi-process browser on top of the kernel above: process isolation, a renderer sandbox, an HTML tokenizer/DOM, a JS interpreter and JIT, a garbage collector, and — the payoff modules — type confusion, a Spectre PoC, and a full renderer-to-sandbox-escape exploit chain, followed by protocol security (CSP, CORS, TLS, cookies, WebSockets). Each module maps back to a specific kernel mechanism (e.g. site isolation reuses Module 15's address-space separation).
- **[`lessons/`](./lessons)** — cross-cutting deep dives that span more than one module (kernel/userspace boundary, PCB/thread/scheduler internals, memory addressing) and don't belong inside a single module folder.
- **[`custom_lessons_bobos_prerequisites/`](./custom_lessons_bobos_prerequisites)** and **[`prerequisite.html`](./prerequisite.html)** — registers, endianness, memory, instructions, and the BIOS boot sequence, for readers who need the hardware background before Module 01.
- **[`GLOSSARY.md`](./GLOSSARY.md)** — every term, filename, and concept used across the project, alphabetized, tagged with the module that first introduces it.
- **[`ROADMAP.md`](./ROADMAP.md)** — the full build plan for both tracks, including recommended stopping points if you only want the OS fundamentals.

## Tech stack

- **x86 assembly (NASM)** — bootloader, ISR stubs, context-switch trampolines
- **C** — kernel, drivers, filesystem, network stack (freestanding, no libc)
- **GNU Make** — per-module build system
- **QEMU** (`qemu-system-i386`) — boot and test target; floppy (`-fda`) + disk (`-hda`)
- **ld** linker scripts — custom memory layout per module
- Target: **x86, 32-bit (i686), BIOS boot** — a flat binary kernel loaded by the project's own bootloader

## Status

Work in progress, built as course material alongside a YouTube series. The OS track (Modules 01–28) each contain working, independently buildable source with a `DECISIONS.md` and `tutorial.html`, going well past the original 24-module roadmap scope into networking, a driver framework, and POSIX compatibility. The `browser/` track has real, buildable module implementations (not stubs) even though `ROADMAP.md`'s status column hasn't been updated to reflect that — treat the module directories themselves, not the roadmap checkboxes, as the source of truth for what's built.

## Exploring / running it

Each module is self-contained and boots on its own:

```sh
cd 13_shell
make
qemu-system-i386 -fda os.img
```

Earlier modules (01–09) produce a floppy image (`os.img`) you boot directly; later modules that add a filesystem (12+) also build a disk image (`-hda`) — check that module's `Makefile` and `tutorial.html` for the exact QEMU invocation, since the boot flags change as the kernel grows (protected mode, paging, user mode, etc.). Read modules in order: each one's `DECISIONS.md` assumes you understand what the previous module deliberately left simple.

Build artifacts (`.o`, `.bin`, `.elf`, `.img`, `.iso`) are intentionally gitignored — run `make` in a module directory to regenerate them.

## Part of a series

theBud OS is the operating-systems half of a two-part, build-it-from-scratch systems programming series under the **AppSec Education** track — reasoning about software the way an attacker or vulnerability researcher does, from first principles, with no frameworks in between you and the machine. The first half, **A-finity**, is the compiler that produces the binaries this kernel loads and runs.
