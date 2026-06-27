/*
 * isr.h — Interrupt Service Routine interface
 *
 * Module 15 addition:
 *   isr_set_user_fault_handler() — register a callback for CPU exceptions
 *   that fire from ring 3.  Instead of kernel panic, the callback is
 *   invoked with the current kernel-side ESP; it should mark the process
 *   dead and return the next ready process's ESP (same contract as
 *   process_exit()).  Registered by process_init().
 */

#ifndef ISR_H
#define ISR_H

#include <stdint.h>

struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} __attribute__((packed));

typedef void (*irq_handler_t)(struct interrupt_frame *frame);

void     irq_register(uint8_t irq, irq_handler_t handler);
uint32_t interrupt_handler(struct interrupt_frame *frame);

void isr_set_scheduler(uint32_t (*fn)(uint32_t current_esp));
void isr_set_syscall_handler(uint32_t (*fn)(struct interrupt_frame *frame));

/*
 * isr_set_user_fault_handler(fn)
 *
 * Register fn as the handler for CPU exceptions (vectors 0-31) that fire
 * while code runs at ring 3 (frame->cs & 3 == 3).
 *
 * fn(current_esp) must:
 *   1. Mark the faulting process DEAD.
 *   2. Return the saved ESP of the next READY process.
 *
 * process_exit() satisfies this contract exactly.
 */
void isr_set_user_fault_handler(uint32_t (*fn)(uint32_t esp));

#endif /* ISR_H */
