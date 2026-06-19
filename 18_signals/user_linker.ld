/*
 * user_linker.ld — linker script for the standalone user ELF binary
 *
 * The ELF is loaded at 0x400000 (4 MB) — safely above the kernel's
 * 4 MB identity map, so user segments never alias kernel pages.
 *
 * The kernel's linker.ld no longer needs to know about user_prog.o.
 */

ENTRY(_start)

SECTIONS {
    . = 0x400000;

    .text   : { *(.text)   }
    .rodata : { *(.rodata) }
    .data   : { *(.data)   }
    .bss    : { *(.bss) *(COMMON) }
}
