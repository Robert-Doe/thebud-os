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
 *
 * i      : index into gdt[] (0 = null, 1 = code, 2 = data)
 * base   : 32-bit start address of the segment
 * limit  : 20-bit segment limit (with G=1 this is in 4KB units → up to 4GB)
 * access : access byte (use GDT_ACCESS_* constants from gdt.h)
 * flags  : flags nibble (upper 4 bits only; use GDT_FLAG_* constants)
 *
 * The GDT entry layout is scrambled across 8 bytes — the base address is
 * split into three non-contiguous pieces, and so is the limit.  This
 * function hides that ugliness from the caller.
 * ------------------------------------------------------------------------- */
static void gdt_set_entry(int i,
                           uint32_t base,
                           uint32_t limit,
                           uint8_t  access,
                           uint8_t  flags)
{
    /* Limit: lower 16 bits go in limit_low; upper 4 bits go in flags_limit */
    gdt[i].limit_low   = (uint16_t)(limit & 0xFFFF);

    /* Base: split into three pieces across the descriptor */
    gdt[i].base_low    = (uint16_t)(base  & 0xFFFF);
    gdt[i].base_mid    = (uint8_t)((base  >> 16) & 0xFF);
    gdt[i].base_high   = (uint8_t)((base  >> 24) & 0xFF);

    gdt[i].access      = access;

    /*
     * flags_limit: pack the 4-bit flags into the HIGH nibble and the top 4
     * bits of the limit into the LOW nibble.
     *
     * Example for our flat segments:
     *   limit = 0xFFFFF  →  limit bits 16-19 = 0xF
     *   flags = 0xC      →  (0xC << 4) = 0xC0
     *   flags_limit      = 0xC0 | 0x0F = 0xCF
     */
    gdt[i].flags_limit = (uint8_t)(((flags & 0x0F) << 4) |
                                    ((limit >> 16) & 0x0F));
}

/* -------------------------------------------------------------------------
 * gdt_load — install the GDT into the CPU and refresh all segment registers.
 *
 * This function is pure inline assembly because:
 *   1. lgdt is not a C function — it is an x86 instruction.
 *   2. Reloading CS (the code segment register) cannot be done with a plain
 *      "mov" — the CPU only allows CS to be changed via a far call, far jump,
 *      or far return.  We use a far return trick here.
 *   3. The other segment registers (DS, ES, FS, GS, SS) can be set with
 *      normal mov instructions but must be done after CS is updated.
 * ------------------------------------------------------------------------- */
static void gdt_load(void)
{
    /* Tell the CPU where the GDT lives */
    gdtr.size   = (uint16_t)(sizeof(gdt) - 1);
    gdtr.offset = (uint32_t)gdt;

    /*
     * Step 1 — lgdt: load the GDT register.
     *
     * "lgdt %0"           : the instruction, with %0 replaced by the address
     * : : "m"(gdtr)       : input operand — 'm' means 'memory operand'; the
     *                        compiler gives lgdt the address of gdtr in RAM.
     * volatile            : do not reorder or remove this instruction.
     */
    __asm__ volatile ("lgdt %0" : : "m"(gdtr));

    /*
     * Step 2 — reload CS via a far return.
     *
     * You cannot write "mov cs, ax" — the CPU refuses it.  Instead we fake a
     * far return: push the new CS selector (0x08) and a return address onto
     * the stack, then execute 'lret' (far return).  The CPU pops both values
     * and simultaneously updates CS and EIP, which flushes the pipeline and
     * puts us in the new code segment.
     *
     * "pushl %0"          : push the code segment selector (0x08) as a dword
     * "pushl $1f"         : push the address of label '1:' (our landing spot)
     * "lret"              : far return — pops EIP then CS from the stack
     * "1:"                : we continue executing here in the new CS
     *
     * The 'l' suffix on pushl/lret means 32-bit (long) operand size.
     */
    __asm__ volatile (
        "pushl %0       \n"   /* push new CS = GDT_SEL_CODE (0x08) */
        "pushl $1f      \n"   /* push return address */
        "lret           \n"   /* far return: pops EIP, then CS */
        "1:             \n"   /* execution resumes here */
        :
        : "i"(GDT_SEL_CODE)
    );

    /*
     * Step 3 — reload every data segment register with the data selector.
     *
     * After lgdt and the far return, DS/ES/FS/GS/SS still hold the values
     * the bootloader left in them.  They pointed to valid descriptors in the
     * old bootloader GDT, but that GDT no longer exists as the authoritative
     * one.  We reload them all to be explicit and safe.
     *
     * AX holds 0x10 (GDT_SEL_DATA).  We move the same value into all five
     * registers; we cannot mov an immediate directly into a segment register
     * so we go through AX.
     */
    __asm__ volatile (
        "mov  %0, %%ax  \n"
        "mov %%ax, %%ds \n"
        "mov %%ax, %%es \n"
        "mov %%ax, %%fs \n"
        "mov %%ax, %%gs \n"
        "mov %%ax, %%ss \n"
        :
        : "i"(GDT_SEL_DATA)
        : "ax"             /* tell GCC we clobber AX */
    );
}

/* -------------------------------------------------------------------------
 * gdt_init — the only public function in this module.
 *
 * Builds the three GDT entries and loads the table into the CPU.
 * Call this once, early in kernel_main().
 * ------------------------------------------------------------------------- */
void gdt_init(void)
{
    /*
     * Entry 0: Null descriptor.
     * Required by the CPU spec.  All fields zero.  If a segment register is
     * ever accidentally loaded with selector 0x00, the CPU fires a General
     * Protection Fault immediately instead of using a garbage descriptor.
     */
    gdt_set_entry(0, 0, 0, 0, 0);

    /*
     * Entry 1: Kernel code segment.
     * Base  = 0x00000000 (starts at the very bottom of address space)
     * Limit = 0xFFFFF    (with G=1 granularity → 0xFFFFF × 4096 = 4GB)
     * Access= 0x9A       (present, ring 0, code/data type, executable, readable)
     * Flags = 0xC        (4KB granularity, 32-bit)
     *
     * This segment covers all 4GB of RAM.  Code the CPU fetches runs here.
     */
    gdt_set_entry(1,
                  0x00000000,
                  0x000FFFFF,
                  GDT_ACCESS_CODE,
                  GDT_FLAGS);

    /*
     * Entry 2: Kernel data segment.
     * Same base and limit as the code segment.
     * Access= 0x92 (present, ring 0, code/data type, NOT executable, writable)
     *
     * Data reads and writes, stack operations — all use this segment.
     */
    gdt_set_entry(2,
                  0x00000000,
                  0x000FFFFF,
                  GDT_ACCESS_DATA,
                  GDT_FLAGS);

    /* Install the GDT and reload all segment registers */
    gdt_load();
}
