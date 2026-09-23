# theBud OS

I built an operating system by hand. Not "by hand" in the loose way people mean when they've patched a Linux config file, I mean the actual thing: the CPU powers on, reads one fixed address in memory, and every layer above that fact, I wrote myself. Bootloader. Protected mode switch. The GDT. The IDT. A physical memory manager. Paging. A heap. Preemptive multitasking. Syscalls. A filesystem. A shell. Ring 3 user mode. `fork`, `exec`, signals. Copy-on-write. Threads. A scheduler. Even a network driver and a POSIX shim on top. No borrowed kernel, no borrowed bootloader. If it runs, I put it there.

## Why do this to yourself

Here's the thing that got me started. You can read a hundred articles about privilege escalation or use-after-free bugs and nod along, but you're nodding at a description of a rule you've never actually built. Ring 3 can't execute `cli`. Okay, sure, but why not, and what actually stops it, and what does the CPU do the instant it tries? A page fault on a copy-on-write page triggers a real copy. Great, but copy of what, tracked how, freed when?

You don't really know an invariant until you've been the one responsible for enforcing it. So that's what this project is. Every mechanism that a real OS uses to keep processes from stepping on each other, I built myself, one module at a time, and I wrote down exactly why I made each choice in a `DECISIONS.md` next to the code. Not just what I did. Why I did it that way, and what I gave up by doing it that way instead of the harder, more correct way a production kernel would.

This is the second half of a two-part series I'm calling **AppSec Education**. The first half, `A-finity`, is the compiler that produces the ELF binaries this kernel loads and runs. Put the two together and you've got the whole chain, from source code to a process actually executing on hardware you understand at every layer.

## What's actually in here

Every `DECISIONS.md` records the choice I made, why I made it, and the specific corner I cut that a real kernel wouldn't cut, along with which later module goes back and fixes it. Module 17 does a full deep copy on `fork`. That's slow and I knew it was slow while I wrote it. Module 21 comes back and replaces it with real copy-on-write. Module 10 gives the scheduler four fixed priority levels instead of anything like Linux's actual CFS. Reading the `DECISIONS.md` files in order is basically watching a kernel get built the way kernels actually get built: simple first, then patched to be less naive once the simple version has taught you what was wrong with it.

## Module map, Track 1: OS Kernel

| # | Module | What it proves |
|---|--------|-----------------|
| 01 | [Bootloader](./01_bootloader) | First code the CPU runs; BIOS real mode disk load |
| 02 | [Kernel Entry](./02_kernel_entry) | Real Mode to 32-bit Protected Mode; C runtime environment |
| 03 | [VGA Text Driver](./03_vga_driver) | Direct framebuffer output; the base of every diagnostic print |
| 04 | [GDT](./04_gdt) | Segment descriptors defined and loaded from C, not raw asm bytes |
| 05 | [IDT & Interrupts](./05_idt_interrupts) | 48 vectors: CPU exceptions plus remapped PIC hardware IRQs |
| 06 | [Physical Memory Manager](./06_pmm) | Bitmap allocator tracking every free 4 KB page |
| 07 | [Paging](./07_paging) | Virtual to physical translation; the MMU turned on |
| 08 | [Heap Allocator](./08_heap) | `kmalloc`/`kfree` with splitting and coalescing |
| 09 | [Keyboard Driver](./09_keyboard) | IRQ1 scancode to ASCII ring buffer; first real-time input |
| 10 | [Processes & Scheduler](./10_processes) | PCBs, context switching, preemptive round robin |
| 11 | [System Calls](./11_syscalls) | `int 0x80` gate as the only user to kernel boundary |
| 12 | [File System (BobFS)](./12_filesystem) | Hand-written ATA PIO driver plus a flat FS on a raw disk image |
| 13 | [Shell](./13_shell) | Interactive process composing every prior subsystem |
| 14 | [User Mode (Ring 3)](./14_usermode) | TSS plus DPL=3 segments; genuine privilege separation |
| 15 | [Per-Process Address Spaces](./15_address_spaces) | One page directory per process; real isolation |
| 16 | [ELF Loader](./16_elf_loader) | User programs as independent ELF binaries loaded from disk |
| 17 | [fork & exec](./17_fork_exec) | Deep-copy `fork`, `exec` replaces an address space, `wait` reaps children |
| 18 | [Signals](./18_signals) | Asynchronous `SIGKILL`/`SIGSEGV` delivery and user handlers |
| 19 | [VFS Layer](./19_vfs) | `vfs_ops` vtable abstraction; BobFS becomes one backend among several |
| 20 | [Pipes & IPC](./20_pipes) | Anonymous ring-buffer IPC; the foundation of shell pipelines |
| 21 | [Copy-on-Write Fork](./21_cow) | Lazy, refcounted page sharing; makes `fork` fast |
| 22 | [Demand Paging](./22_demand_paging) | `mmap` reserves virtual space; physical frames allocated on first fault |
| 23 | [Shared Memory](./23_shared_memory) | Key-based `shmget`/`shmat`; a covert-channel building block |
| 24 | [Threads](./24_threads) | Lightweight execution contexts sharing one address space |
| 25 | [Scheduler](./25_scheduler) | Four-level strict-priority preemption |
| 26 | [Network Stack](./26_network) | NE2000 NIC driver; polled TX/RX ring buffers |
| 27 | [Device Driver Framework](./27_drivers) | `driver_ops` vtable plus probe/register lifecycle |
| 28 | [POSIX Compliance](./28_posix) | `errno`-based POSIX wrapper layer over the native syscall ABI |

