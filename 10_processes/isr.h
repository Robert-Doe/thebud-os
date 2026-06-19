/*
 * isr.h — Interrupt Service Routine interface
 *
 * Module 10 changes:
 *   - interrupt_handler() now returns uint32_t (the ESP to restore after the
 *     interrupt).  Normally it returns (uint32_t)frame — no change.  When the
 *     scheduler decides to switch processes it returns the NEXT process's saved
 *     ESP instead.  isr_common_stub uses this return value (EAX) to set ESP
 *     before the pop/iret sequence, so the CPU restores a different process's
 *     registers and jumps to a different EIP.  That is the entire context switch.
 *   - isr_set_scheduler() lets process.c register its callback without isr.c
 *     needing to include process.h (avoids a circular header dependency).
 */

#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/*
 * struct interrupt_frame — complete CPU state at the moment of an interrupt.
 *
 * Stack layout (lowest address = ESP after all stub pushes, going upward):
 *
 *   gs, fs, es, ds                               pushed by stub (push ds/es/fs/gs)
 *   edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax pushed by pusha
 *   int_no                                        pushed by stub
 *   err_code                                      pushed by CPU or dummy 0
 *   eip, cs, eflags                               pushed by CPU on interrupt
 */
struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} __attribute__((packed));

typedef void (*irq_handler_t)(struct interrupt_frame *frame);

void     irq_register(uint8_t irq, irq_handler_t handler);

/* Called by isr_common_stub. Returns the ESP whose register snapshot will be
 * restored.  Returns (uint32_t)frame when no switch is needed. */
uint32_t interrupt_handler(struct interrupt_frame *frame);

/* Register the scheduler's context-switch hook.
 * fn(current_esp) saves current_esp into the current process's PCB and
 * returns the next ready process's saved ESP. */
void isr_set_scheduler(uint32_t (*fn)(uint32_t current_esp));

#endif /* ISR_H */
