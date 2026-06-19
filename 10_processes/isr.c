/*
 * isr.c — C-level interrupt dispatcher
 *
 * interrupt_handler() is called by isr_common_stub in isr.asm for every
 * interrupt vector 0-47.  It decides whether the interrupt is a CPU
 * exception (0-31) or a hardware IRQ (32-47), then:
 *
 *   Exception (0-31): print a full register dump and halt (kernel panic).
 *   Hardware IRQ (32-47): send EOI to the PIC, then call the registered
 *                         handler if one exists.
 */

#include <stdint.h>
#include "isr.h"
#include "pic.h"
#include "vga.h"

static volatile irq_handler_t irq_handlers[16];

void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}

static uint32_t (*scheduler_hook)(uint32_t esp) = 0;

void isr_set_scheduler(uint32_t (*fn)(uint32_t esp)) {
    scheduler_hook = fn;
}

static const char *exception_names[32] = {
    "#DE Divide-by-Zero",
    "#DB Debug",
    "     Non-Maskable Interrupt",
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available (FPU)",
    "#DF Double Fault",
    "     Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack-Segment Fault",
    "#GP General Protection Fault",
    "#PF Page Fault",
    "     Reserved (15)",
    "#MF x87 FPU Error",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD FP Exception",
    "#VE Virtualisation Exception",
    "     Reserved (21)",
    "     Reserved (22)",
    "     Reserved (23)",
    "     Reserved (24)",
    "     Reserved (25)",
    "     Reserved (26)",
    "     Reserved (27)",
    "     Reserved (28)",
    "     Reserved (29)",
    "#SX Security Exception",
    "     Reserved (31)"
};

static void kernel_panic(struct interrupt_frame *f) {
    vga_set_color(VGA_COLOR(VGA_RED, VGA_WHITE));
    kprintf("\n");
    kprintf("  *** KERNEL PANIC ***\n");
    kprintf("  Exception #%u: %s\n",
            f->int_no,
            (f->int_no < 32) ? exception_names[f->int_no] : "Unknown");
    kprintf("  Error code : 0x%x\n", f->err_code);
    kprintf("\n");
    vga_set_color(VGA_COLOR(VGA_RED, VGA_LIGHT_GREY));
    kprintf("  EIP=0x%x  CS=0x%x  EFLAGS=0x%x\n",
            f->eip, f->cs, f->eflags);
    kprintf("  EAX=0x%x  EBX=0x%x  ECX=0x%x  EDX=0x%x\n",
            f->eax, f->ebx, f->ecx, f->edx);
    kprintf("  ESI=0x%x  EDI=0x%x  EBP=0x%x\n",
            f->esi, f->edi, f->ebp);
    kprintf("  DS=0x%x  ES=0x%x  FS=0x%x  GS=0x%x\n",
            f->ds, f->es, f->fs, f->gs);
    vga_set_color(VGA_COLOR(VGA_RED, VGA_WHITE));
    kprintf("\n  System halted.\n");

    __asm__ volatile ("cli; hlt");
    for (;;) {}
}

uint32_t interrupt_handler(struct interrupt_frame *frame) {
    uint32_t vec = frame->int_no;

    if (vec < 32) {
        kernel_panic(frame);
        return (uint32_t)frame; /* unreachable — kernel_panic halts */
    }

    if (vec < 48) {
        uint8_t irq = (uint8_t)(vec - 32);
        pic_send_eoi(irq);

        /* IRQ0 (timer) drives the scheduler.  Ask it for the next ESP before
         * calling any registered handler so the switch happens on every tick. */
        if (irq == 0 && scheduler_hook != 0) {
            uint32_t new_esp = scheduler_hook((uint32_t)frame);
            if (irq_handlers[0] != 0) {
                irq_handlers[0](frame);
            }
            return new_esp;
        }

        if (irq_handlers[irq] != 0) {
            irq_handlers[irq](frame);
        }
    }

    return (uint32_t)frame;
}
