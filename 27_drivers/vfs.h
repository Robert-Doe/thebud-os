/*
 * vfs.h -- Virtual File System layer for BobOS
 *
 * Module 19 additions:
 *   Provides a unified open/read/write/close interface that dispatches to
 *   backend "vfs_ops" vtables.  Two backends are provided: vga_ops (stdout)
 *   and bobfs_ops (BobFS files).  Each process has its own fd table stored
 *   in its PCB, with fd 0/1/2 pre-wired to vga_ops on creation.
 */

#ifndef VFS_H
#define VFS_H
#include <stdint.h>

#define MAX_FDS    16
#define MAX_MOUNTS  4

/* VFS operations -- one vtable per filesystem or device type */
struct vfs_ops {
    int  (*open) (const char *path, int flags);
    int  (*read) (int inode, char *buf, int n, int offset);
    int  (*write)(int inode, const char *buf, int n, int offset);
    void (*close)(int inode);
};

/* fd type — distinguishes VFS files from devfs devices */
typedef enum { FD_FS = 0, FD_DEVFS = 1, FD_PIPE = 2 } fd_type_t;

/* Open file descriptor in the process fd table */
struct file_descriptor {
    int             valid;
    uint32_t        inode;    /* FS-side handle OR devfs driver_ops ptr */
    int             offset;   /* byte offset for sequential reads       */
    struct vfs_ops *ops;
    fd_type_t       type;     /* FD_FS / FD_DEVFS / FD_PIPE            */
    int             pos;      /* alias for offset — used by devfs       */
};

/* Mount point -- path prefix + ops pointer */
struct vfs_mount {
    char            prefix[32];
    struct vfs_ops *ops;
    int             valid;
};

void vfs_init(void);
int  vfs_mount(const char *prefix, struct vfs_ops *ops);
int  vfs_open(const char *path, int flags);      /* returns fd */
int  vfs_read(int fd, char *buf, int n);
int  vfs_write(int fd, const char *buf, int n);
void vfs_close(int fd);

/* Wired to the current process's fd table by process.c */
struct file_descriptor *vfs_get_fd_table(void);  /* returns current->fd_table */

/* Built-in VFS backends */
extern struct vfs_ops vga_ops;     /* stdout/stderr: vga_putchar */
extern struct vfs_ops bobfs_ops;   /* BobFS backend              */

#endif /* VFS_H */
