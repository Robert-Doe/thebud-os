/*
 * pipe.c -- Anonymous pipe implementation
 *
 * Module 20: Circular ring buffer in kernel memory.  Two vfs_ops vtables
 * expose the read and write ends as ordinary file descriptors.
 * The pipe slot index is used as the inode number in the fd table.
 */

#include <stdint.h>
#include "pipe.h"
#include "vfs.h"
#include "process.h"

static struct pipe_buf pipe_table[MAX_PIPES];

void pipe_init(void) {
    int i;
    for (i = 0; i < MAX_PIPES; i++) {
        pipe_table[i].head       = 0;
        pipe_table[i].tail       = 0;
        pipe_table[i].count      = 0;
        pipe_table[i].read_open  = 0;
        pipe_table[i].write_open = 0;
        pipe_table[i].valid      = 0;
    }
}

/* -- Internal circular-buffer helpers ------------------------------------- */

static int pipe_write_fn(int slot, const char *buf, int n, int off) {
    struct pipe_buf *p;
    int written;
    (void)off;
    if (slot < 0 || slot >= MAX_PIPES || !pipe_table[slot].valid) return -1;
    p = &pipe_table[slot];
    written = 0;
    while (written < n && p->count < PIPE_BUF) {
        p->data[p->tail % PIPE_BUF] = buf[written++];
        p->tail++;
        p->count++;
    }
    return written;
}

static int pipe_read_fn(int slot, char *buf, int n, int off) {
    struct pipe_buf *p;
    int nread;
    (void)off;
    if (slot < 0 || slot >= MAX_PIPES || !pipe_table[slot].valid) return -1;
    p = &pipe_table[slot];
    nread = 0;
    while (nread < n && p->count > 0) {
        buf[nread++] = p->data[p->head % PIPE_BUF];
        p->head++;
        p->count--;
    }
    return nread;
}

/* -- VFS ops: read end ---------------------------------------------------- */

static int pipe_open_read(const char *path, int flags)
    { (void)path; (void)flags; return -1; } /* not used via vfs_open */
static int pipe_read_op(int slot, char *buf, int n, int off)
    { return pipe_read_fn(slot, buf, n, off); }
static int pipe_write_nop(int slot, const char *buf, int n, int off)
    { (void)slot; (void)buf; (void)n; (void)off; return -1; }
static void pipe_close_read(int slot) {
    if (slot < 0 || slot >= MAX_PIPES) return;
    pipe_table[slot].read_open--;
    if (pipe_table[slot].read_open <= 0 && pipe_table[slot].write_open <= 0)
        pipe_table[slot].valid = 0;
}

struct vfs_ops pipe_read_ops = {
    pipe_open_read, pipe_read_op, pipe_write_nop, pipe_close_read
};

/* -- VFS ops: write end --------------------------------------------------- */

static int pipe_open_write(const char *path, int flags)
    { (void)path; (void)flags; return -1; } /* not used via vfs_open */
static int pipe_read_nop(int slot, char *buf, int n, int off)
    { (void)slot; (void)buf; (void)n; (void)off; return 0; }
static int pipe_write_op(int slot, const char *buf, int n, int off)
    { return pipe_write_fn(slot, buf, n, off); }
static void pipe_close_write(int slot) {
    if (slot < 0 || slot >= MAX_PIPES) return;
    pipe_table[slot].write_open--;
    if (pipe_table[slot].read_open <= 0 && pipe_table[slot].write_open <= 0)
        pipe_table[slot].valid = 0;
}

struct vfs_ops pipe_write_ops = {
    pipe_open_write, pipe_read_nop, pipe_write_op, pipe_close_write
};

/* -- pipe_alloc ----------------------------------------------------------- */

int pipe_alloc(int fds[2]) {
    struct file_descriptor *fdt;
    int slot, i, rfd, wfd;

    /* Find a free pipe slot */
    slot = -1;
    for (i = 0; i < MAX_PIPES; i++) {
        if (!pipe_table[i].valid) { slot = i; break; }
    }
    if (slot < 0) return -1;

    fdt = vfs_get_fd_table();
    if (!fdt) return -1;

    /* Find two free fd slots */
    rfd = -1; wfd = -1;
    for (i = 3; i < MAX_FDS; i++) {
        if (!fdt[i].valid) {
            if (rfd < 0)      { rfd = i; }
            else if (wfd < 0) { wfd = i; break; }
        }
    }
    if (rfd < 0 || wfd < 0) return -1;

    pipe_table[slot].head       = 0;
    pipe_table[slot].tail       = 0;
    pipe_table[slot].count      = 0;
    pipe_table[slot].read_open  = 1;
    pipe_table[slot].write_open = 1;
    pipe_table[slot].valid      = 1;

    fdt[rfd].valid  = 1;
    fdt[rfd].inode  = slot;
    fdt[rfd].offset = 0;
    fdt[rfd].ops    = &pipe_read_ops;

    fdt[wfd].valid  = 1;
    fdt[wfd].inode  = slot;
    fdt[wfd].offset = 0;
    fdt[wfd].ops    = &pipe_write_ops;

    fds[0] = rfd;
    fds[1] = wfd;
    return 0;
}
