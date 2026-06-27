# Module 27 — Device Driver Framework: Design Decisions

## 1. driver_ops vtable — same pattern as vfs_ops

**Decision:** Each driver fills a `struct driver_ops` with function pointers for `probe`, `read`, `write`, `ioctl`. This is identical in spirit to `struct vfs_ops` from Module 19.

**Why:** You already understand VFS vtables. Reusing the same pattern for drivers means zero new abstractions to learn — just a new table. Linux uses exactly this pattern: `struct file_operations` in the kernel covers both filesystem files and device files with the same vtable shape.

**Trade-off:** A production driver framework also has `suspend`/`resume` (power management), `mmap` (for framebuffers), `poll` (for select/epoll), and `release` (ref-counted teardown). All omitted here — the lesson is registration and dispatch, not the full lifecycle.

## 2. driver_register() called before kernel_main() — static registration

**Decision:** Drivers call `driver_register(&ops)` before the kernel's main init sequence runs `driver_probe_all()`. No dynamic loading, no hotplug.

**Why:** Dynamic loading requires a module loader (ELF relocation into kernel address space at runtime), permission checks, and symbol resolution — its own full module. Static registration is sufficient to demonstrate the probe→activate→expose-via-devfs lifecycle that every OS driver goes through.

**Trade-off:** All drivers are compiled into the kernel image. Adding a new driver requires a full kernel rebuild. Linux's loadable kernel modules (LKM) solve this at the cost of significant complexity.

## 3. devfs as a zero-footprint VFS backend

**Decision:** devfs is not a real filesystem — it has no on-disk format, no inode table, no superblock. It is a VFS mount point whose `open()` is a `driver_find()` call and whose `read/write/ioctl` are vtable dispatches.

**Why:** The VFS layer (Module 19) already dispatches open/read/write to backend `vfs_ops` structs. devfs plugs in as another backend without touching the VFS dispatch code. This is exactly how Linux's devtmpfs works — the kernel creates device nodes in a RAM-backed tmpfs, but the actual I/O goes to the driver's `file_operations`.

**Trade-off:** Our devfs has no permissions, no major/minor numbers visible to user space, and no `ls /dev` support. A real devfs exposes an inode for each active driver and lets `stat()` return the device type/number.

## 4. driver_ops pointer stored in fd_table inode field

**Decision:** When `devfs_open()` matches a driver, it stores the `driver_ops *` pointer in `fd->inode` (repurposed as a `uint32_t`-sized word). `devfs_read/write/ioctl` cast it back to a pointer.

**Why:** The fd_table already has an `inode` field. Rather than add a new union or a second pointer field (which would increase PCB size for all processes), we repurpose the existing storage. The field is only meaningful while the fd is open — and when it's a devfs fd, there is no numeric inode anyway.

**Trade-off:** Storing a pointer as `uint32_t` is correct on 32-bit x86 but would break on 64-bit (pointers are 8 bytes). The fix for 64-bit is to use `uintptr_t` — which we do via the `FD_DRIVER` cast macro.

## 5. SYS_IOCTL (syscall 24) exposes device control to user space

**Decision:** A single ioctl syscall takes `(fd, cmd, arg)` and dispatches to `devfs_ioctl()` for device fds.

**Why:** Some operations don't fit read/write — for example, setting a serial port's baud rate, querying a NIC's MAC address, or flushing a keyboard buffer. ioctl is the traditional Unix catch-all for device control. The POSIX standard explicitly allows it as a device-specific escape hatch.

**Trade-off:** ioctl is notoriously stringly-typed and hard to audit. Linux's `cmd` numbers are structured (direction bits + type + number + size), but that structure is not enforced by the kernel. For our purposes, `cmd` is just a small integer that the driver interprets however it wants.
