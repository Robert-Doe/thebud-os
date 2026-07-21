; boot.asm — BobOS bootloader (identical to 03_vga_driver/boot.asm)
; Self-contained copy so this module builds and runs independently.
; Loads 20 sectors from disk, switches to Protected Mode, jumps to 0x1000.

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

    mov bx, KERNEL_OFFSET
    mov ah, 0x02
    mov al, 25              ; 25 sectors — room for vga.o + gdt.o + kernel.o
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0
    int 0x13
    jc disk_error

    mov si, msg_loaded
    call print_string

    lgdt [gdt_descriptor]

    mov eax, cr0
    or  eax, 0x1
    mov cr0, eax

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

gdt_start:
gdt_null:
    dd 0x00000000
    dd 0x00000000
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
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

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

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

msg_loading    db 'Loading kernel...', 0x0D, 0x0A, 0
msg_loaded     db 'Kernel loaded. Entering Protected Mode...', 0x0D, 0x0A, 0
msg_disk_error db 'ERROR: Disk read failed!', 0x0D, 0x0A, 0

times 510 - ($ - $$) db 0
dw 0xAA55
