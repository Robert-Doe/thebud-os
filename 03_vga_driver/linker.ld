/*
 * linker.ld — Linker Script for BobOS Component 3
 *
 * Same as component 2. The bootloader loads the kernel at 0x1000, so every
 * address in the binary must be relative to 0x1000. kernel_entry.asm's _start
 * must be the very first bytes so the bootloader's "call 0x1000" lands there.
 */

ENTRY(_start)

SECTIONS {
    . = 0x1000;

    .text : {
        KEEP(*(.text))
    }

    .data : {
        *(.data)
        *(.rodata)
    }

    .bss : {
        *(.bss)
    }
}
