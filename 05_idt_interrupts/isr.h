/*
 * isr.h — Interrupt Service Routine interface
 *
 * Defines the layout of the register snapshot the assembly stubs push onto
 * the stack before calling C, the IRQ handler registration API, and the
 * main C-level dispatch function called from isr.asm.
 */

#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/*
 * struct interrupt_frame — complete CPU state at the moment of an interrupt.
 *
 * The assembly stub in isr.asm builds this on the stack (by pushing segments,
 * using 'pusha', and letting the CPU push its own state) and then passes a
 * pointer to it as the argument to interrupt_handler().
 *
 * Layout on the stack (lowest address = what ESP points to, going upward):
 *
 *   gs, fs, es, ds          ← pushed by stub (push gs last = lowest)
 *   edi,esi,ebp,esp_dummy,
 *   ebx,edx,ecx,eax         ← pushed by pusha (edi first = just above ds)
 *   int_no                  ← pushed by stub (interrupt vector number)
 *   err_code                ← pushed by CPU (or dummy 0 by stub if not applicable)
 *   eip, cs, eflags         ← pushed by CPU automatically on interrupt entry
 *
 * All fields are uint32_t because 'pusha' and the CPU always push 32-bit
 * values in Protected Mode.
 */
struct interrupt_frame {
    /* segment registers — saved/restored by stub */
    uint32_t gs, fs, es, ds;
    /* general-purpose registers — saved/restored by pusha/popa */
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    /* interrupt identification — pushed by stub */
    uint32_t int_no, err_code;
    /* CPU state — pushed automatically by the CPU on interrupt entry */
    uint32_t eip, cs, eflags;
} __attribute__((packed));

/*
 * irq_handler_t — type of a hardware IRQ handler function.
 * Registered via irq_register().  The frame pointer lets the handler
 * inspect the interrupted context if needed.
 */
typedef void (*irq_handler_t)(struct interrupt_frame *frame);

/*
 * irq_register — install a handler for hardware IRQ line 0-15.
 * Can be called any time before or after sti; takes effect on the next
 * occurrence of that IRQ.  Registering NULL removes the handler.
 */
void irq_register(uint8_t irq, irq_handler_t handler);

/*
 * interrupt_handler — the single C entry point for ALL 48 vectors (0-47).
 * Called by isr_common_stub in isr.asm.  Dispatches to exception panic
 * or IRQ handler table based on frame->int_no.
 *
 * Do NOT call this directly from C.
 */
void interrupt_handler(struct interrupt_frame *frame);

#endif /* ISR_H */
