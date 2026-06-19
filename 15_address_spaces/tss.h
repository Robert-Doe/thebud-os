/*
 * tss.h — Task State Segment
 *
 * The TSS is a hardware structure required by the x86 CPU whenever a
 * privilege-level change occurs on an interrupt or exception.  When an IRQ or
 * syscall fires while code is running at ring 3, the CPU needs to know:
 *
 *   "Where is the kernel's stack?"
 *
 * It finds the answer in the TSS: the `esp0` field gives the ring-0 stack
 * pointer to switch to, and `ss0` gives the ring-0 stack segment.  The CPU
 * loads these automatically before pushing any exception frame, so the kernel
 * always has a clean stack to receive interrupts from user-mode code.
 *
 * We fill only the fields we use.  The rest of the 104-byte structure is
 * zeroed and never touched.
 *
 * ── How the CPU uses the TSS ──────────────────────────────────────────────
 *
 *  1. Ring-3 code executes.  IRQ0 fires.
 *  2. CPU loads SS = TSS.ss0, ESP = TSS.esp0.          ← the switch
 *  3. CPU pushes user SS, user ESP, EFLAGS, CS, EIP
 *     onto the NEW kernel stack.
 *  4. ISR stub pushes the rest of the frame.
 *  5. Kernel handles the interrupt, returns via iret.
 *  6. iret sees CS.RPL=3 and additionally pops user ESP and user SS,
 *     restoring the user-mode stack.
 *
 * ── One TSS per CPU, not per process ──────────────────────────────────────
 *
 * Unlike a naive reading of Intel docs might suggest, you do NOT need one TSS
 * per process.  You need one TSS per CPU.  Before scheduling a ring-3 process,
 * the scheduler calls tss_set_kernel_stack() to update TSS.esp0 to point at
 * the top of that process's kernel stack.  One TSS, updated in O(1).
 */

#ifndef TSS_H
#define TSS_H

#include <stdint.h>

/*
 * The minimal 32-bit TSS.  All 26 fields are shown for completeness;
 * we only set ss0, esp0, and iomap_base.
 */
struct tss_entry {
    uint32_t prev_tss;          /* previous TSS selector (for hardware task switching) */
    uint32_t esp0;              /* ← ring-0 stack pointer — the ONLY field we update   */
    uint32_t ss0;               /* ← ring-0 stack segment — set once to GDT_SEL_DATA   */
    uint32_t esp1, ss1;         /* ring-1 stack (unused)                               */
    uint32_t esp2, ss2;         /* ring-2 stack (unused)                               */
    uint32_t cr3;               /* page directory base (unused — we share one PD)      */
    uint32_t eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;        /* offset to I/O permission bitmap (set to sizeof TSS) */
} __attribute__((packed));

/*
 * tss_init(kernel_ss, kernel_esp)
 *
 * Zeroes the TSS, sets ss0/esp0 to the provided kernel stack, installs the
 * TSS descriptor in GDT entry 5, and executes `ltr` to load the Task Register.
 * Call once, after gdt_init() and before enabling interrupts or entering ring 3.
 *
 * kernel_esp: top of the current kernel stack (a value of ESP right now is fine
 *             because we update it again before any ring-3 code runs).
 * kernel_ss:  must be GDT_SEL_DATA (0x10).
 */
void tss_init(uint32_t kernel_ss, uint32_t kernel_esp);

/*
 * tss_set_kernel_stack(esp)
 *
 * Update TSS.esp0 to `esp`.  Call from scheduler_tick() whenever the
 * next process is a ring-3 process, so the CPU will switch to its kernel
 * stack on the next interrupt.
 */
void tss_set_kernel_stack(uint32_t esp);

#endif /* TSS_H */
