/*
 * idt.h — Interrupt Descriptor Table interface
 *
 * The IDT is the CPU's lookup table for interrupt and exception handlers.
 * It holds up to 256 entries — one per interrupt vector.  Each 8-byte
 * entry (a "gate descriptor") stores the address of the handler function,
 * the code segment it runs in, and the type/privilege of the gate.
 *
 * This mirrors the GDT pattern almost exactly:
 *   GDT: lgdt loads base+size → CPU uses it for every memory access
 *   IDT: lidt loads base+size → CPU uses it for every interrupt/exception
 */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * IDT gate descriptor — 8 bytes, packed (same discipline as GDT entries).
 *
 * The handler address (offset) is split across two 16-bit fields, with
 * a reserved zero byte in the middle — another Intel legacy layout.
 * ------------------------------------------------------------------------- */
struct idt_entry {
    uint16_t offset_low;   /* handler address bits  0-15 */
    uint16_t selector;     /* code segment selector      */
    uint8_t  zero;         /* reserved — always 0        */
    uint8_t  type_attr;    /* gate type + attributes     */
    uint16_t offset_high;  /* handler address bits 16-31 */
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * IDT descriptor — 6-byte structure given to 'lidt'.
 * Same layout as the GDT descriptor passed to 'lgdt'.
 * ------------------------------------------------------------------------- */
struct idt_descriptor {
    uint16_t size;    /* size of IDT in bytes, minus 1 */
    uint32_t offset;  /* physical address of IDT        */
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * Gate type_attr byte:
 *
 * Bit 7   Present (P)   = 1 for any valid gate
 * Bits 6-5 DPL          = 00 for kernel gates (ring 0)
 * Bit 4   Storage (S)   = 0 for interrupt/trap gates (must be 0)
 * Bits 3-0 Gate type:
 *   0xE = 32-bit Interrupt Gate — CPU clears IF on entry (no nested IRQs)
 *   0xF = 32-bit Trap Gate     — CPU leaves IF unchanged (nested IRQs ok)
 *
 * We use interrupt gates for all handlers (0x8E = P=1, DPL=0, type=0xE).
 * ------------------------------------------------------------------------- */
#define IDT_GATE_INTERRUPT  0x8E   /* present, ring 0, 32-bit interrupt gate */
#define IDT_GATE_TRAP       0x8F   /* present, ring 0, 32-bit trap gate      */

/* Total number of IDT entries (one per possible interrupt vector) */
#define IDT_NUM_ENTRIES     256

/*
 * idt_set_gate — fill one IDT entry.
 *
 * n        : vector number (0-255)
 * handler  : 32-bit address of the assembly stub for this vector
 * selector : code segment to run handler in (GDT_SEL_CODE = 0x08)
 * type_attr: gate attributes (use IDT_GATE_INTERRUPT or IDT_GATE_TRAP)
 */
void idt_set_gate(uint8_t n, uint32_t handler, uint16_t selector,
                  uint8_t type_attr);

/*
 * idt_init — install all ISR/IRQ gates and load the IDT into the CPU.
 * Call after gdt_init() and pic_init(), before 'sti'.
 */
void idt_init(void);

#endif /* IDT_H */
