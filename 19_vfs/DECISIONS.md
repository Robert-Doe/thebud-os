# Module 19 — VFS Layer: Design Decisions

## 1. VFS as a vtable (vfs_ops struct) rather than a union or switch

**Decision:** Each filesystem or device backend is represented by a `struct vfs_ops` containing four function pointers: `open`, `read`, `write`, `close`.  Routing a call means looking up the mount table and calling through the pointer.

**Why:** A vtable lets new backends be added without modifying the VFS core — the same pattern used by Linux's `file_operations`.  A switch/union would require editing the dispatcher for every new device type, which breaks the open/closed principle.

**Trade-off:** Indirect function calls are slightly slower than a direct switch, but at OS-learning scale the clarity wins overwhelmingly.

## 2. fd_table stored in the PCB, not globally

**Decision:** Each `struct process` owns a `fd_table[MAX_FDS]` array of `file_descriptor` structs.  There is no global fd table.

**Why:** File descriptors are per-process in POSIX — a child's `close(fd)` must not affect the parent's open file.  Embedding the table in the PCB makes fork's copy-on-open semantics trivial: `process_fork` just copies the array into the child's PCB.  A global table would require reference counting and locking that is premature at this stage.

**Trade-off:** Each PCB is larger (16 × sizeof(file_descriptor) ≈ 192 bytes).  Given MAX_PROCESSES=8, the total overhead is under 2 KB — negligible.

## 3. Pre-opening fd 0/1/2 on every process creation

**Decision:** `process_init`, `process_create`, and `process_fork` all set fd 0, 1, 2 to valid entries pointing at `vga_ops`.  User programs receive stdin/stdout/stderr already open.

**Why:** POSIX programs assume fd 0/1/2 exist on entry.  Requiring user programs to open them explicitly would break the convention that `write(1, buf, n)` is always safe.  The pre-open also means existing `SYS_WRITE(fd=1, ...)` calls in earlier demo programs continue to work through the VFS layer without any change.

**Trade-off:** Slightly wasteful for kernel-only processes (the idle task) that never use user-space I/O.  The cost is three valid `file_descriptor` slots per process — acceptable.

## 4. BobFS backend is read-only via vfs_ops

**Decision:** `bobfs_ops.write` always returns -1.  Kernel code that needs to create files calls `fs_create` directly; user programs can only read.

**Why:** BobFS has no locking and no concurrent-write semantics.  Exposing writes through the VFS now would require the full write path (seek, partial-page write, flush) that belongs in a later module.  Making writes fail explicitly prevents silent corruption.

**Trade-off:** User programs cannot write files in Module 19.  This is deferred to Module 25+ when a writable FS backend is designed properly.

## 5. Byte offset tracked in file_descriptor, not in the backend

**Decision:** The `offset` field in `struct file_descriptor` is advanced by `vfs_read` and `vfs_write` after each call.  Backends receive the offset as a parameter and do not maintain it internally.

**Why:** Stateless backends are easier to reason about and reuse — the same `bobfs_ops` can be opened multiple times (each with its own fd and offset) without the backend needing to know which caller is which.  This mirrors how POSIX file positions work: they belong to the open file description, not the underlying inode.

**Trade-off:** Backends cannot implement seeking themselves without the VFS passing the current offset down.  That is fine — a `SYS_LSEEK` syscall (deferred to a later module) would just modify `fd_table[fd].offset` directly.

## 6. Path routing via mount prefix, not a full VFS namespace tree

**Decision:** The mount table stores simple string prefixes.  `find_ops` walks the table and picks the entry whose prefix matches the longest initial substring of the path.  There is no inode tree, no directory lookup, no `stat`.

**Why:** A namespace tree requires directory inodes, dentries, and a recursive lookup algorithm — that is a significant chunk of a real VFS.  For BobOS at this stage, two mount points (`/` for BobFS, `vga:` for the console) cover 100% of current use cases.  The longest-prefix rule correctly disambiguates overlapping mounts without a tree walk.

**Trade-off:** Paths like `/dev/tty` would mis-route unless a `/dev/` mount is added explicitly.  This is not a problem until we have a real device filesystem, which is beyond the current scope.
