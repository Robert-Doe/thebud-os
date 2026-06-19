/*
 * pic.h — 8259A Programmable Interrupt Controller interface
 *
 * The 8259A PIC sits between hardware devices and the CPU's INT pin.
 * IRQ lines from the keyboard, timer, serial ports, etc. feed into the PIC;
 * the PIC serialises them and asserts the CPU interrupt line.
 *
 * There are two cascaded PICs: master (IRQ0-7) and slave (IRQ8-15).
 * We must initialise and remap them before enabling CPU interrupts.
 */

#ifndef PIC_H
#define PIC_H

/* I/O port addresses */
#define PIC1_CMD    0x20    /* master PIC command port */
#define PIC1_DATA   0x21    /* master PIC data port    */
#define PIC2_CMD    0xA0    /* slave  PIC command port */
#define PIC2_DATA   0xA1    /* slave  PIC data port    */

/* End-of-Interrupt command — must be sent after every IRQ handler */
#define PIC_EOI     0x20

/*
 * After remapping, IRQ lines map to these CPU interrupt vectors:
 *   IRQ0-7  → vectors PIC1_OFFSET   .. PIC1_OFFSET+7   (32-39)
 *   IRQ8-15 → vectors PIC2_OFFSET   .. PIC2_OFFSET+7   (40-47)
 *
 * Default BIOS mapping put IRQ0-7 on vectors 8-15, which collide with
 * CPU exception vectors.  We move them to 32+ where they are safe.
 */
#define PIC1_OFFSET  32
#define PIC2_OFFSET  40

/*
 * pic_init — remap both PICs to the offsets above and unmask all IRQ lines.
 * Must be called before idt_init() installs interrupt gates, and before
 * 'sti' enables CPU interrupts.
 */
void pic_init(void);

/*
 * pic_send_eoi — signal End-of-Interrupt for IRQ line 'irq' (0-15).
 * Must be called at the END of every hardware IRQ handler, or the PIC
 * will never send another interrupt on that line.
 */
void pic_send_eoi(uint8_t irq);

/*
 * pic_set_mask / pic_clear_mask — disable or enable a single IRQ line.
 * Masked IRQs are ignored by the PIC even if they fire.
 */
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

#endif /* PIC_H */
