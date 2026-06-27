/*
 * devfs.h — /dev pseudo-filesystem
 *
 * devfs maps device names to driver_ops vtables.  When a process opens
 * "/dev/kbd" the VFS calls devfs_open() which looks up the driver by name
 * and stores the ops pointer in the fd_table entry.  Subsequent read/write
 * calls go directly to the driver's vtable.
 *
 * devfs is a VFS backend (registered as fs_type "devfs").  It has no
 * on-disk format — the device list is built at boot from driver_table[].
 */

#ifndef DEVFS_H
#define DEVFS_H

#include <stdint.h>
#include "vfs.h"

/* VFS backend hooks registered by devfs_init() */
void devfs_init(void);

/* Called by VFS open() when path starts with "/dev/" */
int devfs_open(const char *name, struct file_descriptor *fd);

/* Passthrough read/write to the underlying driver */
int devfs_read (struct file_descriptor *fd, uint8_t *buf, uint32_t len);
int devfs_write(struct file_descriptor *fd, const uint8_t *buf, uint32_t len);
int devfs_ioctl(struct file_descriptor *fd, uint32_t cmd, void *arg);

#endif /* DEVFS_H */
