/*
 * kernel.c — Module 14: User Mode (Ring 3)
 *
 * What this proves
 * ────────────────
 * Code can run at CPU privilege level 3, isolated from the kernel.  A ring-3
 * process cannot execute privileged instructions or access I/O ports directly.
 * The ONLY way it communicates with the kernel is through the syscall gate
 * (int 0x80, IDT vector 128, DPL=3).
 *
 * Boot sequence
 * ─────────────
 *   1. All drivers initialised (same as Module 13).
 *   2. tss_init() — installs the TSS in GDT entry 5 and loads the TR.
 *   3. process_create_user(user_main) — builds a ring-3 process.
 *   4. Idle loop: hlt forever.
 *   5. First IRQ0: scheduler switches to user_main in ring 3.
 *   6. user_main() calls syscalls, then sys_exit(0).
 *   7. Scheduler returns to the idle process (ring 0).
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

    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 14 -- User Mode (Ring 3)\n\n");

    gdt_init();     kprintf("  GDT (6 entries: null/kcode/kdata/ucode/udata/tss)\n");
    pic_init();     pic_clear_mask(0); pic_clear_mask(1);
    kprintf("  PIC\n");
    idt_init();     kprintf("  IDT\n");
    pmm_init(64u * 1024u * 1024u, (uint32_t)&kernel_end);
    kprintf("  PMM: %u pages\n", pmm_get_free());
    paging_init();  kprintf("  Paging\n");
    heap_init(64);  kprintf("  Heap\n");
    keyboard_init();
    syscall_init(); kprintf("  Syscall gate (int 0x80, DPL=3)\n");
    __asm__ volatile ("sti");

    /*
     * TSS must come after gdt_init() (entry 5 exists) and after the heap
     * (the TSS struct lives in BSS which is always mapped).  We read the
     * current ESP so tss_init has a valid initial value — it will be
     * overwritten before any ring-3 code runs.
     */
    __asm__ volatile ("mov %%esp, %0" : "=r"(cur_esp));
    tss_init(0x10u, cur_esp);
    kprintf("  TSS installed, TR loaded\n");

    process_init(); kprintf("  Scheduler (PID 1 = idle)\n");

    disk_init();
    if (disk_present()) {
        int r = fs_init();
        kprintf("  BobFS: %s\n", r == 1 ? "formatted" : (r == 0 ? "loaded" : "ERROR"));
    } else {
        kprintf("  (no -hda disk.img — filesystem skipped)\n");
    }

    kprintf("\n  Spawning ring-3 process...\n\n");
    if (!process_create_user(user_main)) {
        kprintf("  ERROR: process_create_user failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("[kernel] idle (ring 0, PID 1).  User process runs on next tick.\n\n");
    for (;;) __asm__ volatile ("hlt");
}
