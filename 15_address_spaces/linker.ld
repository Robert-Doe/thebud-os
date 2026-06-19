/* linker.ld — Module 15: per-process address spaces.
 *
 * user_prog.o is separated into its own .user_code output section,
 * aligned to 4 KB pages, with linker symbols user_code_start and
 * user_code_end marking its extent.
 *
 * process_create_user() uses these symbols to know which physical pages
 * to mark PAGE_USER in the ring-3 process's page directory.  All other
 * kernel pages stay supervisor-only (U/S = 0).
 */

ENTRY(_start)

SECTIONS {
    . = 0x1000;

    /* Kernel code — all files EXCEPT user_prog.o */
    .text : {
        KEEP(*(EXCLUDE_FILE(*user_prog.o) .text))
    }

    /* ----------------------------------------------------------------
     * User-mode code region.
     * Pages here (and only these pages) get PAGE_USER in ring-3 PDs.
     * Both bounds are 4 KB aligned so the page boundaries are exact.
     * ---------------------------------------------------------------- */
    . = ALIGN(0x1000);
    user_code_start = .;
    .user_code : {
        *user_prog.o(.text)
        *user_prog.o(.rodata)
        *user_prog.o(.data)
        *user_prog.o(.bss)
    }
    . = ALIGN(0x1000);
    user_code_end = .;

    /* Kernel data */
    .data : {
        *(EXCLUDE_FILE(*user_prog.o) .data)
        *(EXCLUDE_FILE(*user_prog.o) .rodata)
    }

    .bss : {
        *(EXCLUDE_FILE(*user_prog.o) .bss)
    }

    kernel_end = .;
}
