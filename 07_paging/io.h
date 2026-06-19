/*
 * io.h — x86 I/O port access helpers
 *
 * The CPU communicates with peripheral chips (PIC, PIT, keyboard controller,
 * etc.) via a separate I/O address space accessed with the 'in' and 'out'
 * instructions.  These are NOT normal memory reads/writes — they go to
 * dedicated hardware registers inside the chip, not RAM.
 *
 * All functions are 'static inline': the compiler pastes them directly at the
 * call site (no function call overhead) and 'static' keeps them from leaking
 * to other translation units.
 */

#ifndef IO_H
#define IO_H

#include <stdint.h>

/* Write a byte to an I/O port */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Read a byte from an I/O port */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * io_wait — burn ~1-4 µs by writing to the unused diagnostic port 0x80.
 * Old ISA hardware needs a small delay between consecutive port writes
 * during chip initialization sequences.  Not strictly needed on QEMU but
 * correct practice.
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif /* IO_H */
