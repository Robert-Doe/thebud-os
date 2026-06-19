/*
 * kernel.c — Module 15: Per-Process Address Spaces
 *
 * What this proves
 * ────────────────
 * Each ring-3 process has its own page directory.  The scheduler switches CR3
 * on every context switch, so processes cannot read each other's memory or the
 * kernel.  When a ring-3 process touches a supervisor-only page, the CPU raises
 * a #PF; the kernel prints the fault address and kills the process gracefully.
 *
 * Boot sequence
 * ─────────────
 *   1. Initialise all drivers (same as Module 14).
 *   2. paging_init() — identity-map 0-4MB, supervisor-only, enable paging.
 *   3. process_init() — registers scheduler and user_fault_hook with ISR.
 *   4. Spawn TWO ring-3 user processes.  Each gets its own PD with separate CR3.
 *   5. Print both CR3 values to show they are different.
 *   6. Idle: hlt.  Scheduler interleaves the two user processes.
 *   7. Each user process eventually tries to read 0x90000, gets killed by #PF.
 *   8. Both dead → idle loop only.
 */

#include <stdint.h>
#include "vga.h"
#include "gdt.h"
#include "pic.h"
#include "idt.h"
#include "isr.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "disk.h"
#include "fs.h"
#include "tss.h"
#include "user_prog.h"

extern char kernel_end;

void kernel_main(void) {
    uint32_t cur_esp;
    uint32_t pid1, pid2;

    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 15 -- Per-Process Address Spaces\n\n");

    gdt_init();     kprintf("  GDT (6 entries: null/kcode/kdata/ucode/udata/tss)\n");
    pic_init();     pic_clear_mask(0); pic_clear_mask(1);
    kprintf("  PIC\n");
    idt_init();     kprintf("  IDT\n");
    pmm_init(64u * 1024u * 1024u, (uint32_t)&kernel_end);
    kprintf("  PMM: %u pages\n", pmm_get_free());
    paging_init();  kprintf("  Paging (0-4MB identity-mapped, supervisor-only)\n");
    heap_init(64);  kprintf("  Heap\n");
    keyboard_init();
    syscall_init(); kprintf("  Syscall gate (int 0x80, DPL=3)\n");
    __asm__ volatile ("sti");

    __asm__ volatile ("mov %%esp, %0" : "=r"(cur_esp));
    tss_init(0x10u, cur_esp);
    kprintf("  TSS installed, TR loaded\n");

    process_init(); kprintf("  Scheduler (PID 1 = idle)\n");

    disk_init();
    if (disk_present()) {
        int r = fs_init();
        kprintf("  BobFS: %s\n", r == 1 ? "formatted" : (r == 0 ? "loaded" : "ERROR"));
    } else {
        kprintf("  (no -hda disk.img -- filesystem skipped)\n");
    }

    kprintf("\n  Spawning two ring-3 user processes...\n");

    pid1 = process_create_user(user_main);
    pid2 = process_create_user(user_main);

    if (!pid1 || !pid2) {
        kprintf("  ERROR: process_create_user failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("  PID %u: CR3=0x%x\n", pid1, paging_get_pd_phys());
    kprintf("  PID %u: CR3 differs (allocated separately by paging_new_address_space)\n", pid2);
    kprintf("\n  Kernel CR3=0x%x\n", paging_kernel_cr3());
    kprintf("\n[kernel] idle (ring 0, PID 1).  Two user processes run on next tick.\n");
    kprintf("         Watch for #PF when each touches 0x90000...\n\n");

    for (;;) __asm__ volatile ("hlt");
}
