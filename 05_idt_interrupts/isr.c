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

/* -------------------------------------------------------------------------
 * IRQ handler dispatch table.
 * Indexed by IRQ line (0-15), not vector number.
 * NULL = no handler registered for that IRQ.
 * 'volatile' because interrupt_handler() writes it and the main thread reads
 * it (and vice versa), so the compiler must not cache the value.
 * ------------------------------------------------------------------------- */
static volatile irq_handler_t irq_handlers[16];

void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}

/* -------------------------------------------------------------------------
 * Exception names for the 32 CPU-defined vectors.
 * ------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------
 * kernel_panic — display a full register dump and halt.
 * Called when a CPU exception fires with no recovery path.
 * ------------------------------------------------------------------------- */
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

    /* Disable interrupts and spin forever */
    __asm__ volatile ("cli; hlt");
    for (;;) {}
}

/* -------------------------------------------------------------------------
 * interrupt_handler — called from isr_common_stub for every vector 0-47.
 * ------------------------------------------------------------------------- */
void interrupt_handler(struct interrupt_frame *frame) {
    uint32_t vec = frame->int_no;

    if (vec < 32) {
        /* CPU exception — no recovery, print panic and halt */
        kernel_panic(frame);

    } else if (vec < 48) {
        /* Hardware IRQ — acknowledge PIC, then dispatch */
        uint8_t irq = (uint8_t)(vec - 32);

        /*
         * EOI must be sent BEFORE calling the handler so that the PIC can
         * accept new interrupts from other lines while we're handling this one.
         * (Some kernels send EOI at the end, but that delays other IRQs
         * unnecessarily when the handler is slow.)
         */
        pic_send_eoi(irq);

        if (irq_handlers[irq] != 0) {
            irq_handlers[irq](frame);
        }
    }
    /* vectors 48-255: not installed in IDT, so they can't reach here */
}
