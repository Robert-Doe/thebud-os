/*
 * gdt.h — Global Descriptor Table interface
 *
 * The GDT is the table the x86 CPU consults on every memory access in
 * Protected Mode to determine whether the access is permitted. This header
 * defines the exact in-memory layout of a GDT entry (as a packed C struct),
 * the access-byte and flags constants you need to build entries, the segment
 * selector values the rest of the kernel uses, and the one public function:
 * gdt_init().
 */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>   /* uint8_t, uint16_t, uint32_t — GCC provides these in -ffreestanding */

/* -------------------------------------------------------------------------
 * GDT entry layout — 8 bytes, exactly as Intel specifies.
 *
 * The field order is scrambled for historical reasons (286 ↔ 386 compat).
 * __attribute__((packed)) prevents the compiler from inserting any padding
 * between fields.  Without it, sizeof(struct gdt_entry) might be 12 instead
 * of 8 and the CPU would read garbage.
 * ------------------------------------------------------------------------- */
struct gdt_entry {
    uint16_t limit_low;    /* Limit  bits  0–15  */
    uint16_t base_low;     /* Base   bits  0–15  */
    uint8_t  base_mid;     /* Base   bits 16–23  */
    uint8_t  access;       /* Access byte        */
    uint8_t  flags_limit;  /* Flags[7:4] + Limit bits 16–19 [3:0] */
    uint8_t  base_high;    /* Base   bits 24–31  */
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * GDT descriptor — the 6-byte structure passed to the lgdt instruction.
 * ------------------------------------------------------------------------- */
struct gdt_descriptor {
    uint16_t size;    /* Size of the GDT in bytes, minus 1 */
    uint32_t offset;  /* Physical address of the GDT       */
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * Access byte bit-field constants (OR these together to build an access byte)
 *
 * Bit 7  Present (P)        — must be 1 for any valid segment
 * Bit 6  DPL high  ─┐      — privilege ring: 00=ring0, 11=ring3
 * Bit 5  DPL low   ─┘
 * Bit 4  Descriptor type    — 1 = code/data segment (vs system segment)
 * Bit 3  Executable (E)    — 1 = code, 0 = data
 * Bit 2  DC                — data: direction (0=grows up); code: conforming
 * Bit 1  RW                — code: readable; data: writable
 * Bit 0  Accessed (A)      — CPU sets this; we initialise to 0
 * ------------------------------------------------------------------------- */
#define GDT_ACCESS_PRESENT    0x80   /* bit 7:  segment is present in memory  */
#define GDT_ACCESS_RING0      0x00   /* bits 6-5: kernel privilege            */
#define GDT_ACCESS_RING3      0x60   /* bits 6-5: user privilege              */
#define GDT_ACCESS_DESCTYPE   0x10   /* bit 4:  code/data descriptor type     */
#define GDT_ACCESS_EXEC       0x08   /* bit 3:  executable (code segment)     */
#define GDT_ACCESS_RW         0x02   /* bit 1:  readable (code) / writable (data) */

/* Pre-built access bytes for the three segments we define */
#define GDT_ACCESS_CODE  (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | \
                          GDT_ACCESS_DESCTYPE | GDT_ACCESS_EXEC  | \
                          GDT_ACCESS_RW)          /* = 0x9A */
#define GDT_ACCESS_DATA  (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | \
                          GDT_ACCESS_DESCTYPE | GDT_ACCESS_RW)    /* = 0x92 */

/* -------------------------------------------------------------------------
 * Flags nibble constants (these go into the HIGH 4 bits of flags_limit)
 *
 * Bit 3  Granularity (G)  — 0: limit in bytes, 1: limit in 4KB pages
 * Bit 2  Size (DB)        — 0: 16-bit, 1: 32-bit segment
 * Bit 1  Long mode (L)    — 1: 64-bit segment (we always use 0)
 * Bit 0  Reserved         — always 0
 *
 * We want: G=1, DB=1, L=0, Reserved=0  →  flags nibble = 0xC
 * Stored in the high nibble of flags_limit: (0xC << 4) = 0xC0
 * The low nibble of flags_limit holds limit bits 16–19.
 * ------------------------------------------------------------------------- */
#define GDT_FLAG_4K_GRAN   0x8   /* granularity = 4KB pages */
#define GDT_FLAG_32BIT     0x4   /* 32-bit protected mode   */

/* Combined flags nibble for all our segments */
#define GDT_FLAGS          (GDT_FLAG_4K_GRAN | GDT_FLAG_32BIT)  /* = 0xC */

/* -------------------------------------------------------------------------
 * Segment selectors — the values loaded into CS, DS, SS, etc.
 * Selector = (GDT index) * 8 | privilege_level
 * We run everything at ring 0 so the bottom 3 bits are all zero.
 * ------------------------------------------------------------------------- */
#define GDT_SEL_NULL   0x00   /* must never actually be used */
#define GDT_SEL_CODE   0x08   /* GDT[1] × 8 = 0x08           */
#define GDT_SEL_DATA   0x10   /* GDT[2] × 8 = 0x10           */

/* Number of entries in our GDT */
#define GDT_NUM_ENTRIES  3

/*
 * gdt_init — Build the GDT and load it into the CPU.
 *
 * Sets up three descriptors (null, code, data) as a flat 0–4GB model,
 * calls lgdt to load the GDTR, then reloads every segment register
 * so the CPU starts using the new descriptors immediately.
 *
 * Call this once, early in kernel_main(), before anything that might
 * fault or cause a segment violation.
 */
void gdt_init(void);

#endif /* GDT_H */
