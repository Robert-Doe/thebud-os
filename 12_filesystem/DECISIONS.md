# Module 12 — File System: Design Decisions

## What this module adds

A persistent file system (BobFS) backed by a raw ATA hard disk.  The kernel
can create, read, list, and delete named files that survive reboots.  A minimal
ATA PIO driver handles all disk I/O using only CPU instructions — no DMA, no
BIOS.

---

## Decision 1: ATA PIO instead of DMA

**Why PIO?**
DMA (Direct Memory Access) lets the disk controller write sectors straight to
RAM without CPU involvement, which is faster for large transfers.  But DMA
requires programming a DMA controller (the 8237A or its APIC-era successors),
mapping I/O buffers to physical addresses, and handling IRQ acknowledgements
properly.  That is another 200–400 lines of tricky, hardware-specific code.

PIO (Programmed I/O) is the fallback mode every ATA device supports.  The CPU
explicitly reads or writes each 16-bit word from/to port 0x1F0.  For a sector
of 512 bytes that is 256 word-transfers, which takes microseconds on a GHz CPU.
At our OS's speed (no parallelism beyond the scheduler, no network, no GPU) PIO
is fast enough to be invisible.

**QEMU compatibility:** QEMU's emulated IDE controller supports PIO perfectly
and requires no special flags.  Adding `-hda disk.img` is all that is needed.

---

## Decision 2: LBA28 addressing

Hard disks were historically addressed by Cylinder/Head/Sector (CHS), a
geometric coordinate system tied to actual disk platters.  CHS is limited to
~8 GB and requires knowing the drive geometry.

LBA28 (Logical Block Addressing, 28-bit) treats the disk as a flat array of
512-byte sectors numbered 0 to 2²⁸−1 (~137 GB).  The ATA registers for LBA28:

```
Port 0x1F6 (DRIVE/HEAD): 0xE0 | (lba[27:24])   — 0xE0 = LBA mode + drive 0
Port 0x1F3 (LBA LOW)   : lba[7:0]
Port 0x1F4 (LBA MID)   : lba[15:8]
Port 0x1F5 (LBA HIGH)  : lba[23:16]
```

This is simpler to program than CHS and works for any disk image up to 137 GB.

---

## Decision 3: Polling (busy-wait) instead of IRQ-driven I/O

The ATA controller can raise an IRQ when a transfer completes.  IRQ-driven I/O
would let the CPU do other work while the disk spins up.  However:

- We are already in the interrupt handler path (scheduler runs on IRQ0).
  Nesting disk IRQs inside the scheduler complicates the stack.
- QEMU's emulated disk responds instantaneously; polling exits on the first
  check in practice.
- Adding IRQ-driven disk I/O requires wiring IRQ14/15, managing a completion
  flag, and sleeping the calling process — essentially async I/O, which is
  a full module's worth of complexity.

Polling is correct and simple.  A real kernel would switch to IRQ-driven I/O
for multi-process throughput.

---

## Decision 4: 16-bit word reads at the data port

The ATA DATA port (0x1F0) is a 16-bit port.  Reading or writing it 8 bits at a
time is not allowed by the ATA specification and produces garbage on real
hardware (and on strict emulators).  Our shared `io.h` only has byte-width
`inb`/`outb`.

Rather than widen `io.h` (which would change a shared header used by 11 modules
already), `disk.c` defines its own local static inline helpers:

```c
static inline uint16_t ata_inw(uint16_t port) { ... }
static inline void     ata_outw(uint16_t port, uint16_t val) { ... }
```

These are private to `disk.c` and invisible to the rest of the kernel.  The
`inw`/`outw` x86 instructions access the full 16-bit data bus.

---

## Decision 5: BobFS — a flat, append-only filesystem

**Design philosophy:** keep the on-disk format readable in five minutes.

