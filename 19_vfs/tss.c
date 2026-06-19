/*
 * tss.c — Task State Segment implementation
 */

#include <stdint.h>
#include "tss.h"
#include "gdt.h"

/* The single system-wide TSS.  Static = invisible outside this file. */
static struct tss_entry tss;

void tss_init(uint32_t kernel_ss, uint32_t kernel_esp) {
    uint8_t  *p = (uint8_t *)&tss;
    uint32_t  i;

    /* Zero every byte — saves us from naming every unused field. */
    for (i = 0; i < sizeof(tss); i++) p[i] = 0;

    tss.ss0         = kernel_ss;
    tss.esp0        = kernel_esp;
    /*
     * iomap_base set to the size of the TSS means there is no I/O
     * permission bitmap — all I/O port access from ring 3 is forbidden.
     * A real OS would expose selected ports to specific processes here.
     */
    tss.iomap_base  = (uint16_t)sizeof(struct tss_entry);

    /* Ask gdt.c to fill in GDT entry 5 with this TSS's address/limit. */
    gdt_install_tss((uint32_t)&tss, (uint32_t)(sizeof(tss) - 1));

    /* `ltr` loads the Task Register with the TSS selector.
     * GDT_SEL_TSS = 0x28 (GDT index 5, RPL 0). */
    __asm__ volatile ("ltr %0" : : "r"((uint16_t)GDT_SEL_TSS));
}

void tss_set_kernel_stack(uint32_t esp) {
    tss.esp0 = esp;
}
