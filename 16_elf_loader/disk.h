/*
 * disk.h — ATA PIO hard-disk driver
 *
 * Provides bare-metal read/write of 512-byte sectors from the primary ATA
 * bus (the first hard drive, which QEMU exposes when you pass -hda disk.img).
 *
 * We use LBA28 (Logical Block Addressing, 28-bit), the simplest mode:
 *   - No CHS geometry calculations.
 *   - Sectors are numbered 0, 1, 2, … up to 2^28 - 1 (128 GB).
 *   - All access goes through 7 I/O ports on the primary ATA bus (0x1F0-0x1F7).
 *
 * We use PIO (Programmed I/O) rather than DMA.  In PIO mode, the CPU reads
 * and writes every byte of every sector directly via port instructions.  DMA
 * is faster but far more complex to set up — PIO is perfect for learning.
 */

#ifndef DISK_H
#define DISK_H

#include <stdint.h>

/*
 * disk_init() — detect the primary ATA drive.
 * Checks the status register; if no drive is present, marks disk absent.
 * Must be called before any read/write.
 */
void disk_init(void);

/* Returns 1 if a disk was detected, 0 otherwise. */
int  disk_present(void);

/*
 * disk_read_sector(lba, buf)
 * Read one 512-byte sector at logical block address `lba` into `buf`.
 * Returns 0 on success, -1 on error (no disk, ATA error bit set).
 * `buf` must point to at least 512 bytes of writable memory.
 */
int  disk_read_sector(uint32_t lba, uint8_t *buf);

/*
 * disk_write_sector(lba, buf)
 * Write 512 bytes from `buf` to logical block address `lba`.
 * Returns 0 on success, -1 on error.
 * Issues a FLUSH CACHE command afterwards to ensure the data is committed.
 */
int  disk_write_sector(uint32_t lba, const uint8_t *buf);

#endif /* DISK_H */
