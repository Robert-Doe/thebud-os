/* linker.ld — place kernel at 0x1000, _start first.
 * kernel_end marks the first byte past the last section so the PMM knows
 * which pages to skip when it marks free memory. */

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

    /* Expose the end of the kernel image as a linker symbol.
     * C code reads this with: extern char kernel_end;
     * and takes its address: (uint32_t)&kernel_end              */
    kernel_end = .;
}
