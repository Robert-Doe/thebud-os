/*
 * fs.c — BobFS flat file system implementation
 *
 * Three on-disk structures (all fit in individual 512-byte sectors):
 *
 *   Sector 0 : struct fs_super  (magic, version, file count, next free sector)
 *   Sector 1 : struct fs_dirent[16]  (directory — 16 × 32 bytes = 512 bytes)
 *   Sector 2+ : raw file data
 *
 * After fs_init() reads the superblock into `super` and the directory into
 * `dir[]`, all subsequent operations work on those in-memory copies and
 * flush them back to disk after every change.
 */

#include <stdint.h>
#include "fs.h"
#include "disk.h"

/* ── In-memory copies of on-disk structures ──────────────────────────── */

static struct fs_super   super;
static struct fs_dirent  dir[FS_MAX_FILES];
static int               fs_ready = 0;

/* ── Scratch sector buffer (static — never on the stack) ─────────────── */
static uint8_t sec_buf[FS_SECTOR_SIZE];

/* ── Tiny string utilities (no libc) ─────────────────────────────────── */

static int fs_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static void fs_strncpy(char *dst, const char *src, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n - 1u && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static uint32_t fs_strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

/* ── Superblock I/O ───────────────────────────────────────────────────── */

static int super_read(void) {
    uint32_t i;
    if (disk_read_sector(0, sec_buf) < 0) return -1;
    /* Copy byte by byte to avoid strict-aliasing issues. */
    for (i = 0; i < sizeof(super); i++)
        ((uint8_t *)&super)[i] = sec_buf[i];
    return 0;
}

static int super_write(void) {
    uint32_t i;
    uint32_t j;
    for (i = 0; i < FS_SECTOR_SIZE; i++) sec_buf[i] = 0;
    for (i = 0; i < sizeof(super); i++)
        sec_buf[i] = ((uint8_t *)&super)[i];
    (void)j;
    return disk_write_sector(0, sec_buf);
}

/* ── Directory I/O ───────────────────────────────────────────────────── */

static int dir_read(void) {
    uint32_t i;
    if (disk_read_sector(1, sec_buf) < 0) return -1;
    for (i = 0; i < sizeof(dir); i++)
        ((uint8_t *)dir)[i] = sec_buf[i];
    return 0;
}

static int dir_write(void) {
    uint32_t i;
    for (i = 0; i < FS_SECTOR_SIZE; i++) sec_buf[i] = 0;
    for (i = 0; i < sizeof(dir); i++)
        sec_buf[i] = ((uint8_t *)dir)[i];
    return disk_write_sector(1, sec_buf);
}

/* ── Public API ──────────────────────────────────────────────────────── */

int fs_init(void) {
    uint32_t i;

    if (!disk_present()) return -1;

    if (super_read() < 0) return -1;

    if (super.magic != FS_MAGIC || super.version != FS_VERSION) {
        /* Fresh disk — format it. */
        super.magic       = FS_MAGIC;
        super.version     = FS_VERSION;
        super.num_files   = 0;
        super.next_sector = FS_DATA_START;
        for (i = 0; i < FS_SECTOR_SIZE - 16u; i++) super.pad[i] = 0;

        for (i = 0; i < FS_MAX_FILES; i++) {
            dir[i].name[0]      = '\0';
            dir[i].size         = 0;
            dir[i].start_sector = 0;
            dir[i].flags        = 0;
        }

        if (super_write() < 0) return -1;
        if (dir_write()   < 0) return -1;
        fs_ready = 1;
        return 1;   /* freshly formatted */
    }

    /* Existing filesystem — load directory. */
    if (dir_read() < 0) return -1;
    fs_ready = 1;
    return 0;
}

int fs_create(const char *name, const char *data, uint32_t len) {
    uint32_t i, slot, sectors_needed, s, bytes_left, chunk;

    if (!fs_ready) return -1;
    if (!name || fs_strlen(name) >= FS_NAME_LEN) return -1;
    if (len > FS_MAX_FILE_SIZE) return -1;

    /* Check name does not already exist. */
    for (i = 0; i < FS_MAX_FILES; i++) {
        if (dir[i].flags && fs_strcmp(dir[i].name, name) == 0)
            return -2;
    }

    /* Find a free directory slot. */
    slot = FS_MAX_FILES;
    for (i = 0; i < FS_MAX_FILES; i++) {
        if (!dir[i].flags) { slot = i; break; }
    }
    if (slot == FS_MAX_FILES) return -1;   /* directory full */

    /* Allocate contiguous data sectors. */
    sectors_needed = (len + FS_SECTOR_SIZE - 1u) / FS_SECTOR_SIZE;
    if (sectors_needed == 0) sectors_needed = 1;

    dir[slot].start_sector = super.next_sector;
    dir[slot].size         = len;

    /* Write data sectors. */
    bytes_left = len;
    for (s = 0; s < sectors_needed; s++) {
        uint32_t j;
        chunk = bytes_left > FS_SECTOR_SIZE ? FS_SECTOR_SIZE : bytes_left;
        for (j = 0; j < chunk; j++)
            sec_buf[j] = (uint8_t)data[s * FS_SECTOR_SIZE + j];
        for (j = chunk; j < FS_SECTOR_SIZE; j++)
            sec_buf[j] = 0;    /* zero-pad the last sector */
        if (disk_write_sector(super.next_sector + s, sec_buf) < 0)
            return -1;
        bytes_left -= chunk;
    }

    /* Commit directory entry. */
    fs_strncpy(dir[slot].name, name, FS_NAME_LEN);
    dir[slot].flags = 1;

    /* Advance the free-sector pointer and update superblock. */
    super.next_sector += sectors_needed;
    super.num_files++;

    if (dir_write()   < 0) return -1;
    if (super_write() < 0) return -1;
    return 0;
}

int fs_read(const char *name, char *buf, uint32_t max_len) {
    uint32_t i, sectors, s, bytes_left, chunk, total = 0;

    if (!fs_ready) return -1;

    /* Find the file. */
    for (i = 0; i < FS_MAX_FILES; i++) {
        if (dir[i].flags && fs_strcmp(dir[i].name, name) == 0) {
            uint32_t size = dir[i].size < max_len ? dir[i].size : max_len;
            sectors    = (size + FS_SECTOR_SIZE - 1u) / FS_SECTOR_SIZE;
            bytes_left = size;

            for (s = 0; s < sectors; s++) {
                uint32_t j;
                if (disk_read_sector(dir[i].start_sector + s, sec_buf) < 0)
                    return -1;
                chunk = bytes_left > FS_SECTOR_SIZE ? FS_SECTOR_SIZE : bytes_left;
                for (j = 0; j < chunk; j++)
                    buf[total + j] = (char)sec_buf[j];
                total      += chunk;
                bytes_left -= chunk;
            }
            if (total < max_len) buf[total] = '\0';
            return (int)total;
        }
    }
    return -1;   /* not found */
}

int fs_delete(const char *name) {
    uint32_t i;
    if (!fs_ready) return -1;

    for (i = 0; i < FS_MAX_FILES; i++) {
        if (dir[i].flags && fs_strcmp(dir[i].name, name) == 0) {
            dir[i].flags   = 0;
            dir[i].name[0] = '\0';
            super.num_files--;
            dir_write();
            super_write();
            return 0;
        }
    }
    return -1;
}

void fs_list(void (*cb)(const char *name, uint32_t size)) {
    uint32_t i;
    if (!fs_ready || !cb) return;
    for (i = 0; i < FS_MAX_FILES; i++) {
        if (dir[i].flags) cb(dir[i].name, dir[i].size);
    }
}

uint32_t fs_num_files(void) {
    return fs_ready ? super.num_files : 0;
}
