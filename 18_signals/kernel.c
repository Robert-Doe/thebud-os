/*
 * kernel.c — Module 18: Signals
 *
 * What this proves
 * ────────────────
 * Processes can now receive and handle signals asynchronously:
 *   - SYS_SIGNAL installs a user-space handler for a signal number.
 *   - SYS_KILL sends a signal to any process by PID.
 *   - scheduler_tick delivers pending signals to ring-3 processes before
 *     returning the CPU to them — no dedicated "signal check" path needed.
 *   - SIGKILL cannot be caught; it zombifies the target immediately.
 *   - SIGSEGV can be caught: process_fault_handler checks for a registered
 *     handler instead of always killing the faulting process.
 *
 * Boot sequence
 * ─────────────
 *   Drivers -> paging -> heap -> TSS -> process_init ->
 *   BobFS -> write signal_demo.elf -> spawn exec_stub ->
 *   exec_stub calls SYS_EXEC("signal_demo.elf") ->
 *   user_main() runs at ring 3 at 0x400000:
 *     Part A: SIGUSR1 handler fires during yield.
 *     Part B: child catches SIGSEGV from intentional #PF.
 *     Part C: fork/wait baseline (regression check).
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

extern char kernel_end;
extern char _binary_user_prog_elf_start;
extern char _binary_user_prog_elf_end;

static void exec_stub(void) {
    int r = sys_exec("signal_demo.elf");
    kprintf("[exec_stub] SYS_EXEC failed: %d\n", r);
    sys_exit(1);
}

void kernel_main(void) {
    uint32_t cur_esp;
    uint32_t elf_size;
    int      r;

    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 18 -- Signals\n\n");

    gdt_init();     kprintf("  GDT\n");
    pic_init();     pic_clear_mask(0); pic_clear_mask(1);
    kprintf("  PIC\n");
    idt_init();     kprintf("  IDT\n");
    pmm_init(64u * 1024u * 1024u, (uint32_t)&kernel_end);
    kprintf("  PMM: %u pages free\n", pmm_get_free());
    paging_init();  kprintf("  Paging\n");
    heap_init(64);  kprintf("  Heap\n");
    keyboard_init();
    syscall_init(); kprintf("  Syscall gate\n");
    __asm__ volatile ("sti");

    __asm__ volatile ("mov %%esp, %0" : "=r"(cur_esp));
    tss_init(0x10u, cur_esp);
    kprintf("  TSS\n");

    process_init(); kprintf("  Scheduler + signal subsystem\n");

    disk_init();
    if (!disk_present()) {
        kprintf("  ERROR: no disk\n");
        for (;;) __asm__ volatile ("hlt");
    }

    r = fs_init();
    kprintf("  BobFS: %s\n", r == 1 ? "formatted" : "loaded");

    elf_size = (uint32_t)(&_binary_user_prog_elf_end - &_binary_user_prog_elf_start);
    kprintf("\n  Writing signal_demo.elf (%u bytes)...\n", elf_size);
    fs_delete("signal_demo.elf");
    r = fs_create("signal_demo.elf", &_binary_user_prog_elf_start, (int)elf_size);
    if (r < 0) {
        kprintf("  ERROR: fs_create failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("  Spawning exec_stub -> signal_demo.elf\n\n");
    if (!process_create(exec_stub)) {
        kprintf("  ERROR: process_create failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("[kernel] idle.  Watch signal delivery below:\n\n");
    for (;;) __asm__ volatile ("hlt");
}
