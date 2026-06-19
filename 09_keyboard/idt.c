/*
 * idt.c — Interrupt Descriptor Table implementation
 */

#include "idt.h"
#include "isr.h"
#include "gdt.h"

static struct idt_entry     idt[IDT_NUM_ENTRIES];
static struct idt_descriptor idtr;

extern void isr0(void);   extern void isr1(void);   extern void isr2(void);
extern void isr3(void);   extern void isr4(void);   extern void isr5(void);
extern void isr6(void);   extern void isr7(void);   extern void isr8(void);
extern void isr9(void);   extern void isr10(void);  extern void isr11(void);
extern void isr12(void);  extern void isr13(void);  extern void isr14(void);
extern void isr15(void);  extern void isr16(void);  extern void isr17(void);
extern void isr18(void);  extern void isr19(void);  extern void isr20(void);
extern void isr21(void);  extern void isr22(void);  extern void isr23(void);
extern void isr24(void);  extern void isr25(void);  extern void isr26(void);
extern void isr27(void);  extern void isr28(void);  extern void isr29(void);
extern void isr30(void);  extern void isr31(void);

extern void irq0(void);   extern void irq1(void);   extern void irq2(void);
extern void irq3(void);   extern void irq4(void);   extern void irq5(void);
extern void irq6(void);   extern void irq7(void);   extern void irq8(void);
extern void irq9(void);   extern void irq10(void);  extern void irq11(void);
extern void irq12(void);  extern void irq13(void);  extern void irq14(void);
extern void irq15(void);

void idt_set_gate(uint8_t n, uint32_t handler, uint16_t selector,
                  uint8_t type_attr) {
    idt[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[n].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[n].selector    = selector;
    idt[n].zero        = 0;
    idt[n].type_attr   = type_attr;
}

static void idt_load(void) {
    idtr.size   = (uint16_t)(sizeof(idt) - 1);
    idtr.offset = (uint32_t)idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

void idt_init(void) {
    idt_set_gate( 0, (uint32_t)isr0,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate( 1, (uint32_t)isr1,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate( 2, (uint32_t)isr2,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate( 3, (uint32_t)isr3,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate( 4, (uint32_t)isr4,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate( 5, (uint32_t)isr5,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate( 6, (uint32_t)isr6,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate( 7, (uint32_t)isr7,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate( 8, (uint32_t)isr8,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate( 9, (uint32_t)isr9,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(10, (uint32_t)isr10, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(11, (uint32_t)isr11, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(12, (uint32_t)isr12, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(13, (uint32_t)isr13, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(14, (uint32_t)isr14, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(15, (uint32_t)isr15, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(16, (uint32_t)isr16, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(17, (uint32_t)isr17, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(18, (uint32_t)isr18, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(19, (uint32_t)isr19, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(20, (uint32_t)isr20, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(21, (uint32_t)isr21, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(22, (uint32_t)isr22, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(23, (uint32_t)isr23, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(24, (uint32_t)isr24, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(25, (uint32_t)isr25, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(26, (uint32_t)isr26, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(27, (uint32_t)isr27, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(28, (uint32_t)isr28, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(29, (uint32_t)isr29, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(30, (uint32_t)isr30, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(31, (uint32_t)isr31, GDT_SEL_CODE, IDT_GATE_INTERRUPT);

    idt_set_gate(32, (uint32_t)irq0,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(33, (uint32_t)irq1,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(34, (uint32_t)irq2,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(35, (uint32_t)irq3,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(36, (uint32_t)irq4,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(37, (uint32_t)irq5,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(38, (uint32_t)irq6,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(39, (uint32_t)irq7,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(40, (uint32_t)irq8,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(41, (uint32_t)irq9,  GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(42, (uint32_t)irq10, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(43, (uint32_t)irq11, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(44, (uint32_t)irq12, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(45, (uint32_t)irq13, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(46, (uint32_t)irq14, GDT_SEL_CODE, IDT_GATE_INTERRUPT);
    idt_set_gate(47, (uint32_t)irq15, GDT_SEL_CODE, IDT_GATE_INTERRUPT);

    idt_load();
}
