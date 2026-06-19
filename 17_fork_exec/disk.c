/*
 * disk.c — ATA PIO hard-disk driver implementation
 *
 * ── ATA port map (primary bus) ────────────────────────────────────────────
 *
 *  Port    Name              Role
 *  0x1F0   DATA              Read/write 16-bit words (512 bytes = 256 words)
 *  0x1F1   ERROR / FEATURES  Read: error bits.  Write: feature select.
 *  0x1F2   SECTOR COUNT      Number of sectors to transfer (we always use 1)
 *  0x1F3   LBA LOW           Bits  0-7  of the 28-bit LBA address
 *  0x1F4   LBA MID           Bits  8-15
 *  0x1F5   LBA HIGH          Bits 16-23
 *  0x1F6   DRIVE/HEAD        Bits 24-27 of LBA (lower nibble) + mode bits
 *  0x1F7   STATUS / COMMAND  Read: status flags.  Write: command byte.
 *
 * ── Status register bits ─────────────────────────────────────────────────
 *
 *  Bit 7  BSY  — drive is busy; no other register is valid
 *  Bit 6  DRDY — drive is ready to accept a command
 *  Bit 3  DRQ  — data request; drive is ready to transfer a sector
 *  Bit 0  ERR  — an error occurred; see ERROR register for details
 *
 * ── LBA28 read sequence ───────────────────────────────────────────────────
 *
 *  1. Wait until BSY=0.
 *  2. Write 0xE0|(lba>>24 & 0xF) to DRIVE/HEAD.   (0xE0 = LBA mode, drive 0)
 *  3. Write 1 to SECTOR COUNT.
 *  4. Write lba[7:0], lba[15:8], lba[23:16] to LBA LOW/MID/HIGH.
 *  5. Write 0x20 (READ SECTORS) to COMMAND.
 *  6. Wait until DRQ=1 (and ERR=0).
 *  7. Read 256 16-bit words from DATA port → 512 bytes.
 *
 * ── LBA28 write sequence ─────────────────────────────────────────────────
 *
 *  Steps 1-4 same as read.
 *  5. Write 0x30 (WRITE SECTORS) to COMMAND.
 *  6. Wait until DRQ=1.
 *  7. Write 256 16-bit words to DATA port.
 *  8. Write 0xE7 (FLUSH CACHE) to COMMAND; wait until BSY=0.
 */

#include <stdint.h>
#include "disk.h"

/* ── ATA port addresses ──────────────────────────────────────────────── */
#define ATA_DATA     0x1F0u
#define ATA_ERROR    0x1F1u
#define ATA_COUNT    0x1F2u
#define ATA_LBA_LO   0x1F3u
#define ATA_LBA_MID  0x1F4u
#define ATA_LBA_HI   0x1F5u
#define ATA_DRIVE    0x1F6u
#define ATA_STATUS   0x1F7u
#define ATA_CMD      0x1F7u

/* ── Status bits ─────────────────────────────────────────────────────── */
#define ATA_BSY  0x80u
#define ATA_DRQ  0x08u
#define ATA_ERR  0x01u

/* ── Commands ────────────────────────────────────────────────────────── */
#define CMD_READ   0x20u
#define CMD_WRITE  0x30u
#define CMD_FLUSH  0xE7u

/* ── Local port I/O helpers (16-bit words for the data register) ─────── */

static inline uint8_t  ata_inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline uint16_t ata_inw(uint16_t port) {
    uint16_t v;
    __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void ata_outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void ata_outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* ── Internal helpers ────────────────────────────────────────────────── */

static int g_disk_ok = 0;

/* Spin until BSY clears.  No timeout — in a real driver you'd add one. */
static void ata_wait_bsy(void) {
    while (ata_inb(ATA_STATUS) & ATA_BSY) {}
}

/* Spin until DRQ sets (data ready) or ERR sets.
 * Returns 0 if DRQ set, -1 if ERR set. */
static int ata_wait_drq(void) {
    uint8_t s;
    for (;;) {
        s = ata_inb(ATA_STATUS);
        if (s & ATA_ERR) return -1;
        if (s & ATA_DRQ) return 0;
    }
}

/* Set up the drive/LBA registers common to both read and write. */
static void ata_setup_lba(uint32_t lba, uint8_t count) {
    ata_outb(ATA_DRIVE,   (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    ata_outb(ATA_COUNT,   count);
    ata_outb(ATA_LBA_LO,  (uint8_t)(lba));
    ata_outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    ata_outb(ATA_LBA_HI,  (uint8_t)(lba >> 16));
}

/* ── Public API ──────────────────────────────────────────────────────── */

void disk_init(void) {
    uint8_t s = ata_inb(ATA_STATUS);
    /* 0xFF = floating bus (no drive), 0x00 = unpowered */
    if (s == 0xFF || s == 0x00) {
        g_disk_ok = 0;
        return;
    }
    ata_wait_bsy();
    g_disk_ok = 1;
}

int disk_present(void) {
    return g_disk_ok;
}

int disk_read_sector(uint32_t lba, uint8_t *buf) {
    int i;
    if (!g_disk_ok) return -1;

    ata_wait_bsy();
    ata_setup_lba(lba, 1);
    ata_outb(ATA_CMD, CMD_READ);

    if (ata_wait_drq() < 0) return -1;

    /* Read 256 16-bit words = 512 bytes. */
    for (i = 0; i < 256; i++) {
        uint16_t w = ata_inw(ATA_DATA);
        buf[i * 2]     = (uint8_t)(w);
        buf[i * 2 + 1] = (uint8_t)(w >> 8);
    }
    return 0;
}

int disk_write_sector(uint32_t lba, const uint8_t *buf) {
    int i;
    if (!g_disk_ok) return -1;

    ata_wait_bsy();
    ata_setup_lba(lba, 1);
    ata_outb(ATA_CMD, CMD_WRITE);

    if (ata_wait_drq() < 0) return -1;

    /* Write 256 16-bit words = 512 bytes. */
    for (i = 0; i < 256; i++) {
        uint16_t w = (uint16_t)buf[i * 2] | ((uint16_t)buf[i * 2 + 1] << 8);
        ata_outw(ATA_DATA, w);
    }

    /* Flush cache to ensure data is committed to the storage medium. */
    ata_outb(ATA_CMD, CMD_FLUSH);
    ata_wait_bsy();
    return 0;
}
