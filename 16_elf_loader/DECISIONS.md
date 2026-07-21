# Module 16 — ELF Loader: Design Decisions

## 1. User programs are separate ELF binaries, not compiled into the kernel

**Decision:** `user_prog.c` is compiled with its own linker script (`user_linker.ld`)
into a standalone ELF32 binary (`user_prog.elf`) at 0x400000.  The kernel no longer
has any `user_prog.o` in its link step.  The ELF binary's raw bytes are embedded into
the kernel image via `objcopy --input-target binary`, written to BobFS on first boot,
and loaded at runtime via `SYS_EXEC`.

**Why:** This is the architectural leap that makes BobOS a real OS.  In Modules 14-15,
"user programs" were just function pointers compiled into the kernel binary — they were
kernel code dressed up as user code.  With a separate ELF binary, the user program is
genuinely independent: it has no access to kernel symbols, it links at a different base
address, and the kernel doesn't know what code it contains.

**Trade-off:** The ELF bytes are still shipped inside the kernel image (via the blob
object).  A true OS would ship the ELF on a filesystem image separately.  We embed it
for build simplicity — the important lesson is the load-time separation, not the
distribution mechanism.


## 2. ELF load address at 0x400000 (4 MB)

**Decision:** `user_linker.ld` sets `. = 0x400000`.  The ELF loader maps segments
starting at this virtual address in a fresh page directory.

**Why:** The kernel's identity map covers 0-4MB (PDE[0]).  Placing user code at exactly
4MB means it lives in PDE[1], a completely separate directory entry.  The kernel's PDE[0]
copy in the user PD is supervisor-only; PDE[1] is freshly allocated and user-accessible.
There is zero risk of virtual address collision between kernel and user code.


## 3. SYS_EXEC replaces the calling process's address space in-place

**Decision:** `do_exec()` calls `elf_load()`, replaces the calling process's `cr3`
via `process_set_cr3()`, switches to the new PD immediately, builds a new ring-3 fake
frame on the existing kernel stack, and returns the new frame's ESP.  The process's
PID, kernel stack, and scheduler position are all preserved.

**Why:** This matches the Unix `execve` semantic: same process, new address space.
The alternative (kill the old process and create a new one) would change the PID,
break any parent that stored it, and waste a scheduler slot.

**Old PD leak:** The previous page directory's physical pages are not returned to the
PMM.  This is intentional for this module — it keeps `do_exec` simple.  Module 17
(fork) will introduce proper PD reference counting and reclamation.


## 4. exec_stub pattern: ring-0 process calls SYS_EXEC

**Decision:** `kernel_main` spawns a ring-0 process (`exec_stub`) whose only job is to
call `sys_exec("hello.elf")`.  SYS_EXEC then transforms it into a ring-3 process.

**Why:** We need something to issue the first `SYS_EXEC`.  The idle process can't do it
(it must stay alive to keep the scheduler from halting).  A dedicated stub is the
simplest path — it starts as a normal ring-0 kernel process (no TSS update needed)
and is atomically converted to ring-3 inside `do_exec` when the new frame is returned.


## 5. objcopy --input-target binary to embed the ELF in the kernel image

**Decision:** The Makefile uses:
```
objcopy -I binary -O elf32-i386 -B i386 user_prog.elf user_elf_blob.o
```
This wraps the raw bytes of `user_prog.elf` as a linkable ELF32 object with three
symbols: `_binary_user_prog_elf_start`, `_binary_user_prog_elf_end`, and
`_binary_user_prog_elf_size`.  `kernel.c` declares these as `extern char` and writes
the byte range to BobFS.

**Why:** The cleanest way to ship a binary blob with a C program without a separate
loader, filesystem tool, or build-time disk image manipulation step.  The kernel is
self-bootstrapping: it carries its own first user program and installs it on first boot.


## 6. ELF segment mapping via direct physical address writes

**Decision:** After `install_user_page()` allocates a physical frame and maps it in
the new PD, `elf_load()` writes to the physical address directly (without switching CR3).
It walks the new PD's page tables to translate virtual addresses to physical frame
addresses, then memcpys the file data there.

**Why:** The kernel runs in an identity-mapped address space (virt == phys for 0-4MB).
All PMM pages are in this range.  Writing to a physical frame address is therefore safe
and does not require temporarily switching CR3 or installing a temporary kernel mapping.
This would not work for a non-identity-mapped kernel (e.g., a higher-half kernel) —
in that case, a temporary mapping or a dedicated "window" page would be needed.


## 7. User stack at 0x800000 for exec'd programs

**Decision:** `do_exec()` places the user stack at `EXEC_USER_STACK_TOP = 0x800000`
(8 MB), one page (4 KB) below the top.  The ring-3 fake frame sets `user_esp` to 0x800000.

**Why:** 0x800000 is safely above both the 4 MB kernel map (PDE[0]) and the user code
region starting at 0x400000 (PDE[1]).  It lives in PDE[2], which is freshly allocated
and user-accessible.  Future modules can grow the stack downward from 0x800000 with
demand-paged guard pages.


## 8. Linker script simplified — no more user_code_start/user_code_end

**Decision:** `linker.ld` drops the `.user_code` section, `EXCLUDE_FILE`, and the
`user_code_start`/`user_code_end` symbols entirely.  It is now a plain three-section
script (`.text`, `.data`, `.bss`) with `kernel_end` at the end.

**Why:** Those symbols existed only to tell `process_create_user()` which pages to mark
`PAGE_USER`.  Since `process_create_user()` is replaced by `elf_load()`, the symbols
are no longer needed.  Removing them makes the linker script self-explanatory.
