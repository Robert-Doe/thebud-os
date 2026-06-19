; kernel_entry.asm — lands at 0x1000, calls kernel_main(), hangs on return.
; Linux/ELF toolchain: no underscore prefix.

[BITS 32]
[EXTERN kernel_main]

global _start

_start:
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
