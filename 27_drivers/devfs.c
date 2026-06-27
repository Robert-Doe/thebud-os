/*
 * devfs.c — /dev pseudo-filesystem implementation
 *
 * devfs is the thinnest possible VFS backend: no on-disk state, no inodes,
 * no dentries.  open() is a driver table lookup; read/write/ioctl are
 * direct vtable dispatches.
 */

#include "devfs.h"
#include "driver.h"
#include "vfs.h"
#include "vga.h"

/* fd_table private data: we repurpose the existing inode field to
   store a pointer to the matched driver_ops */
#define FD_DRIVER(fd) ((struct driver_ops *)(uintptr_t)(fd)->inode)

void devfs_init(void) {
    vga_print("[devfs] /dev mounted\n");
}

int devfs_open(const char *name, struct file_descriptor *fd) {
    /* Strip leading /dev/ if present */
    const char *dev_name = name;
    if (dev_name[0]=='/' && dev_name[1]=='d' && dev_name[2]=='e' &&
        dev_name[3]=='v' && dev_name[4]=='/')
        dev_name += 5;

    struct driver_ops *ops = driver_find(dev_name);
    if (!ops) return -1;

    fd->type  = FD_DEVFS;
    fd->inode = (uint32_t)(uintptr_t)ops; /* store ops pointer in inode */
    fd->pos   = 0;
    fd->valid = 1;
    return 0;
}

int devfs_read(struct file_descriptor *fd, uint8_t *buf, uint32_t len) {
    struct driver_ops *ops = FD_DRIVER(fd);
    if (!ops || !ops->read) return -1;
    return ops->read(buf, len);
}

int devfs_write(struct file_descriptor *fd, const uint8_t *buf, uint32_t len) {
    struct driver_ops *ops = FD_DRIVER(fd);
    if (!ops || !ops->write) return -1;
    return ops->write(buf, len);
}

int devfs_ioctl(struct file_descriptor *fd, uint32_t cmd, void *arg) {
    struct driver_ops *ops = FD_DRIVER(fd);
    if (!ops || !ops->ioctl) return -1;
    return ops->ioctl(cmd, arg);
}
