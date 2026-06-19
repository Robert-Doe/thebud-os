/*
 * linker.ld — Linker Script for BobOS Kernel
 *
 * A linker script tells the GNU linker (ld) exactly how to arrange the compiled
 * object files into a final binary — what goes where in memory.
 *
 * Why do we need this?
 * -------------------
 * The bootloader loads our kernel at physical address 0x1000 in RAM. The kernel
 * binary must therefore be built assuming it will run at 0x1000 — any absolute
 * address in the code (function pointers, string literals, global variables) must
 * be calculated relative to 0x1000.
 *
 * If we don't specify this, the linker defaults to 0x0 or some other address,
 * and all those absolute addresses will be wrong — the kernel will crash
 * immediately when it tries to access any data.
 *
 * Sections:
 * ---------
 * ELF object files (output of gcc/nasm) contain sections:
 *   .text   — executable code
 *   .data   — initialized global/static variables
 *   .bss    — uninitialized global/static variables (zeroed at startup)
 *   .rodata — read-only data (string literals, const arrays)
 *
 * We merge them all into a flat binary with no ELF headers (output format
 * is 'binary' — specified in the Makefile with --oformat binary).
 */

ENTRY(_start)          /* The entry point symbol — defined in kernel_entry.asm */

SECTIONS {
    /*
     * Start placing everything at 0x1000.
     * The '.' is the "location counter" — it tracks the current address.
     */
    . = 0x1000;

    /*
     * .text section first — kernel_entry.asm's _start must be the very
     * first bytes at 0x1000, because that's where the bootloader jumps.
     * KEEP() prevents the linker from discarding _start if it thinks
     * nothing references it.
     */
    .text : {
        KEEP(*(.text))
    }

    /* Initialized data follows code */
    .data : {
        *(.data)
        *(.rodata)     /* read-only data (string literals) merged here */
    }

    /* BSS: uninitialized data — takes no space in the binary file */
    .bss : {
        *(.bss)
    }
}
