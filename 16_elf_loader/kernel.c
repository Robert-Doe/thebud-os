/*
 * kernel.c — Module 16: ELF Loader
 *
 * What this proves
 * ────────────────
 * User programs no longer live inside the kernel binary.  At build time
 * user_prog.elf is a completely separate ELF file.  The Makefile embeds
 * its raw bytes into kernel.c as a linkable blob using objcopy
 * --input-target binary.  On first boot the kernel writes those bytes to
 * BobFS as "hello.elf", then spawns a ring-0 stub process that calls
 * SYS_EXEC("hello.elf").  SYS_EXEC calls elf_load(), which:
 *   1. Reads "hello.elf" from BobFS into a kernel buffer.
 *   2. Validates the ELF32 header and program headers.
 *   3. Allocates a fresh page directory (paging_new_address_space).
 *   4. For each PT_LOAD segment: allocates physical pages, maps them
 *      user-accessible, copies file data in, zeros BSS.
 *   5. Replaces the calling process's CR3 with the new PD.
 *   6. Builds a new ring-3 fake frame at the ELF entry point (0x400000).
 *   7. Returns the new frame's ESP so isr_common_stub irets to ring-3.
 *
 * Boot sequence
 * ─────────────
 *   Drivers -> paging -> heap -> TSS -> process_init -> BobFS ->
 *   write ELF to disk -> spawn exec_stub -> idle loop ->
 *   first tick -> exec_stub calls SYS_EXEC ->
 *   ELF loaded -> iret to user_main() at 0x400000 in ring 3.
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

/*
 * These symbols are injected by the Makefile via:
 *   objcopy -I binary -O elf32-i386 -B i386 user_prog.elf user_elf_blob.o
 *
 * objcopy --input-target binary exposes:
 *   _binary_user_prog_elf_start  (address of first byte)
 *   _binary_user_prog_elf_end    (address one past last byte)
 *   _binary_user_prog_elf_size   (size as a symbol value)
 */
extern char _binary_user_prog_elf_start;
extern char _binary_user_prog_elf_end;

static void exec_stub(void) {
    int r = sys_exec("hello.elf");
    kprintf("[exec_stub] SYS_EXEC failed: %d\n", r);
    sys_exit(1);
}

void kernel_main(void) {
    uint32_t cur_esp;
    uint32_t elf_size;
    int      r;

    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 16 -- ELF Loader\n\n");

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
        kprintf("  ERROR: no disk (-hda disk.img required)\n");
        for (;;) __asm__ volatile ("hlt");
    }

    r = fs_init();
    kprintf("  BobFS: %s\n", r == 1 ? "formatted" : (r == 0 ? "loaded" : "ERROR"));

    elf_size = (uint32_t)(&_binary_user_prog_elf_end - &_binary_user_prog_elf_start);
    kprintf("\n  Writing hello.elf to BobFS (%u bytes)...\n", elf_size);
    fs_delete("hello.elf");
    r = fs_create("hello.elf", &_binary_user_prog_elf_start, (int)elf_size);
    if (r < 0) {
        kprintf("  ERROR: fs_create failed (%d)\n", r);
        for (;;) __asm__ volatile ("hlt");
    }
    kprintf("  hello.elf written.\n");

    kprintf("\n  Spawning exec_stub -> SYS_EXEC(\"hello.elf\")...\n\n");
    if (!process_create(exec_stub)) {
        kprintf("  ERROR: process_create failed\n");
        for (;;) __asm__ volatile ("hlt");
    }

    kprintf("[kernel] idle (PID 1).  exec_stub runs on next tick.\n\n");
    for (;;) __asm__ volatile ("hlt");
}
