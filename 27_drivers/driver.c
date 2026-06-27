/*
 * driver.c — Device driver registration and probe
 *
 * Drivers call driver_register() before kernel_main() runs probe_all().
 * Registered drivers that probe successfully become accessible via devfs.
 */

#include "driver.h"
#include "vga.h"

struct driver_entry driver_table[MAX_DRIVERS];
int driver_count = 0;

static uint32_t next_major = 1;

void driver_init(void) {
    for (int i = 0; i < MAX_DRIVERS; i++) {
        driver_table[i].ops    = 0;
        driver_table[i].active = 0;
    }
    driver_count = 0;
}

int driver_register(struct driver_ops *ops) {
    if (driver_count >= MAX_DRIVERS) return -1;
    driver_table[driver_count].ops       = ops;
    driver_table[driver_count].active    = 0;
    driver_table[driver_count].dev_major = next_major++;
    driver_count++;
    return 0;
}

void driver_probe_all(void) {
    for (int i = 0; i < driver_count; i++) {
        struct driver_ops *ops = driver_table[i].ops;
        if (!ops) continue;
        vga_print("[driver] probing: ");
        vga_print(ops->name);
        if (ops->probe && ops->probe() == 0) {
            driver_table[i].active = 1;
            vga_print(" ... OK\n");
        } else {
            vga_print(" ... not found\n");
        }
    }
}

struct driver_ops *driver_find(const char *name) {
    for (int i = 0; i < driver_count; i++) {
        if (!driver_table[i].active) continue;
        const char *n = driver_table[i].ops->name;
        int j = 0;
        while (n[j] && name[j] && n[j] == name[j]) j++;
        if (!n[j] && !name[j]) return driver_table[i].ops;
    }
    return 0;
}
