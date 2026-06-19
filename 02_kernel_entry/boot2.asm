; =============================================================================
; BobOS Bootloader — Stage 1 (updated)
; New responsibilities:
;   1. Load the kernel from disk into RAM at 0x1000
;   2. Set up the GDT
;   3. Switch CPU to 32-bit Protected Mode
;   4. Jump to the kernel
; =============================================================================

[BITS 16]
[ORG 0x7C00]

; -----------------------------------------------------------------------------
; CONSTANTS
; The kernel will be loaded here in RAM.
; 0x1000 = 4096 — safely above the interrupt vector table and BIOS data area.
; -----------------------------------------------------------------------------
KERNEL_OFFSET equ 0x1000

start:
    ; Zero and stabilize segment registers (same as Component 1)
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Print a status message so we know where we are
    mov si, msg_loading
    call print_string

    ; -------------------------------------------------------------------------
    ; STEP 1: Load the kernel from disk
    ; We use BIOS INT 0x13, Function 0x02: Read Sectors from Drive
    ;
    ; Registers on entry:
    ;   AH = 0x02       (function: read sectors)
    ;   AL = number of sectors to read
    ;   CH = cylinder number (0-based)
    ;   CL = sector number (1-based! Sector 1 is the bootloader, we want 2+)
    ;   DH = head number
    ;   DL = drive number (0x00 = floppy A, 0x80 = first hard disk)
    ;   ES:BX = destination address in memory
    ; -------------------------------------------------------------------------
    mov bx, KERNEL_OFFSET   ; ES:BX = 0x0000:0x1000 = physical address 0x1000
    mov ah, 0x02             ; function: read sectors
    mov al, 15               ; read 15 sectors (7680 bytes — enough for our kernel)
    mov ch, 0                ; cylinder 0
    mov cl, 2                ; start at sector 2 (sector 1 is our bootloader)
    mov dh, 0                ; head 0
    mov dl, 0                ; drive 0 (floppy A)
    int 0x13                 ; call BIOS disk service

    ; Check if the read succeeded. BIOS sets the Carry Flag on error.
    jc disk_error

    mov si, msg_loaded
    call print_string

    ; -------------------------------------------------------------------------
    ; STEP 2: Load the GDT into the CPU's GDTR register
    ; lgdt expects a pointer to a 6-byte structure: 2-byte size + 4-byte address
    ; -------------------------------------------------------------------------
    lgdt [gdt_descriptor]

    ; -------------------------------------------------------------------------
    ; STEP 3: Switch to Protected Mode
    ; Set bit 0 (PE = Protection Enable) of control register CR0.
    ; CR0 cannot be modified directly — must go through a general register.
    ; -------------------------------------------------------------------------
    mov eax, cr0
    or  eax, 0x1             ; set the PE bit
    mov cr0, eax

    ; -------------------------------------------------------------------------
    ; STEP 4: Far jump to flush the CPU instruction pipeline
    ;
    ; Why a far jump? The CPU pre-fetches instructions. After setting CR0,
    ; there may be 16-bit instructions in the pipeline. The far jump forces
    ; the pipeline to flush and reloads CS with our new Protected Mode selector.
    ;
    ; 0x08 is our code segment selector (GDT entry 1, offset 8 bytes from start)
    ; -------------------------------------------------------------------------
    jmp 0x08:init_protected_mode

; Should never reach here — if disk read fails, print error and halt
disk_error:
    mov si, msg_disk_error
    call print_string
    cli
    hlt

; -----------------------------------------------------------------------------
; print_string — same as Component 1 (BIOS teletype output)
; -----------------------------------------------------------------------------
print_string:
    mov ah, 0x0E
    mov bh, 0x00
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

; =============================================================================
; THE GDT — Global Descriptor Table
;
; Each entry is 8 bytes. The format is scrambled for historical reasons.
; We use a macro-style layout with 'db' (define byte) and 'dw' (define word).
;
; Access byte breakdown (8 bits):
;   Bit 7:   Present (1 = this segment is valid)
;   Bits 6-5: Privilege Ring (00 = ring 0 / kernel)
;   Bit 4:   Descriptor type (1 = code/data segment)
;   Bit 3:   Executable (1 = code segment, 0 = data segment)
;   Bit 2:   Direction/Conforming (0 = grows up for data, not conforming for code)
;   Bit 1:   Readable/Writable (1 = readable for code, writable for data)
;   Bit 0:   Accessed (CPU sets this; we initialize to 0)
;
; Flags nibble (4 bits in upper byte):
;   Bit 3: Granularity (1 = limit is in 4KB pages, so 0xFFFFF * 4096 = 4GB)
;   Bit 2: Size (1 = 32-bit protected mode)
;   Bit 1: Long mode (0 = we're not doing 64-bit)
;   Bit 0: Reserved (0)
; =============================================================================
gdt_start:

; --- Entry 0: Null Descriptor (required, must be all zeros) ---
gdt_null:
    dd 0x00000000
    dd 0x00000000

; --- Entry 1: Code Segment ---
; Base = 0x00000000, Limit = 0xFFFFF (with granularity=1, covers 4GB)
; Access = 0x9A = 10011010b (present, ring 0, code, executable, readable)
; Flags  = 0xC  = 1100b     (4KB granularity, 32-bit)
gdt_code:
    dw 0xFFFF       ; Limit bits 0-15
    dw 0x0000       ; Base  bits 0-15
    db 0x00         ; Base  bits 16-23
    db 10011010b    ; Access byte
    db 11001111b    ; Flags (upper 4 bits) + Limit bits 16-19 (lower 4 bits)
    db 0x00         ; Base  bits 24-31

; --- Entry 2: Data Segment ---
; Same as code but access byte marks it as a data (non-executable, writable) segment
; Access = 0x92 = 10010010b (present, ring 0, data, not executable, writable)
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b    ; Access byte — note bit 3 = 0 (data, not code)
    db 11001111b
    db 0x00

gdt_end:

; The descriptor we pass to lgdt: 6 bytes total
gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; size of GDT minus 1 (hardware quirk)
    dd gdt_start                  ; 32-bit address of GDT

; Convenient labels for the segment selectors
; Selector = (GDT entry index) * 8
CODE_SEG equ gdt_code - gdt_start   ; = 0x08
DATA_SEG equ gdt_data - gdt_start   ; = 0x10

; -----------------------------------------------------------------------------
; We are now in 32-bit Protected Mode
; This code runs immediately after the far jump above
; -----------------------------------------------------------------------------
[BITS 32]
init_protected_mode:
    ; Reload all data segment registers with the data segment selector (0x10)
    ; They still hold their Real Mode values — we must update them now
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Set up a new stack in a safe area of memory
    mov ebp, 0x90000
    mov esp, ebp

    ; Jump to our kernel entry point (loaded at KERNEL_OFFSET = 0x1000)
    call KERNEL_OFFSET

    ; If the kernel ever returns, halt
    cli
    hlt

; -----------------------------------------------------------------------------
; Strings
; -----------------------------------------------------------------------------
msg_loading    db 'Loading kernel...', 0x0D, 0x0A, 0
msg_loaded     db 'Kernel loaded. Entering Protected Mode...', 0x0D, 0x0A, 0
msg_disk_error db 'ERROR: Disk read failed!', 0x0D, 0x0A, 0

; Boot signature
times 510 - ($ - $$) db 0
dw 0xAA55
