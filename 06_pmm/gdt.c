/*
 * gdt.c — Global Descriptor Table implementation
 *
 * The bootloader defined a GDT as raw hand-crafted assembly bytes just to
 * satisfy the CPU's requirement before entering Protected Mode. That was the
 * minimum needed to get through the door. This module replaces it with a
 * proper C implementation that lives inside the kernel:
 *
 *   - The GDT is a typed C array of struct gdt_entry (8 bytes each, packed).
 *   - gdt_set_entry() builds one descriptor from human-readable parameters.
 *   - gdt_load() uses inline assembly to call lgdt and then reload every
 *     segment register, including CS (which requires a far jump — you cannot
 *     mov directly into CS).
 *   - gdt_init() ties it all together and is the only function called from
 *     kernel_main().
 *
 * After gdt_init() returns, the CPU is using the GDT defined here, not the
 * one from the bootloader.
 */

#include "gdt.h"

/* -------------------------------------------------------------------------
 * The GDT itself and its descriptor — both must be in memory at runtime.
 * 'static' keeps them invisible outside this file.
 * ------------------------------------------------------------------------- */
static struct gdt_entry     gdt[GDT_NUM_ENTRIES];
static struct gdt_descriptor gdtr;

/* -------------------------------------------------------------------------
 * gdt_set_entry — fill one GDT entry from plain parameters.
 * ------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------
 * gdt_load — install the GDT into the CPU and refresh all segment registers.
 * ------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------
 * gdt_init — the only public function in this module.
 * ------------------------------------------------------------------------- */
void gdt_init(void)
{
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0x00000000, 0x000FFFFF, GDT_ACCESS_CODE, GDT_FLAGS);
    gdt_set_entry(2, 0x00000000, 0x000FFFFF, GDT_ACCESS_DATA, GDT_FLAGS);
    gdt_load();
}
