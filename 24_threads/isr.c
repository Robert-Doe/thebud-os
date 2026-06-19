/*
 * isr.c — C-level interrupt dispatcher
 *
 * Module 15 change: CPU exceptions from ring 3 are handled gracefully.
 * Instead of kernel panic, the registered user_fault_hook is called to
 * kill the faulting process and switch to the next one.  Exceptions from
 * ring 0 still panic — they indicate a kernel bug.
 */

#include <stdint.h>
#include "isr.h"
#include "pic.h"
#include "vga.h"

static volatile irq_handler_t irq_handlers[16];

void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) irq_handlers[irq] = handler;
}

static uint32_t (*scheduler_hook)(uint32_t esp) = 0;
static uint32_t (*syscall_hook)(struct interrupt_frame *frame) = 0;
static uint32_t (*user_fault_hook)(uint32_t esp) = 0;

void isr_set_scheduler(uint32_t (*fn)(uint32_t esp))                     { scheduler_hook   = fn; }
void isr_set_syscall_handler(uint32_t (*fn)(struct interrupt_frame *f))  { syscall_hook     = fn; }
void isr_set_user_fault_handler(uint32_t (*fn)(uint32_t esp))            { user_fault_hook  = fn; }

static const char *exception_names[32] = {
    "#DE Divide-by-Zero",    "#DB Debug",
    "NMI",                   "#BP Breakpoint",
    "#OF Overflow",          "#BR Bound Range",
    "#UD Invalid Opcode",    "#NM FPU Unavailable",
    "#DF Double Fault",      "Coprocessor Overrun",
    "#TS Invalid TSS",       "#NP Segment Not Present",
    "#SS Stack Fault",       "#GP General Protection",
    "#PF Page Fault",        "Reserved (15)",
    "#MF x87 FP Error",      "#AC Alignment Check",
    "#MC Machine Check",     "#XM SIMD FP",
    "#VE Virtualisation",    "Reserved (21)",
    "Reserved (22)",         "Reserved (23)",
    "Reserved (24)",         "Reserved (25)",
    "Reserved (26)",         "Reserved (27)",
    "Reserved (28)",         "Reserved (29)",
    "#SX Security",          "Reserved (31)"
};

static void kernel_panic(struct interrupt_frame *f) {
    vga_set_color(VGA_COLOR(VGA_RED, VGA_WHITE));
    kprintf("\n*** KERNEL PANIC ***\n");
    kprintf("  Exception #%u: %s\n", f->int_no,
            f->int_no < 32 ? exception_names[f->int_no] : "?");
    kprintf("  Error code: 0x%x\n", f->err_code);
    kprintf("  EIP=0x%x  CS=0x%x  EFLAGS=0x%x\n", f->eip, f->cs, f->eflags);
    kprintf("  EAX=0x%x  EBX=0x%x  ECX=0x%x  EDX=0x%x\n",
            f->eax, f->ebx, f->ecx, f->edx);
    kprintf("  ESI=0x%x  EDI=0x%x  EBP=0x%x\n", f->esi, f->edi, f->ebp);
    kprintf("  DS=0x%x  ES=0x%x  FS=0x%x  GS=0x%x\n",
            f->ds, f->es, f->fs, f->gs);
    kprintf("\n  System halted.\n");
    __asm__ volatile ("cli; hlt");
    for (;;) {}
}

uint32_t interrupt_handler(struct interrupt_frame *frame) {
    uint32_t vec = frame->int_no;

    if (vec < 32) {
        /* Check whether the exception came from ring 3 */
        if ((frame->cs & 3u) == 3u && user_fault_hook != 0) {
            uint32_t fault_addr = 0;
            vga_set_color(VGA_COLOR(VGA_RED, VGA_LIGHT_GREY));
            if (vec == 14u) {
                __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));
                kprintf("\n[#PF] ring-3 process killed: illegal access to 0x%x"
                        "  (err=0x%x  eip=0x%x)\n",
                        fault_addr, frame->err_code, frame->eip);
            } else {
                kprintf("\n[exception #%u] ring-3 process killed"
                        "  (eip=0x%x)\n", vec, frame->eip);
            }
            vga_set_color(VGA_COLOR(VGA_BLACK, VGA_WHITE));
            return user_fault_hook((uint32_t)frame);
        }
        kernel_panic(frame);
        return (uint32_t)frame; /* unreachable */
    }

    if (vec < 48u) {
        uint8_t irq = (uint8_t)(vec - 32u);
        pic_send_eoi(irq);

        if (irq == 0 && scheduler_hook != 0) {
            uint32_t new_esp = scheduler_hook((uint32_t)frame);
            if (irq_handlers[0] != 0) irq_handlers[0](frame);
            return new_esp;
        }

        if (irq_handlers[irq] != 0) irq_handlers[irq](frame);
    }

    if (vec == 0x80u) {
        if (syscall_hook != 0) return syscall_hook(frame);
        return (uint32_t)frame;
    }

    return (uint32_t)frame;
}
