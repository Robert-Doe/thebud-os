/*
 * gdt.c — Global Descriptor Table implementation
 *
 * Module 14 changes: GDT expanded from 3 to 6 entries.
 * Entries 3 and 4 are user-mode code and data (DPL=3, flat 4GB).
 * Entry 5 is the TSS descriptor, installed via gdt_install_tss().
 */

#include "gdt.h"

static struct gdt_entry     gdt[GDT_NUM_ENTRIES];
static struct gdt_descriptor gdtr;

static void gdt_set_entry(int i,
                           uint32_t base,
                           uint32_t limit,
                           uint8_t  access,
                           uint8_t  flags)
{
    gdt[i].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[i].base_low    = (uint16_t)(base  & 0xFFFF);
    gdt[i].base_mid    = (uint8_t)((base  >> 16) & 0xFF);
    gdt[i].base_high   = (uint8_t)((base  >> 24) & 0xFF);
    gdt[i].access      = access;
    gdt[i].flags_limit = (uint8_t)(((flags & 0x0F) << 4) |
                                    ((limit >> 16) & 0x0F));
}

static void gdt_load(void)
{
    gdtr.size   = (uint16_t)(sizeof(gdt) - 1);
    gdtr.offset = (uint32_t)gdt;

    __asm__ volatile ("lgdt %0" : : "m"(gdtr));

    __asm__ volatile (
        "pushl %0       \n"
        "pushl $1f      \n"
        "lret           \n"
        "1:             \n"
        :
        : "i"(GDT_SEL_CODE)
    );

    __asm__ volatile (
        "mov  %0, %%ax  \n"
        "mov %%ax, %%ds \n"
        "mov %%ax, %%es \n"
        "mov %%ax, %%fs \n"
        "mov %%ax, %%gs \n"
        "mov %%ax, %%ss \n"
        :
        : "i"(GDT_SEL_DATA)
        : "ax"
    );
}

void gdt_init(void)
{
    /* Entry 0: null — required by Intel spec */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Entry 1: kernel code (ring 0, flat 4GB) */
    gdt_set_entry(1, 0x00000000, 0x000FFFFF, GDT_ACCESS_CODE, GDT_FLAGS);

    /* Entry 2: kernel data (ring 0, flat 4GB) */
    gdt_set_entry(2, 0x00000000, 0x000FFFFF, GDT_ACCESS_DATA, GDT_FLAGS);

    /* Entry 3: user code (ring 3, flat 4GB) */
    gdt_set_entry(3, 0x00000000, 0x000FFFFF, GDT_ACCESS_USER_CODE, GDT_FLAGS);

    /* Entry 4: user data (ring 3, flat 4GB) */
    gdt_set_entry(4, 0x00000000, 0x000FFFFF, GDT_ACCESS_USER_DATA, GDT_FLAGS);

    /* Entry 5: TSS — left blank; gdt_install_tss() fills it after tss_init() */
    gdt_set_entry(5, 0, 0, 0, 0);

    gdt_load();
}

void gdt_install_tss(uint32_t base, uint32_t limit)
{
    /*
     * The TSS descriptor is a SYSTEM descriptor (S-bit = 0), so flags = 0
     * (no granularity, not 32-bit code/data).  The limit is in bytes.
     * Access byte 0x89 = Present | DPL=0 | Type=9 (32-bit TSS available).
     */
    gdt_set_entry(5, base, limit, GDT_ACCESS_TSS, 0);
}
