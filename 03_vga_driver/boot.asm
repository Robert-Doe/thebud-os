; =============================================================================
; BobOS Bootloader — Component 3
;
; Identical to the bootloader from 02_kernel_entry. We keep a self-contained
; copy in each component directory so every module can be built and run in
; QEMU independently, without depending on a previous directory.
;
; Responsibilities:
;   1. Load the kernel binary from disk into RAM at 0x1000
;   2. Set up the Global Descriptor Table (GDT)
;   3. Switch the CPU from 16-bit Real Mode to 32-bit Protected Mode
;   4. Jump to the kernel
; =============================================================================

[BITS 16]
[ORG 0x7C00]

KERNEL_OFFSET equ 0x1000

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov si, msg_loading
    call print_string

    ; ---- Load kernel from disk (BIOS INT 0x13, function 0x02) ----
    mov bx, KERNEL_OFFSET
    mov ah, 0x02
    mov al, 20              ; 20 sectors = 10240 bytes — room for vga.o + kernel.o
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0
    int 0x13
    jc disk_error

    mov si, msg_loaded
    call print_string

    ; ---- Load GDT ----
    lgdt [gdt_descriptor]

    ; ---- Enable Protected Mode: set bit 0 of CR0 ----
    mov eax, cr0
    or  eax, 0x1
    mov cr0, eax

    ; ---- Far jump to flush the pipeline and reload CS ----
    jmp 0x08:init_protected_mode

disk_error:
    mov si, msg_disk_error
    call print_string
    cli
    hlt

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
; GDT — Global Descriptor Table (flat memory model, 0 → 4GB)
; =============================================================================
gdt_start:

gdt_null:
    dd 0x00000000
    dd 0x00000000

; Code segment: base=0, limit=4GB, ring 0, executable, readable
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

; Data segment: base=0, limit=4GB, ring 0, not executable, writable
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start   ; = 0x08
DATA_SEG equ gdt_data - gdt_start   ; = 0x10

; =============================================================================
; 32-bit Protected Mode entry point
; =============================================================================
[BITS 32]
init_protected_mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000
    mov esp, ebp

    call KERNEL_OFFSET

    cli
    hlt

; -----------------------------------------------------------------------------
msg_loading    db 'Loading kernel...', 0x0D, 0x0A, 0
msg_loaded     db 'Kernel loaded. Entering Protected Mode...', 0x0D, 0x0A, 0
msg_disk_error db 'ERROR: Disk read failed!', 0x0D, 0x0A, 0

times 510 - ($ - $$) db 0
dw 0xAA55
