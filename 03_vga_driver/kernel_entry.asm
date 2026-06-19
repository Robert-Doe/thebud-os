; =============================================================================
; kernel_entry.asm — The bridge between the bootloader and our C kernel
;
; Identical in purpose to the one in 02_kernel_entry. Placed first by the
; linker script so it lands at exactly 0x1000 — the address the bootloader
; jumps to. It calls kernel_main() and hangs if it ever returns.
;
; NOTE: We build through WSL (Linux GCC + GNU ld with elf_i386 emulation).
; Linux ELF toolchains do NOT add the _ prefix to C symbols — the function
; is 'kernel_main', not '_kernel_main'. Component 2 used MinGW which does
; add the prefix; this component switched to WSL to get a real ELF linker.
; =============================================================================

[BITS 32]
[EXTERN kernel_main]    ; Linux/ELF toolchain: no underscore prefix on C symbols

global _start

_start:
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
