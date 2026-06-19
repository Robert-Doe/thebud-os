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

    kernel_end = .;
}
