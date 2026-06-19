; =============================================================================
; isr.asm — Assembly stubs for all 48 interrupt vectors (0-31 + 32-47)
;
; Why assembly?  The C compiler has no concept of "I was interrupted in the
; middle of something — save every register before doing anything."  C function
; prologues only save the registers the ABI says a callee must preserve.  But
; an interrupt can fire between ANY two instructions, so ALL registers must be
; saved — including the ones C normally treats as caller-saved (EAX, ECX, EDX).
;
; Each stub:
;   1. Pushes a dummy error code (0) if the CPU does not push one itself.
;   2. Pushes the interrupt vector number.
;   3. Jumps to isr_common_stub.
;
; isr_common_stub:
;   1. Saves ALL general-purpose registers with 'pusha'.
;   2. Saves all segment registers.
;   3. Switches DS/ES/FS/GS to the kernel data segment (0x10).
;   4. Passes ESP (pointer to struct interrupt_frame) to interrupt_handler().
;   5. Uses the RETURN VALUE (EAX) as the new ESP — normally unchanged, but
;      the scheduler returns a different process's saved ESP here to switch.
;   6. Restores registers from the (possibly new) ESP and executes 'iret'.
; =============================================================================

[BITS 32]

[EXTERN interrupt_handler]

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp  isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1
    jmp  isr_common_stub
%endmacro

%macro IRQ_STUB 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp  isr_common_stub
%endmacro

ISR_NOERRCODE  0
ISR_NOERRCODE  1
ISR_NOERRCODE  2
ISR_NOERRCODE  3
ISR_NOERRCODE  4
ISR_NOERRCODE  5
ISR_NOERRCODE  6
ISR_NOERRCODE  7
ISR_ERRCODE    8
ISR_NOERRCODE  9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

; Vector 128 (0x80) — software syscall gate, invoked with `int 0x80`.
; Uses the same ISR_NOERRCODE macro as exception stubs.
global isr128
isr128:
    push dword 0
    push dword 128
    jmp  isr_common_stub

ISR_NOERRCODE  0
IRQ_STUB  1, 33
IRQ_STUB  2, 34
IRQ_STUB  3, 35
IRQ_STUB  4, 36
IRQ_STUB  5, 37
IRQ_STUB  6, 38
IRQ_STUB  7, 39
IRQ_STUB  8, 40
IRQ_STUB  9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

isr_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call interrupt_handler   ; EAX = ESP to restore (may be next process's stack)
    mov  esp, eax            ; switch stacks — the entire context switch is here

    pop gs
    pop fs
    pop es
    pop ds
    popa

    add esp, 8
    iret