```
Sector 0  : Superblock  (512 bytes)
Sector 1  : Directory   (16 entries × 32 bytes = 512 bytes)
Sector 2+ : File data   (contiguous, one file after another)
```

### Superblock (sector 0)

```c
struct fs_super {
    uint32_t magic;        // 0x424F5342 = 'BOSB'
    uint32_t version;      // 1
    uint32_t num_files;    // count of live entries
    uint32_t next_sector;  // bump pointer for allocation
    uint8_t  pad[496];     // unused — fills exactly 512 bytes
};
```

The magic number lets `fs_init()` distinguish a freshly zeroed disk image from
an existing BobFS volume.  If the magic is wrong, BobFS writes a blank
superblock and directory — auto-format on first boot.

### Directory entry (32 bytes each)

```c
struct fs_dirent {
    char     name[20];     // filename, null-terminated
    uint32_t size;         // byte count
    uint32_t start_sector; // first data sector
    uint32_t flags;        // 0=free, 1=present
};
```

20 + 4 + 4 + 4 = 32 bytes exactly.  16 entries × 32 bytes = 512 bytes = 1
sector.  The directory always occupies exactly one sector.

### Why no sub-directories?

A hierarchical directory would require either:
- Recursive directory entries (each entry can point to another directory
  sector), or
- An inode table where directories are just files containing entry lists.

Both need pointer-chasing and recursive sector reads.  A flat namespace handles
the shell use case (dozens of files) and keeps `fs_read`/`fs_create` each under
30 lines.

### Contiguous allocation (bump pointer)

When a file is created, `next_sector` is the starting sector.  The file takes
`ceil(size / 512)` sectors, then `next_sector` advances.  This is O(1)
allocation with zero metadata overhead per sector.

**Trade-off:** deleted files leave a gap.  The data sectors are never reclaimed.
On a 4096-sector disk image (2 MB) this is fine for the demo.  A production FS
would add a free-list or an extent bitmap.

---

## Decision 6: Static sector buffer in fs.c

Every `fs_read` / `fs_create` / `fs_delete` call needs a 512-byte scratch
buffer for `disk_read_sector` / `disk_write_sector`.  Allocating 512 bytes on
the kernel stack per call:

- Could overflow the 4 KB per-process stack when deep in a call chain.
- Is wasteful since only one disk operation runs at a time (no concurrency).

Solution: one static `uint8_t sec_buf[512]` in `fs.c`.  It is reused across
calls.  This is safe because BobFS has no concurrency — `fs_*` functions are
not re-entrant and interrupts are irrelevant to the buffer's lifetime.

Similarly, `kernel.c` uses a static `read_buf[]` for the demo reads.

---

## Decision 7: disk.img separate from os.img

The bootable floppy image (`os.img`) is read-only from the OS's perspective —
it is the boot medium.  Mixing the filesystem onto the floppy would require
managing a FAT or raw offset on the floppy, complicating the bootloader.

QEMU supports two separate block devices:

```
-fda os.img   # floppy: boot sector + kernel
-hda disk.img # hard disk: BobFS data
```

The hard disk image is a plain file of zeros (`dd if=/dev/zero ...`).  BobFS
auto-formats it on first boot.  The image persists between QEMU runs, so data
survives "reboots" — this is the first time BobOS achieves real persistence.

---

## Key lessons from this module

| Concept | Takeaway |
|---|---|
| ATA PIO | All disk I/O can be done with 7 I/O ports and two instructions per word |
| LBA28 | A flat sector index is far simpler than CHS geometry |
| Polling | Correct and simple when the disk is fast and the system is single-threaded |
| Superblock magic | One 4-byte field distinguishes blank media from a valid volume |
| Flat directory | 16 × 32-byte entries fit in one sector; no tree traversal needed |
| Bump allocation | O(1) file creation with zero per-sector overhead |
| Static buffer | Avoids stack overflow from large on-stack arrays in interrupt context |
| Dual images | Separating boot media from data storage keeps both concerns clean |
