/*
 * kernel.c — Module 12: File System
 *
 * What this proves
 * ────────────────
 * BobFS can create, write, read, list, and delete files on a virtual hard
 * disk image (-hda disk.img).  The ATA PIO driver talks directly to the
 * emulated IDE controller at I/O ports 0x1F0-0x1F7.
 *
 * Demo sequence
 * ─────────────
 *   1. Detect and initialise the ATA disk.
 *   2. fs_init() — auto-formats if blank, loads superblock & directory.
 *   3. Create three files with different content.
 *   4. List all files (proves the directory was written to disk).
 *   5. Read each file back and print its content (proves data was stored).
 *   6. Delete one file, list again (proves deletion works).
 *   7. Attempt to read the deleted file (proves it returns -1).
 *
 * Between runs: the disk.img persists, so if you run the demo twice without
 * deleting disk.img you'll see "file already exists" errors — that is correct
 * behaviour and shows persistence across boots.
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

extern char kernel_end;

/* ── fs_list callback ────────────────────────────────────────────────── */

static void print_dirent(const char *name, uint32_t size) {
    kprintf("    %-20s  %u bytes\n", name, size);
}

/* ── Read buffer (static — keeps it off the stack) ───────────────────── */
static char read_buf[FS_MAX_FILE_SIZE + 1];

/* ── Kernel entry ────────────────────────────────────────────────────── */

void kernel_main(void) {
    int ret;

    vga_init(VGA_COLOR(VGA_BLACK, VGA_WHITE));
    kprintf("[BobOS] Module 12 -- File System\n\n");

    gdt_init();     kprintf("  GDT installed\n");
    pic_init();     pic_clear_mask(0);  pic_clear_mask(1);
    kprintf("  PIC remapped\n");
    idt_init();     kprintf("  IDT installed\n");
    pmm_init(64u * 1024u * 1024u, (uint32_t)&kernel_end);
    kprintf("  PMM: %u free pages\n", pmm_get_free());
    paging_init();  kprintf("  Paging ON\n");
    heap_init(64);  kprintf("  Heap: %u KB\n", (64u * 4096u) / 1024u);
    keyboard_init();
    syscall_init(); kprintf("  Syscall gate ready\n");
    __asm__ volatile ("sti");
    process_init(); kprintf("  Scheduler ready\n\n");

    /* ── Disk init ──────────────────────────────────────────────────── */
    disk_init();
    if (!disk_present()) {
        kprintf("  ERROR: no ATA disk found.\n");
        kprintf("  Run with: qemu-system-i386 -fda os.img -hda disk.img\n");
        for (;;) __asm__ volatile ("hlt");
    }
    kprintf("  ATA disk detected\n");

    /* ── Filesystem init ────────────────────────────────────────────── */
    ret = fs_init();
    if (ret < 0) {
        kprintf("  ERROR: fs_init failed\n");
        for (;;) __asm__ volatile ("hlt");
    }
    if (ret == 1)
        kprintf("  BobFS: fresh disk formatted\n");
    else
        kprintf("  BobFS: existing filesystem loaded\n");
    kprintf("\n");

    /* ── Step 1: Create files ───────────────────────────────────────── */
    kprintf("--- Creating files ---\n");

    ret = fs_create("hello.txt",
                    "Hello, BobOS! This is the first file.\n", 38);
    kprintf("  create hello.txt  : %s\n", ret==0?"OK":(ret==-2?"exists":"ERR"));

    ret = fs_create("notes.txt",
                    "BobOS has:\n"
                    "  - Bootloader\n"
                    "  - Protected Mode\n"
                    "  - VGA driver\n"
                    "  - Interrupts\n"
                    "  - Paging\n"
                    "  - Heap\n"
                    "  - Processes\n"
                    "  - Syscalls\n"
                    "  - Filesystem\n", 142);
    kprintf("  create notes.txt  : %s\n", ret==0?"OK":(ret==-2?"exists":"ERR"));

    ret = fs_create("count.txt", "1 2 3 4 5 6 7 8 9 10\n", 21);
    kprintf("  create count.txt  : %s\n", ret==0?"OK":(ret==-2?"exists":"ERR"));

    /* ── Step 2: List directory ─────────────────────────────────────── */
    kprintf("\n--- Directory (%u file(s)) ---\n", fs_num_files());
    fs_list(print_dirent);

    /* ── Step 3: Read files back ────────────────────────────────────── */
    kprintf("\n--- Reading hello.txt ---\n");
    ret = fs_read("hello.txt", read_buf, sizeof(read_buf) - 1);
    if (ret >= 0) kprintf("%s", read_buf);
    else           kprintf("  ERROR: not found\n");

    kprintf("\n--- Reading count.txt ---\n");
    ret = fs_read("count.txt", read_buf, sizeof(read_buf) - 1);
    if (ret >= 0) kprintf("%s", read_buf);
    else           kprintf("  ERROR: not found\n");

    /* ── Step 4: Delete a file ──────────────────────────────────────── */
    kprintf("\n--- Deleting count.txt ---\n");
    ret = fs_delete("count.txt");
    kprintf("  delete: %s\n", ret==0?"OK":"ERR");

    kprintf("\n--- Directory after delete (%u file(s)) ---\n", fs_num_files());
    fs_list(print_dirent);

    /* ── Step 5: Try to read deleted file ──────────────────────────── */
    kprintf("\n--- Reading count.txt (should fail) ---\n");
    ret = fs_read("count.txt", read_buf, sizeof(read_buf) - 1);
    kprintf("  fs_read returned %d  %s\n", ret,
            ret < 0 ? "(correct -- file deleted)" : "(unexpected success)");

    /* ── Done ───────────────────────────────────────────────────────── */
    kprintf("\n--- Module 12 complete.  Filesystem data persists in disk.img ---\n");
    for (;;) __asm__ volatile ("hlt");
}
