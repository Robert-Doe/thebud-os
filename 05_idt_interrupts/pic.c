/*
 * pic.c — 8259A PIC initialisation and control
 *
 * The 8259A is programmed with a sequence of Initialisation Command Words
 * (ICW1-ICW4) written to its command and data ports.  Once initialised,
 * it passes incoming IRQ signals to the CPU as interrupt vectors we specify.
 *
 * Before we touch the PIC, CPU interrupts must be OFF (they are off by
 * default after boot, and our bootloader never calls 'sti').
 */

#include <stdint.h>
#include "pic.h"
#include "io.h"

/* ICW1 flags */
#define ICW1_ICW4       0x01   /* will send ICW4                 */
#define ICW1_INIT       0x10   /* initialisation bit — must be 1 */

/* ICW4 flags */
#define ICW4_8086       0x01   /* 8086/8088 mode (not MCS-80/85) */

void pic_init(void) {
    /*
     * Save current interrupt masks so we can restore them after remapping.
     * (On a real system you'd restore them; for us they start as 0xFF anyway.)
     */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /*
     * ICW1 — start initialisation sequence.
     * Writing to the command port with bit 4 set begins a 4-word init.
     */
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4);   io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4);   io_wait();

    /*
     * ICW2 — set the vector offset for each PIC.
     * Master IRQ0 → CPU vector 32, Slave IRQ8 → CPU vector 40.
     */
    outb(PIC1_DATA, PIC1_OFFSET);   io_wait();   /* master: vectors 32-39 */
    outb(PIC2_DATA, PIC2_OFFSET);   io_wait();   /* slave:  vectors 40-47 */

    /*
     * ICW3 — configure cascade.
     * Master: bit 2 set = slave is connected on IRQ2.
     * Slave:  identity = 2 (its cascade line number).
     */
    outb(PIC1_DATA, 0x04);   io_wait();   /* master: slave on IRQ2 */
    outb(PIC2_DATA, 0x02);   io_wait();   /* slave: cascade id = 2 */

    /*
     * ICW4 — set operating mode.
     * 0x01 = 8086 mode.  Without this, the PIC stays in MCS-80 mode
     * and doesn't send EOI acknowledgements correctly.
     */
    outb(PIC1_DATA, ICW4_8086);   io_wait();
    outb(PIC2_DATA, ICW4_8086);   io_wait();

    /*
     * Restore saved masks (all IRQs enabled = 0x00).
     * We start with everything unmasked; individual drivers can mask
     * IRQ lines they don't own yet via pic_set_mask().
     */
    outb(PIC1_DATA, 0x00);
    outb(PIC2_DATA, 0x00);

    (void)mask1; (void)mask2;   /* suppress unused-variable warnings */
}

void pic_send_eoi(uint8_t irq) {
    /*
     * IRQ8-15 come through the slave PIC, so we must acknowledge both:
     * first the slave (for its own bookkeeping), then the master (because
     * the slave signal reaches the master on IRQ2 and the master also needs
     * to be told the cycle is done).
     */
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    outb(port, inb(port) | (1 << irq));
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    outb(port, inb(port) & ~(1 << irq));
}
