/*
 * keyboard.h — PS/2 Keyboard Driver interface
 *
 * The keyboard controller fires IRQ1 (CPU vector 33) every time a key is
 * pressed or released.  The handler reads a one-byte "scancode" from I/O
 * port 0x60, translates it to an ASCII character using a lookup table, and
 * pushes the result into a small ring buffer.
 *
 * The rest of the kernel reads characters out of that buffer by calling
 * keyboard_getchar().  The call is non-blocking — it returns 0 immediately
 * if no key is waiting.
 *
 * Scancode set: PS/2 Set 1 (the default on QEMU and real BIOS-initialised
 * keyboards).  Each key press emits a "make" code (bit 7 = 0); each release
 * emits a "break" code (make code | 0x80).  We only act on make codes.
 *
 * Shift handling: left shift (0x2A) and right shift (0x36) make/break codes
 * are tracked so letters come out uppercase when shift is held.
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/*
 * keyboard_init — register the IRQ1 handler and enable the keyboard IRQ line.
 *
 * Call after idt_init() and pic_init() and before sti().
 * (Calling after sti() is also fine — the handler is simply not live yet.)
 */
void keyboard_init(void);

/*
 * keyboard_getchar — return the next ASCII character from the ring buffer.
 *
 * Non-blocking: returns 0 immediately if no key is waiting.
 * Returns the ASCII value of the character otherwise.
 *
 * Special keys:
 *   Enter     → '\n'  (0x0A)
 *   Backspace → '\b'  (0x08)
 *   Tab       → '\t'  (0x09)
 *   Escape    → 0x1B
 *   Space     → ' '
 *
 * Keys with no ASCII mapping (function keys, ctrl, alt, etc.) are silently
 * discarded — they never enter the ring buffer.
 */
char keyboard_getchar(void);

/*
 * keyboard_pending — return non-zero if at least one character is waiting.
 */
int keyboard_pending(void);

#endif /* KEYBOARD_H */
