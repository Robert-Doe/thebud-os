# Module 28 — POSIX Compliance: Design Decisions

## 1. POSIX wrappers in a separate posix.c — not folded into syscall.c

**Decision:** All POSIX-compatible wrappers live in `posix.c` and `posix.h`. The kernel's `syscall.c` retains its BobOS-internal names and negative error codes.

**Why:** Separating the POSIX adaptation layer from the syscall dispatch keeps two concerns cleanly split. The kernel's internal error model (SYSCALL_EBADF = -1, etc.) is optimised for fast dispatch code; the POSIX model (errno set on return, -1 returned) is optimised for user-space readability. Mixing them would require changing every syscall handler — a risky refactor. The translation layer is a thin mapping and the right place for that mapping is the boundary between the two models.

**Trade-off:** Programs must include `posix.h` explicitly to get POSIX names. They cannot just include `<unistd.h>` — there is no libc. A future step would be to compile posix.c into a user-space library archive and link against it automatically.

## 2. errno is a single global int — not thread-local storage

**Decision:** `int errno` is one global variable in posix.c. All processes and threads share it.

**Why:** Thread-local storage (TLS) on x86 requires the FS or GS segment register to point at a per-thread data block. Setting that up requires the OS to allocate TLS blocks, populate them in the TSS/GDT, and update them on context switch. That is a separate feature of non-trivial complexity. A global errno is wrong under concurrent access (thread A's errno gets clobbered by thread B's syscall) but correct for single-threaded processes, which is all our demo programs are.

**Trade-off:** A global errno is a latent bug in any multi-threaded program. POSIX mandates that errno is thread-local (C11 `_Thread_local`). The fix is to store `errno` in the PCB and expose it via a pointer through the FS segment, exactly as glibc does.

## 3. select() implemented as a kernel-side busy poll — no blocking

**Decision:** SYS_SELECT(25) takes a bitmask of readable fds, loops checking each one, and returns the set that are ready. It does not block — it polls once and returns.

**Why:** True blocking select() requires the process to sleep (PROC_WAITING) and wake when any watched fd becomes readable. That requires each fd to maintain a "wait queue" of sleeping processes, and the producer (NIC IRQ, keyboard ISR) to wake the queue. The wait-queue infrastructure is a full subsystem. Polling select() demonstrates the interface and the fd-set bitmask mechanics without that infrastructure.

**Trade-off:** A program that calls select() with no fds ready gets EAGAIN (errno=11) immediately instead of sleeping. The correct pattern is a yield loop: `while (select(...) == -1 && errno == EAGAIN) sys_yield();`. This is the same pattern epoll_wait uses internally before going to sleep.

## 4. fstat() returns a simplified struct stat — no timestamps, no permissions

**Decision:** `struct stat` contains only `st_ino`, `st_mode`, `st_size`, `st_blksize`, `st_blocks`. No `st_uid`, `st_gid`, `st_atime`, `st_mtime`, `st_ctime`.

**Why:** BobFS (Module 12) stores only a file name, size, and data. There is no timestamp on disk, no owner concept, no permission bits. Returning zeros for those fields would be misleading. The subset we return is exactly what the underlying filesystem knows. A future BobFS v2 could add an inode table with timestamps — then stat() returns them.

**Trade-off:** Programs that call `stat()` to check file permissions or modification times will see zeros and may misbehave. Acceptable for a demo OS.

## 5. socket()/bind()/sendto()/recvfrom() are thin wrappers around udp_*()

**Decision:** POSIX socket functions call `udp_bind()`, `udp_send()`, `udp_recvfrom()` directly. No generic socket layer.

**Why:** The BSD socket API is protocol-agnostic (AF_INET, AF_UNIX, AF_BLUETOOTH…). A real implementation has a `struct proto_ops` vtable per address family and dispatches socket() to the right one. We support exactly one: AF_INET/SOCK_DGRAM. The direct call is honest about what's implemented and adds no false abstraction.

**Trade-off:** `socket(AF_UNIX, ...)` returns EAFNOSUPPORT. A future module could add AF_UNIX (Unix domain sockets — just a pipe with a path) as a zero-network-stack exercise.

## 6. SYS_LSEEK and SYS_STAT added as new syscalls 26 and 27

**Decision:** Rather than overloading an existing syscall, two new numbers are allocated.

**Why:** Adding a new syscall number is the correct approach — it keeps the dispatch table simple, avoids versioning problems, and makes strace-style debugging straightforward. Syscall numbers are a stable ABI commitment (Linux has never removed a syscall); we follow the same discipline even in a toy OS.

**Trade-off:** The syscall table is now 27 entries. With MAX_PROCESSES=8 and 27 syscalls, the BobOS ABI is getting large enough that a syscall table array (instead of a switch) would be more maintainable. Module 28 is the right moment to notice that.
