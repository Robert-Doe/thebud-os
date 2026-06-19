; =============================================================================
; kernel_entry.asm — The bridge between the bootloader and our C kernel
;
; When the bootloader jumps to 0x1000, it lands HERE — not directly in C.
; Why? Because C assumes certain things are set up (a valid stack, segment
; registers pointing somewhere sane). The bootloader already did that, but
; we put this stub here as the guaranteed first thing in our kernel binary
; so the linker always places it at 0x1000 (via linker.ld).
;
; After this stub runs, all C code in kernel.c can execute normally.
; =============================================================================

[BITS 32]               ; We're already in 32-bit Protected Mode
[EXTERN _kernel_main]   ; MinGW prefixes C symbols with _ (so kernel_main → _kernel_main)

global _start           ; Make _start visible to the linker

_start:
    call _kernel_main   ; Call our C kernel function
    ; If kernel_main returns (it shouldn't), hang forever
    cli
.hang:
    hlt
    jmp .hang
