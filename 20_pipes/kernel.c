/*
 * kernel.c -- Module 20: Pipes & IPC
 *
 * What this proves
 * ----------------
 * Anonymous kernel pipes connect two processes via a shared ring buffer.
 * SYS_PIPE returns two file descriptors (read end, write end) backed by
 * pipe_read_ops / pipe_write_ops vtables in the VFS layer.  fork() copies
 * the fd table, so both parent and child hold references to the same pipe slot.
 *
 * Boot sequence
 * -------------
 *   Drivers -> paging -> heap -> TSS -> pipe_init -> process_init ->
 *   VFS init -> write pipe_demo.elf -> spawn exec_stub ->
 *   user_main() creates pipe, forks, child writes, parent reads.
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
#include "vfs.h"
#include "pipe.h"

extern char kernel_end;
extern char _binary_user_prog_elf_start;
extern char _binary_user_prog_elf_end;

static void exec_stub(void) {
    int r = sys_exec("pipe_demo.elf");
    kprintf("[exec_stub] SYS_EXEC failed: %d\n", r);
    sys_exit(1);
}

void kernel_main(void) {
    uint32_t cur_esp;
    uint32_t elf_size;
    int      r;

    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 20 -- Pipes & IPC\n\n");

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

    pipe_init();
    kprintf("  Pipes\n");

    process_init(); kprintf("  Scheduler\n");

    disk_init();
    if (!disk_present()) {
        kprintf("  ERROR: no disk\n");
        for (;;) __asm__ volatile ("hlt");
    }

    r = fs_init();
    kprintf("  BobFS: %s\n", r == 1 ? "formatted" : "loaded");

    vfs_init();
    vfs_mount("/", &bobfs_ops);
    vfs_mount("vga:", &vga_ops);
    kprintf("  VFS: mounted\n");

    elf_size = (uint32_t)(&_binary_user_prog_elf_end - &_binary_user_prog_elf_start);
    kprintf("\n  Writing pipe_demo.elf (%u bytes)...\n", elf_size);
    fs_delete("pipe_demo.elf");
    r = fs_create("pipe_demo.elf", &_binary_user_prog_elf_start, (int)elf_size);
    if (r < 0) {
        kprintf("  ERROR: fs_create failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("  Spawning exec_stub -> pipe_demo.elf\n\n");
    if (!process_create(exec_stub)) {
        kprintf("  ERROR: process_create failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("[kernel] idle.  Watch pipe demo below:\n\n");
    for (;;) __asm__ volatile ("hlt");
}
