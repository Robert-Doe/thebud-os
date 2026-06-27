/*
 * gdt.h — Global Descriptor Table interface
 *
 * Module 14 additions:
 *   - GDT entries 3 and 4: user-mode code and data segments (DPL = 3).
 *   - GDT entry 5: TSS descriptor (installed by tss.c via gdt_install_tss()).
 *   - New selectors: GDT_SEL_USER_CODE (0x1B), GDT_SEL_USER_DATA (0x23),
 *     GDT_SEL_TSS (0x28).
 *   - New public function: gdt_install_tss(base, limit).
 */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_descriptor {
    uint16_t size;
    uint32_t offset;
} __attribute__((packed));

/* ── Access byte bit-field constants ────────────────────────────────── */
#define GDT_ACCESS_PRESENT    0x80
#define GDT_ACCESS_RING0      0x00
#define GDT_ACCESS_RING3      0x60
#define GDT_ACCESS_DESCTYPE   0x10   /* code/data descriptor (S=1) */
#define GDT_ACCESS_EXEC       0x08
#define GDT_ACCESS_RW         0x02

/* Pre-built access bytes */
#define GDT_ACCESS_CODE  (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | \
                          GDT_ACCESS_DESCTYPE | GDT_ACCESS_EXEC  | \
                          GDT_ACCESS_RW)                             /* 0x9A */
#define GDT_ACCESS_DATA  (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | \
                          GDT_ACCESS_DESCTYPE | GDT_ACCESS_RW)       /* 0x92 */

/* User-mode versions — same as above but DPL=3 */
#define GDT_ACCESS_USER_CODE  (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | \
                               GDT_ACCESS_DESCTYPE | GDT_ACCESS_EXEC | \
                               GDT_ACCESS_RW)                        /* 0xFA */
#define GDT_ACCESS_USER_DATA  (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | \
                               GDT_ACCESS_DESCTYPE | GDT_ACCESS_RW)  /* 0xF2 */

/*
 * TSS descriptor access byte: Present=1, DPL=0, S=0 (system), Type=9
 * (32-bit TSS available).  0x89 = 1000 1001b.
 */
#define GDT_ACCESS_TSS  0x89

/* ── Flags nibble ────────────────────────────────────────────────────── */
#define GDT_FLAG_4K_GRAN   0x8
#define GDT_FLAG_32BIT     0x4
#define GDT_FLAGS          (GDT_FLAG_4K_GRAN | GDT_FLAG_32BIT)  /* 0xC */

/* ── Segment selectors ───────────────────────────────────────────────── */
/* Format: (GDT_index << 3) | RPL
 * Kernel segments: RPL=0.  User segments: RPL=3. */
#define GDT_SEL_NULL       0x00u  /* index 0, RPL 0 */
#define GDT_SEL_CODE       0x08u  /* index 1, RPL 0 — kernel code  */
#define GDT_SEL_DATA       0x10u  /* index 2, RPL 0 — kernel data  */
#define GDT_SEL_USER_CODE  0x1Bu  /* index 3, RPL 3 — user code    */
#define GDT_SEL_USER_DATA  0x23u  /* index 4, RPL 3 — user data    */
#define GDT_SEL_TSS        0x28u  /* index 5, RPL 0 — TSS          */

/* Number of GDT entries: null, kcode, kdata, ucode, udata, tss */
#define GDT_NUM_ENTRIES  6

/*
 * gdt_init() — build and load the GDT.
 * Installs entries 0-4 (null, kernel code/data, user code/data).
 * Entry 5 (TSS) is blank here; gdt_install_tss() fills it later.
 */
void gdt_init(void);

/*
 * gdt_install_tss(base, limit) — fill GDT entry 5 with the TSS descriptor.
 * Called by tss_init() after the TSS struct is ready in memory.
 */
void gdt_install_tss(uint32_t base, uint32_t limit);

#endif /* GDT_H */
