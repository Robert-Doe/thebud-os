# Module 20 — Pipes & IPC: Design Decisions

## 1. Circular ring buffer — no malloc, fixed-size PIPE_BUF

**Decision:** Each pipe slot is a `struct pipe_buf` with a fixed 4096-byte `char data[]` array, a head index, a tail index, and a count.  The buffer lives in kernel BSS and is never dynamically allocated.

**Why:** Dynamic allocation for pipe data would require `kmalloc` and introduce fragmentation.  A fixed-size ring is constant-time for both enqueue and dequeue, has no allocation overhead, and is cache-friendly.  4 KB matches a page frame — a natural unit for in-kernel buffers.

**Trade-off:** Large writes that exceed 4 KB will be truncated to what fits.  This is acceptable for an educational OS; production kernels (Linux) use a linked list of pages per pipe.

## 2. Two separate vfs_ops structs (pipe_read_ops, pipe_write_ops)

**Decision:** The read end and write end of a pipe have distinct vtables.  `pipe_read_ops.write` is a no-op returning -1; `pipe_write_ops.read` returns 0.

**Why:** Pipes are inherently asymmetric — one end reads, one end writes.  Encoding this asymmetry in the vtable means a `vfs_write(read_fd, ...)` call is correctly rejected without any extra fd-direction flag.  Using a single vtable with a direction flag would couple the read and write paths unnecessarily.

**Trade-off:** Two vtable structs instead of one.  The extra 32 bytes of rodata is trivial.

## 3. No blocking — read returns 0 when empty

**Decision:** `pipe_read_fn` returns 0 (not -1, not a sleep) when the buffer has no data.  There is no scheduler integration to block the reader until data arrives.

**Why:** Blocking I/O requires a "wake on data" mechanism (a waitqueue, condition variable, or event flag) which belongs in a later module on synchronization primitives.  Returning 0 early is the minimal correct behavior: the caller can retry or yield.  It matches the demo use case where the child writes before the parent reads (guaranteed by fork + scheduler ordering).

**Trade-off:** Polling is CPU-wasteful in a production system.  This simplification is explicitly noted so the reader knows to expect blocking in a future module.

## 4. Pipe slot index used as the inode number in fd_table

**Decision:** When `pipe_alloc` sets up fd entries, it stores the `slot` index (0-7) as `fd_table[fd].inode`.  The vtable function receives this as its first argument.

**Why:** The VFS inode field is already a generic integer handle — its meaning is entirely up to the backend.  Reusing it as a pipe table index avoids any extra indirection structure.  This is the same pattern the BobFS backend uses (bobfs inode index) and keeps all backends consistent.

**Trade-off:** Inode numbers are not globally unique across backends (pipe slot 2 and bobfs inode 2 are different things).  Since the vtable dispatches before the inode is interpreted, this is not a problem in practice.

## 5. SYS_PIPE takes a user pointer to int[2] — matches POSIX pipe(2) API

**Decision:** `do_pipe(int *fds)` receives the address of the user's `int fds[2]` array and writes both fd numbers directly into it.

**Why:** POSIX `pipe(2)` has the same interface.  User programs written for this module will be recognizable to any Unix programmer.  Returning two values via a pointer is the idiomatic C way to return multiple outputs from a function that already uses its return value for error status.

**Trade-off:** The kernel writes directly into user virtual memory.  This requires the user pointer to be valid — no bounds checking is done.  A hardened kernel would validate the pointer range before writing.
