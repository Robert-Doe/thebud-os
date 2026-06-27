; user_entry.asm — ELF entry point for the standalone user program
;
; The ELF loader jumps to this symbol (_start) via the fake ring-3 iret frame.
; We just call user_main() and then loop forever if it returns.
; user_main() is expected to call sys_exit before returning.

[BITS 32]
[GLOBAL _start]

EXTERN user_main

_start:
    call user_main
.hang:
    jmp  .hang
