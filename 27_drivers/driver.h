/*
 * driver.h — Unified device driver registration table
 *
 * Every driver fills in a driver_ops struct and calls driver_register().
 * The kernel calls probe() at boot for each registered driver.
 * Matched drivers expose a /dev/<name> node through devfs.
 */

#ifndef DRIVER_H
#define DRIVER_H

#include <stdint.h>

#define MAX_DRIVERS  16
#define DRIVER_NAME_LEN 16

/* Device types — determines which /dev node class is created */
typedef enum {
    DEV_CHAR  = 1,   /* character device: keyboard, serial, VGA  */
    DEV_BLOCK = 2,   /* block device: disk, ramdisk              */
    DEV_NET   = 3,   /* network device: NE2000, loopback         */
} dev_type_t;

/*
 * driver_ops — the vtable every driver must implement.
 *
 * probe()  — detect and initialise hardware.  Return 0 on success, -1 if
 *             not present.  Called once at boot.
 * read()   — read `len` bytes into `buf`.  Returns bytes read or <0 on error.
 * write()  — write `len` bytes from `buf`.  Returns bytes written or <0.
 * ioctl()  — device-specific control command.
 */
struct driver_ops {
    const char *name;            /* e.g. "kbd", "disk", "eth0" */
    dev_type_t  type;
    int  (*probe)(void);
    int  (*read) (uint8_t *buf, uint32_t len);
    int  (*write)(const uint8_t *buf, uint32_t len);
    int  (*ioctl)(uint32_t cmd, void *arg);
};

/* Global driver table */
struct driver_entry {
    struct driver_ops *ops;
    int               active;    /* 1 if probe() succeeded */
    uint32_t          dev_major; /* major number for /dev node */
};

extern struct driver_entry driver_table[MAX_DRIVERS];
extern int driver_count;

void driver_init(void);
int  driver_register(struct driver_ops *ops);
void driver_probe_all(void);

/* Look up a driver by /dev name — used by devfs open() */
struct driver_ops *driver_find(const char *name);

#endif /* DRIVER_H */
