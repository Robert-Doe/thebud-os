/*
 * vfs.c -- Virtual File System implementation
 *
 * Module 19: Dispatches open/read/write/close to registered backend ops.
 * Mount table maps path prefixes to vfs_ops vtables.
 * fd table lives in the PCB (accessed via vfs_get_fd_table()).
 */

#include <stdint.h>
#include "vfs.h"
#include "fs.h"
#include "vga.h"
#include "process.h"

/* -- Global mount table --------------------------------------------------- */

static struct vfs_mount mount_table[MAX_MOUNTS];

void vfs_init(void) {
    int i;
    for (i = 0; i < MAX_MOUNTS; i++) mount_table[i].valid = 0;
}

int vfs_mount(const char *prefix, struct vfs_ops *ops) {
    int i, j;
    for (i = 0; i < MAX_MOUNTS; i++) {
        if (!mount_table[i].valid) {
            for (j = 0; j < 31 && prefix[j]; j++)
                mount_table[i].prefix[j] = prefix[j];
            mount_table[i].prefix[j] = '\0';
            mount_table[i].ops   = ops;
            mount_table[i].valid = 1;
            return 0;
        }
    }
    return -1;
}

static struct vfs_ops *find_ops(const char *path) {
    int i, j, match;
    struct vfs_ops *best = 0;
    int best_len = -1;
    for (i = 0; i < MAX_MOUNTS; i++) {
        if (!mount_table[i].valid) continue;
        match = 1;
        for (j = 0; mount_table[i].prefix[j]; j++) {
            if (path[j] != mount_table[i].prefix[j]) { match = 0; break; }
        }
        if (match && j > best_len) { best = mount_table[i].ops; best_len = j; }
    }
    return best;
}

/* -- fd table helpers (delegates to process.c's current fd table) --------- */

int vfs_open(const char *path, int flags) {
    struct file_descriptor *fds = vfs_get_fd_table();
    struct vfs_ops *ops;
    int inode, slot, i;

    if (!fds) return -1;
    ops = find_ops(path);
    if (!ops) return -1;

    inode = ops->open(path, flags);
    if (inode < 0) return -1;

    /* Find free slot (skip 0/1/2 which are pre-opened) */
    slot = -1;
    for (i = 3; i < MAX_FDS; i++) {
        if (!fds[i].valid) { slot = i; break; }
    }
    if (slot < 0) { ops->close(inode); return -1; }

    fds[slot].valid  = 1;
    fds[slot].inode  = inode;
    fds[slot].offset = 0;
    fds[slot].ops    = ops;
    return slot;
}

int vfs_read(int fd, char *buf, int n) {
    struct file_descriptor *fds = vfs_get_fd_table();
    int r;
    if (!fds) return -1;
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].valid) return -1;
    r = fds[fd].ops->read(fds[fd].inode, buf, n, fds[fd].offset);
    if (r > 0) fds[fd].offset += r;
    return r;
}

int vfs_write(int fd, const char *buf, int n) {
    struct file_descriptor *fds = vfs_get_fd_table();
    int r;
    if (!fds) return -1;
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].valid) return -1;
    r = fds[fd].ops->write(fds[fd].inode, buf, n, fds[fd].offset);
    if (r > 0) fds[fd].offset += r;
    return r;
}

void vfs_close(int fd) {
    struct file_descriptor *fds = vfs_get_fd_table();
    if (!fds) return;
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].valid) return;
    fds[fd].ops->close(fds[fd].inode);
    fds[fd].valid = 0;
}

/* -- VGA backend (stdout / stderr) ---------------------------------------- */

static int vga_open(const char *path, int flags)
    { (void)path; (void)flags; return 1; }
static int vga_read(int inode, char *buf, int n, int off)
    { (void)inode; (void)buf; (void)n; (void)off; return 0; }
static int vga_write_fn(int inode, const char *buf, int n, int off)
    { int i; (void)inode; (void)off; for (i = 0; i < n; i++) vga_putchar(buf[i]); return n; }
static void vga_close_fn(int inode)
    { (void)inode; }

struct vfs_ops vga_ops = { vga_open, vga_read, vga_write_fn, vga_close_fn };

/* -- BobFS backend --------------------------------------------------------- */

static char bobfs_buf[4096];
static int  bobfs_sizes[8];
static char bobfs_names[8][32];
static int  bobfs_used[8];

static int bobfs_open(const char *path, int flags) {
    int i, r;
    (void)flags;
    for (i = 0; i < 8; i++) {
        if (!bobfs_used[i]) {
            r = fs_read(path, bobfs_buf, sizeof(bobfs_buf));
            if (r < 0) return -1;
            bobfs_sizes[i] = r;
            bobfs_used[i]  = 1;
            for (r = 0; r < 31 && path[r]; r++) bobfs_names[i][r] = path[r];
            bobfs_names[i][r] = '\0';
            return i;
        }
    }
    return -1;
}

static int bobfs_read(int inode, char *buf, int n, int offset) {
    int avail, i;
    if (inode < 0 || inode >= 8 || !bobfs_used[inode]) return -1;
    avail = bobfs_sizes[inode] - offset;
    if (avail <= 0) return 0;
    if (n > avail) n = avail;
    for (i = 0; i < n; i++) buf[i] = bobfs_buf[offset + i];
    return n;
}

static int bobfs_write(int inode, const char *buf, int n, int offset) {
    (void)inode; (void)buf; (void)n; (void)offset;
    return -1;  /* BobFS is read-only via this backend */
}

static void bobfs_close(int inode) {
    if (inode >= 0 && inode < 8) bobfs_used[inode] = 0;
}

struct vfs_ops bobfs_ops = { bobfs_open, bobfs_read, bobfs_write, bobfs_close };
