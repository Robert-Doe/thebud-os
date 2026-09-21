# BobOS Component 4 — Makefile
# Build: NASM on Windows, GCC + ld via WSL (ELF toolchain required)

NASM = nasm
QEMU = qemu-system-i386

WSL     = wsl -d Ubuntu --
WSL_GCC = $(WSL) gcc
WSL_LD  = $(WSL) ld
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

gdt.o: gdt.c gdt.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/gdt.c -o $(WSL_DIR)/gdt.o

kernel.o: kernel.c vga.h gdt.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/kernel.c -o $(WSL_DIR)/kernel.o

kernel.bin: kernel_entry.o vga.o gdt.o kernel.o linker.ld
	$(WSL_LD) $(LD_FLAGS) -o $(WSL_DIR)/kernel.bin \
	    $(WSL_DIR)/kernel_entry.o $(WSL_DIR)/vga.o \
	    $(WSL_DIR)/gdt.o $(WSL_DIR)/kernel.o

os.img: boot.bin kernel.bin
	copy /b boot.bin + kernel.bin os.img 2>nul || cat boot.bin kernel.bin > os.img

run: os.img
	$(QEMU) -fda os.img

clean:
	del *.bin *.o *.img 2>nul || rm -f *.bin *.o *.img
