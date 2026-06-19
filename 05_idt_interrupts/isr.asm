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
;   5. Restores everything and executes 'iret' to resume interrupted code.
; =============================================================================

[BITS 32]

[EXTERN interrupt_handler]    ; C function in isr.c

; -----------------------------------------------------------------------------
; Macros — generate one stub per vector
; -----------------------------------------------------------------------------

; For exceptions that do NOT push an error code: we push a dummy 0 first
; so the stack layout is identical for all vectors.
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0     ; dummy error code (keeps frame layout uniform)
    push dword %1    ; interrupt vector number
    jmp  isr_common_stub
%endmacro

; For exceptions that DO push an error code: the CPU already pushed it,
; so we only push the vector number.
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1    ; interrupt vector number (error code already on stack)
    jmp  isr_common_stub
%endmacro

; For hardware IRQs: the CPU pushes no error code, so we push 0 as dummy.
; The vector = irq_number + 32 (because we remapped the PIC to offset 32).
%macro IRQ_STUB 2
global irq%1
irq%1:
    push dword 0     ; dummy error code
    push dword %2    ; vector = irq + 32
    jmp  isr_common_stub
%endmacro

; -----------------------------------------------------------------------------
; CPU Exception stubs (vectors 0-31)
;
; Which exceptions push an error code is defined by Intel:
;   Error code: 8, 10, 11, 12, 13, 14, 17
;   No error code: everything else
; -----------------------------------------------------------------------------
ISR_NOERRCODE  0    ; #DE  Divide-by-zero
ISR_NOERRCODE  1    ; #DB  Debug
ISR_NOERRCODE  2    ;      NMI (Non-Maskable Interrupt)
ISR_NOERRCODE  3    ; #BP  Breakpoint
ISR_NOERRCODE  4    ; #OF  Overflow
ISR_NOERRCODE  5    ; #BR  Bound Range Exceeded
ISR_NOERRCODE  6    ; #UD  Invalid Opcode
ISR_NOERRCODE  7    ; #NM  Device Not Available (FPU)
ISR_ERRCODE    8    ; #DF  Double Fault           (error code = always 0)
ISR_NOERRCODE  9    ;      Coprocessor Segment Overrun (reserved, legacy)
ISR_ERRCODE   10    ; #TS  Invalid TSS
ISR_ERRCODE   11    ; #NP  Segment Not Present
ISR_ERRCODE   12    ; #SS  Stack-Segment Fault
ISR_ERRCODE   13    ; #GP  General Protection Fault
ISR_ERRCODE   14    ; #PF  Page Fault
ISR_NOERRCODE 15    ;      Reserved
ISR_NOERRCODE 16    ; #MF  x87 FPU Floating-Point Error
ISR_ERRCODE   17    ; #AC  Alignment Check
ISR_NOERRCODE 18    ; #MC  Machine Check
ISR_NOERRCODE 19    ; #XM  SIMD Floating-Point Exception
ISR_NOERRCODE 20    ; #VE  Virtualisation Exception
ISR_NOERRCODE 21    ;      Reserved
ISR_NOERRCODE 22    ;      Reserved
ISR_NOERRCODE 23    ;      Reserved
ISR_NOERRCODE 24    ;      Reserved
ISR_NOERRCODE 25    ;      Reserved
ISR_NOERRCODE 26    ;      Reserved
ISR_NOERRCODE 27    ;      Reserved
ISR_NOERRCODE 28    ;      Reserved
ISR_NOERRCODE 29    ;      Reserved
ISR_ERRCODE   30    ; #SX  Security Exception
ISR_NOERRCODE 31    ;      Reserved

; -----------------------------------------------------------------------------
; Hardware IRQ stubs (vectors 32-47, after PIC remapping)
;
; IRQ_STUB irq_number, vector_number
; -----------------------------------------------------------------------------
IRQ_STUB  0, 32     ; Timer (PIT)
IRQ_STUB  1, 33     ; Keyboard
IRQ_STUB  2, 34     ; Cascade (internal, not a real device)
IRQ_STUB  3, 35     ; COM2 serial port
IRQ_STUB  4, 36     ; COM1 serial port
IRQ_STUB  5, 37     ; LPT2 parallel port
IRQ_STUB  6, 38     ; Floppy disk
IRQ_STUB  7, 39     ; LPT1 / spurious
IRQ_STUB  8, 40     ; Real-Time Clock
IRQ_STUB  9, 41     ; ACPI
IRQ_STUB 10, 42     ; Free / NIC
IRQ_STUB 11, 43     ; Free / USB
IRQ_STUB 12, 44     ; PS/2 mouse
IRQ_STUB 13, 45     ; FPU / coprocessor
IRQ_STUB 14, 46     ; ATA primary
IRQ_STUB 15, 47     ; ATA secondary

; -----------------------------------------------------------------------------
; isr_common_stub — shared entry point for all 48 vectors
;
; Stack at entry (top = lowest address):
;   [ESP+0]  = err_code  (CPU-pushed or dummy 0)
;   [ESP+4]  = int_no    (pushed by our stub)
;   [ESP+8]  = EIP       (pushed by CPU)
;   [ESP+12] = CS        (pushed by CPU)
;   [ESP+16] = EFLAGS    (pushed by CPU)
;
; After saving registers the layout matches struct interrupt_frame in isr.h.
; -----------------------------------------------------------------------------
isr_common_stub:
    pusha           ; push EAX,ECX,EDX,EBX,ESP(original),EBP,ESI,EDI
    push ds
    push es
    push fs
    push gs

    ; Switch all data segment registers to the kernel data segment.
    ; They might hold user-mode selectors if we had user mode (we don't yet),
    ; but we still set them explicitly to ensure a clean kernel environment.
    mov ax, 0x10    ; GDT_SEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; ESP now points to the bottom of struct interrupt_frame.
    ; Pass it as the single argument to interrupt_handler(frame*).
    push esp
    call interrupt_handler
    add  esp, 4     ; remove the argument we just pushed

    ; Restore everything in reverse order
    pop gs
    pop fs
    pop es
    pop ds
    popa

    ; Remove int_no and err_code that were pushed by the stub
    add esp, 8

    ; Return from interrupt.  The CPU pops EIP, CS, EFLAGS (and in privilege
    ; changes also ESP, SS) and resumes the interrupted code.
    iret
