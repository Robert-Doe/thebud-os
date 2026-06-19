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
 * ------------------------------------------------------------------------- */
struct idt_descriptor {
    uint16_t size;    /* size of IDT in bytes, minus 1 */
    uint32_t offset;  /* physical address of IDT        */
} __attribute__((packed));

/* -------------------------------------------------------------------------
 * Gate type_attr byte:
 * 0x8E = present, ring 0, 32-bit interrupt gate (IF cleared on entry)
 * 0x8F = present, ring 0, 32-bit trap gate      (IF unchanged)
 * ------------------------------------------------------------------------- */
#define IDT_GATE_INTERRUPT  0x8E
#define IDT_GATE_TRAP       0x8F

#define IDT_NUM_ENTRIES     256

void idt_set_gate(uint8_t n, uint32_t handler, uint16_t selector,
                  uint8_t type_attr);
void idt_init(void);

#endif /* IDT_H */
