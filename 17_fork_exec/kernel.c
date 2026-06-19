/*
 * kernel.c — Module 17: fork & exec
 *
 * What this proves
 * ────────────────
 * SYS_FORK deep-copies a process's entire address space into a fresh page
 * directory.  Both parent and child resume after the fork() call — the child
 * sees 0 in EAX; the parent sees the child's PID.  SYS_WAIT blocks the parent
 * until the child exits (becomes PROC_ZOMBIE), then reaps the zombie and
 * returns the child's exit code.  SYS_EXIT now stores the exit code in the PCB
 * before zombifying, and frees the process's physical pages.
 *
 * Boot sequence
 * ─────────────
 *   Drivers -> paging -> heap -> TSS -> process_init ->
 *   BobFS -> write fork_demo.elf -> spawn exec_stub ->
 *   exec_stub calls SYS_EXEC("fork_demo.elf") ->
 *   user_main() runs at ring 3 at 0x400000 ->
 *   user_main() calls SYS_FORK ->
 *   child: prints, SYS_EXIT(42) ->
 *   parent: SYS_WAIT -> gets 42 -> SYS_EXIT(0).
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
    int r = sys_exec("fork_demo.elf");
    kprintf("[exec_stub] SYS_EXEC failed: %d\n", r);
    sys_exit(1);
}

void kernel_main(void) {
    uint32_t cur_esp;
    uint32_t elf_size;
    int      r;

    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 17 -- fork & exec\n\n");

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

    process_init(); kprintf("  Scheduler (PID 1 = idle)\n");

    disk_init();
    if (!disk_present()) {
        kprintf("  ERROR: no disk\n");
        for (;;) __asm__ volatile ("hlt");
    }

    r = fs_init();
    kprintf("  BobFS: %s\n", r == 1 ? "formatted" : "loaded");

    elf_size = (uint32_t)(&_binary_user_prog_elf_end - &_binary_user_prog_elf_start);
    kprintf("\n  Writing fork_demo.elf (%u bytes)...\n", elf_size);
    fs_delete("fork_demo.elf");
    r = fs_create("fork_demo.elf", &_binary_user_prog_elf_start, (int)elf_size);
    if (r < 0) {
        kprintf("  ERROR: fs_create failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("  Spawning exec_stub -> fork_demo.elf\n\n");
    if (!process_create(exec_stub)) {
        kprintf("  ERROR: process_create failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("[kernel] idle.  Watch fork/wait play out below:\n\n");
    for (;;) __asm__ volatile ("hlt");
}
