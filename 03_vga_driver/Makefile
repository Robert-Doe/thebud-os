# =============================================================================
# BobOS Component 3 — Makefile
#
# Build pipeline:
#   1. Assemble boot.asm          → boot.bin         (512-byte bootloader, NASM on Windows)
#   2. Assemble kernel_entry.asm  → kernel_entry.o   (ELF object, NASM on Windows)
#   3. Compile vga.c              → vga.o            (ELF object, GCC via WSL)
#   4. Compile kernel.c           → kernel.o         (ELF object, GCC via WSL)
#   5. Link all .o files          → kernel.bin       (flat binary at 0x1000, ld via WSL)
#   6. Concatenate boot + kernel  → os.img           (bootable disk image)
#   7. Run in QEMU
#
# WHY WSL for C compilation and linking:
#   The MinGW32 toolchain on Windows produces PE/COFF objects and only supports
#   the i386pe linker emulation. Our kernel needs ELF objects and the elf_i386
#   emulation (so the linker understands our linker.ld and --oformat binary).
#   WSL's gcc/ld provide a native Linux ELF toolchain on the same machine.
#   NASM runs fine on Windows and produces ELF objects, so we use it directly.
# =============================================================================

NASM  = nasm
QEMU  = qemu-system-i386

# WSL-prefixed commands — compile and link inside WSL where elf_i386 is available
WSL      = wsl -d Ubuntu --
WSL_GCC  = $(WSL) gcc
WSL_LD   = $(WSL) ld

WSL_DIR := $(shell wsl wslpath -a "$(CURDIR)")

GCC_FLAGS = -m32 -ffreestanding -fno-pie -c -O0
LD_FLAGS  = -m elf_i386 -T linker.ld --oformat binary

all: os.img

boot.bin: boot.asm
	$(NASM) -f bin boot.asm -o boot.bin

kernel_entry.o: kernel_entry.asm
	$(NASM) -f elf32 kernel_entry.asm -o kernel_entry.o

vga.o: vga.c vga.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/vga.c -o $(WSL_DIR)/vga.o

kernel.o: kernel.c vga.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/kernel.c -o $(WSL_DIR)/kernel.o

# kernel_entry.o must be listed first so _start lands at 0x1000
kernel.bin: kernel_entry.o vga.o kernel.o linker.ld
	$(WSL_LD) $(LD_FLAGS) -o $(WSL_DIR)/kernel.bin \
	    $(WSL_DIR)/kernel_entry.o $(WSL_DIR)/vga.o $(WSL_DIR)/kernel.o

os.img: boot.bin kernel.bin
	copy /b boot.bin + kernel.bin os.img 2>nul || cat boot.bin kernel.bin > os.img

run: os.img
	$(QEMU) -fda os.img

clean:
	del *.bin *.o *.img *.elf 2>nul || rm -f *.bin *.o *.img *.elf