## The rest of the repo

Once I had a kernel, I couldn't resist building a toy browser on top of it, since a browser is basically the most complicated userspace program most of us interact with every day, and every security boundary inside it maps straight back to an OS mechanism I'd already built.

- **[`browser/`](./browser)**, modules B01 through B18: process isolation, a renderer sandbox, an HTML tokenizer and DOM, a JS interpreter and JIT, a garbage collector, and then the modules I was building toward the whole time: type confusion, a Spectre proof of concept, and a full renderer-to-sandbox-escape exploit chain, followed by protocol security work on CSP, CORS, TLS, cookies, and WebSockets. Site isolation in B04 literally reuses Module 15's address-space separation. None of this is decoration, it's the same invariants wearing a browser's clothes.
- **[`lessons/`](./lessons)**: deep dives on ideas that stretch across more than one module, like the kernel/userspace boundary, how a PCB actually relates to a thread and a scheduler slot, and memory addressing in general.
- **[`custom_lessons_bobos_prerequisites/`](./custom_lessons_bobos_prerequisites)** and **[`prerequisite.html`](./prerequisite.html)**: registers, endianness, memory, instructions, and the BIOS boot sequence, for anyone who wants the hardware background before Module 01 rather than picking it up as they go.
- **[`GLOSSARY.md`](./GLOSSARY.md)**: every term and filename I used across the whole project, alphabetized, tagged with the module that first introduces it.
- **[`ROADMAP.md`](./ROADMAP.md)**: the full build plan for both tracks, plus where to stop if you only care about the OS fundamentals and not the browser half.

## Tech stack

- x86 assembly, assembled with NASM: the bootloader, ISR stubs, context-switch trampolines
- C, freestanding, no libc: the kernel, drivers, filesystem, network stack
- GNU Make, one Makefile per module
- QEMU (`qemu-system-i386`) as the boot and test target, floppy image plus disk image depending on the module
- Custom `ld` linker scripts controlling exactly where in memory each module's code lands
- Target: x86, 32-bit (i686), BIOS boot, a flat binary kernel loaded by my own bootloader

## Where this actually stands

This is a living project, built alongside a YouTube series, and it's further along than the roadmap checkboxes make it look. Every module from 01 through 28 is real, working, independently buildable source with its own `DECISIONS.md` and `tutorial.html`, and I went well past my original 24-module plan into networking, a driver framework, and POSIX compatibility once I got there and didn't want to stop. The `browser/` track has genuine, buildable implementations too, not stubs, even though `ROADMAP.md` hasn't caught up to say so yet. If the roadmap and the actual folders ever disagree, trust the folders.

## Actually running it

Each module boots on its own, which is the whole point of structuring it this way:

```sh
cd 13_shell
make
qemu-system-i386 -fda os.img
```

Modules 01 through 09 only need a floppy image (`os.img`). Once a module adds a filesystem, from 12 onward, it needs a disk image too (`-hda`), and the exact QEMU flags shift a little as the kernel grows more capable. Check that module's own `Makefile` and `tutorial.html` if the boot command above doesn't match what you're looking at.

Read the modules in order if you actually want this to make sense. Every `DECISIONS.md` assumes you already understand whatever the previous module deliberately left too simple.

Build artifacts (`.o`, `.bin`, `.elf`, `.img`, `.iso`) are gitignored on purpose. Run `make` and they'll show back up.

## Part of a series

theBud OS is the operating systems half of a two-part systems programming series I'm calling **AppSec Education**: build the whole stack from first principles, with nothing borrowed and nothing hidden behind a framework, so you can reason about software the way someone trying to break it would. The other half, **A-finity**, is the compiler that produces the binaries this kernel loads and runs.
