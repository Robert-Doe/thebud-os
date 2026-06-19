/*
 * linker.ld — Kernel linker script (Module 16)
 *
 * user_prog.o is gone.  The kernel image no longer embeds any user code.
 * User programs are standalone ELF binaries stored on disk.img and loaded
 * at runtime by elf_loader.c via SYS_EXEC.
 *
 * The user_code_start / user_code_end symbols from Module 15 are removed;
 * process_create_user() in this module uses elf_load() instead.
 */

ENTRY(_start)

SECTIONS {
    . = 0x1000;

    .text   : { KEEP(*(.text))   }
    .rodata : { *(.rodata)       }
    .data   : { *(.data)         }
    .bss    : { *(.bss)          }

    kernel_end = .;
}
