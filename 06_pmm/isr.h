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
 * Layout on the stack (lowest address = what ESP points to, going upward):
 *
 *   gs, fs, es, ds          ← pushed by stub
 *   edi,esi,ebp,esp_dummy,
 *   ebx,edx,ecx,eax         ← pushed by pusha
 *   int_no                  ← pushed by stub
 *   err_code                ← pushed by CPU (or dummy 0 by stub)
 *   eip, cs, eflags         ← pushed by CPU automatically
 */
struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} __attribute__((packed));

typedef void (*irq_handler_t)(struct interrupt_frame *frame);

void irq_register(uint8_t irq, irq_handler_t handler);
void interrupt_handler(struct interrupt_frame *frame);

#endif /* ISR_H */
