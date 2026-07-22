; =============================================================================
; BobOS Bootloader — Stage 1
; Loaded by BIOS at 0x7C00. Runs in 16-bit Real Mode.
; =============================================================================

[BITS 16]           ; Tell NASM we're writing 16-bit code
[ORG 0x7C00]        ; Tell NASM our code will sit at address 0x7C00 in RAM

; -----------------------------------------------------------------------------
; Entry point — first instruction the CPU runs after BIOS hands off to us
; -----------------------------------------------------------------------------
start:
    ; Zero out segment registers.
    ; In Real Mode, memory address = Segment*16 + Offset.
    ; We set them all to 0 so our addresses are straightforward.
    xor ax, ax
    mov ds, ax      ; Data Segment = 0
    mov es, ax      ; Extra Segment = 0
    mov ss, ax      ; Stack Segment = 0

    ; Set up the stack just below our bootloader.
    ; The stack grows downward, so 0x7C00 is a safe top-of-stack.
    mov sp, 0x7C00

    ; Print our boot message
    mov si, msg_boot    ; SI = pointer to our string
    call print_string

    ; Hang the CPU — we have nothing else to do yet.
    ; Later this is where we'll load the kernel from disk.
    cli             ; Clear interrupts (no more hardware interrupts)
    hlt             ; Halt the CPU

; -----------------------------------------------------------------------------
; print_string — prints a null-terminated string using BIOS INT 0x10
; Input: SI = address of string
; BIOS function AH=0x0E is "Teletype output" — prints one character at a time
; -----------------------------------------------------------------------------
print_string:
    mov ah, 0x0E        ; BIOS video function: teletype output
    mov bh, 0x00        ; Page number 0
    mov bl, 0x07        ; Text color: light grey on black (only matters in some modes)
.loop:
    lodsb               ; Load byte at [SI] into AL, then SI++
    test al, al         ; Is AL == 0? (null terminator?)
    jz .done            ; Yes — we're done
    int 0x10            ; Call BIOS: print character in AL
    jmp .loop
.done:
    ret

; -----------------------------------------------------------------------------
; Data
; -----------------------------------------------------------------------------
msg_boot db 'BobOS Booting...', 0x0D, 0x0A, 0   ; \r\n then null terminator

; -----------------------------------------------------------------------------
; Boot Signature — BIOS requires the last 2 bytes of the 512-byte sector
; to be 0x55, 0xAA. We pad everything between here and byte 510 with zeros.
; -----------------------------------------------------------------------------
times 510 - ($ - $$) db 0   ; Pad with zeros up to byte 510
dw 0xAA55                    ; Boot signature (little-endian: stored as 55 AA)
