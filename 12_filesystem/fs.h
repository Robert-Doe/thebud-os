/*
 * fs.h — BobFS: a minimal flat file system
 *
 * BobFS is a deliberately simple file system designed to be read in one
 * sitting.  It has no directories (everything is in one flat namespace),
 * no fragmentation (files are stored contiguously), and no access control.
 *
 * ── Disk layout ─────────────────────────────────────────────────────────
 *
 *   Sector 0  : Superblock   — magic number, file count, next free sector
 *   Sector 1  : Directory    — 16 × 32-byte directory entries (fits exactly)
 *   Sectors 2+ : File data   — raw bytes of every file, stored end-to-end
 *
 * ── Directory entry (32 bytes each, 16 per 512-byte sector) ─────────────
 *
 *   char     name[20]       — null-terminated filename
 *   uint32_t size           — file size in bytes
 *   uint32_t start_sector   — first data sector
 *   uint32_t flags          — 0=free slot, 1=file present
 *
 * ── Allocation ────────────────────────────────────────────────────────
 *
 *   Files are allocated by bumping `next_sector` in the superblock.  Once
 *   written, a file's data cannot be extended.  Deletion marks the directory
 *   entry free but does NOT reclaim data sectors (no garbage collection).
 *   This keeps the implementation to ~150 lines of C.
 */

#ifndef FS_H
#define FS_H

#include <stdint.h>

/* ── Limits ──────────────────────────────────────────────────────────── */
#define FS_MAGIC        0x424F5342u   /* 'B','O','S','B' */
#define FS_VERSION      1u
#define FS_MAX_FILES    16u
#define FS_NAME_LEN     20u
#define FS_SECTOR_SIZE  512u
#define FS_DATA_START   2u            /* first usable data sector */
#define FS_MAX_FILE_SIZE (64u * FS_SECTOR_SIZE)  /* 32 KB per file */

/* ── On-disk structures ───────────────────────────────────────────────
 * Both structs are packed so their in-memory layout matches the disk layout.
 */

struct fs_super {
    uint32_t magic;         /* must equal FS_MAGIC            */
    uint32_t version;       /* must equal FS_VERSION          */
    uint32_t num_files;     /* count of used directory slots  */
    uint32_t next_sector;   /* next free data sector to allocate */
    uint8_t  pad[FS_SECTOR_SIZE - 16u]; /* pad to 512 bytes   */
} __attribute__((packed));

struct fs_dirent {
    char     name[FS_NAME_LEN]; /* filename, null-terminated      */
    uint32_t size;              /* file size in bytes             */
    uint32_t start_sector;      /* first sector of file data      */
    uint32_t flags;             /* 0 = free, 1 = file present     */
} __attribute__((packed));
/* 20 + 4 + 4 + 4 = 32 bytes  →  16 entries × 32 = 512 = 1 sector  ✓ */

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * fs_init() — attach to the disk and read the superblock.
 * If the disk is blank (magic mismatch), writes a fresh superblock
 * and empty directory automatically.
 * Returns  0 : existing filesystem found
 *          1 : fresh filesystem written
 *         -1 : no disk detected
 */
int  fs_init(void);

/*
 * fs_create(name, data, len) — create a new file.
 * Allocates contiguous data sectors, writes the data, updates the directory
 * and superblock.
 * Returns  0 on success
 *         -1 if directory is full, no disk, or name too long
 *         -2 if a file with that name already exists
 */
int  fs_create(const char *name, const char *data, uint32_t len);

/*
 * fs_read(name, buf, max_len) — read a file into buf.
 * Reads up to max_len bytes (or the whole file, whichever is smaller).
 * Writes a null terminator after the last byte if there is room.
 * Returns number of bytes read, or -1 if file not found.
 */
int  fs_read(const char *name, char *buf, uint32_t max_len);

/*
 * fs_delete(name) — remove a file.
 * Marks its directory entry free; data sectors are NOT reclaimed.
 * Returns 0 on success, -1 if not found.
 */
int  fs_delete(const char *name);

/*
 * fs_list(cb) — iterate over all present files.
 * Calls cb(name, size) once for each live directory entry.
 */
void fs_list(void (*cb)(const char *name, uint32_t size));

/* Returns the number of live files. */
uint32_t fs_num_files(void);

#endif /* FS_H */
