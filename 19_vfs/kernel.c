/*
 * kernel.c -- Module 19: VFS Layer
 *
 * What this proves
 * ----------------
 * A Virtual File System abstraction sits between user syscalls and storage
 * backends.  BobFS becomes one backend; the VGA console is another.  fd 0/1/2
 * are pre-opened for every process.  SYS_OPEN/READ/CLOSE work end-to-end.
 *
 * Boot sequence
 * -------------
 *   Drivers -> paging -> heap -> TSS -> process_init ->
 *   VFS init -> mount backends -> write hello.txt ->
 *   write vfs_demo.elf -> spawn exec_stub ->
 *   user_main() opens hello.txt via SYS_OPEN, reads it, closes it.
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

extern char kernel_end;
extern char _binary_user_prog_elf_start;
extern char _binary_user_prog_elf_end;

static void exec_stub(void) {
    int r = sys_exec("vfs_demo.elf");
    kprintf("[exec_stub] SYS_EXEC failed: %d\n", r);
    sys_exit(1);
}

void kernel_main(void) {
    uint32_t cur_esp;
    uint32_t elf_size;
    int      r;

    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 19 -- VFS Layer\n\n");

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

    /* Init VFS and mount backends */
    vfs_init();
    vfs_mount("/", &bobfs_ops);
    vfs_mount("vga:", &vga_ops);
    kprintf("  VFS: mounted / -> bobfs, vga: -> vga\n");

    /* Write a test file to BobFS */
    fs_delete("hello.txt");
    fs_create("hello.txt", "Hello from BobFS!\n", 18);
    kprintf("  BobFS: wrote hello.txt\n");

    elf_size = (uint32_t)(&_binary_user_prog_elf_end - &_binary_user_prog_elf_start);
    kprintf("\n  Writing vfs_demo.elf (%u bytes)...\n", elf_size);
    fs_delete("vfs_demo.elf");
    r = fs_create("vfs_demo.elf", &_binary_user_prog_elf_start, (int)elf_size);
    if (r < 0) {
        kprintf("  ERROR: fs_create failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("  Spawning exec_stub -> vfs_demo.elf\n\n");
    if (!process_create(exec_stub)) {
        kprintf("  ERROR: process_create failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("[kernel] idle.  Watch VFS demo below:\n\n");
    for (;;) __asm__ volatile ("hlt");
}
