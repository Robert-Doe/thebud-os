# BobOS Module 21 -- Copy-on-Write Fork

NASM = nasm
QEMU = qemu-system-i386

WSL     = wsl -d Ubuntu --
WSL_GCC = $(WSL) gcc
WSL_LD  = $(WSL) ld
WSL_DIR := $(shell wsl wslpath -a "$(CURDIR)")

GCC_FLAGS      = -m32 -ffreestanding -fno-pie -c -O0
GCC_USER_FLAGS = -m32 -ffreestanding -fno-pie -c -O0 -nostdlib
LD_FLAGS       = -m elf_i386 -T linker.ld --oformat binary
LD_USER_FLAGS  = -m elf_i386 -T user_linker.ld

all: os.img

boot.bin: boot.asm
	$(NASM) -f bin boot.asm -o boot.bin

kernel_entry.o: kernel_entry.asm
	$(NASM) -f elf32 kernel_entry.asm -o kernel_entry.o

isr_stubs.o: isr.asm
	$(NASM) -f elf32 isr.asm -o isr_stubs.o

# ── User ELF (signal demo) ────────────────────────────────────────────

user_entry.o: user_entry.asm
	$(NASM) -f elf32 user_entry.asm -o user_entry.o

user_prog_obj.o: user_prog.c
	$(WSL_GCC) $(GCC_USER_FLAGS) $(WSL_DIR)/user_prog.c -o $(WSL_DIR)/user_prog_obj.o

user_prog.elf: user_entry.o user_prog_obj.o user_linker.ld
	$(WSL_LD) $(LD_USER_FLAGS) -o $(WSL_DIR)/user_prog.elf \
	    $(WSL_DIR)/user_entry.o \
	    $(WSL_DIR)/user_prog_obj.o

user_elf_blob.o: user_prog.elf
	$(WSL) objcopy -I binary -O elf32-i386 -B i386 \
	    $(WSL_DIR)/user_prog.elf $(WSL_DIR)/user_elf_blob.o

# ── Kernel C objects ──────────────────────────────────────────────────

vga.o: vga.c vga.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/vga.c -o $(WSL_DIR)/vga.o

gdt.o: gdt.c gdt.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/gdt.c -o $(WSL_DIR)/gdt.o

pic.o: pic.c pic.h io.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/pic.c -o $(WSL_DIR)/pic.o

idt.o: idt.c idt.h isr.h gdt.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/idt.c -o $(WSL_DIR)/idt.o

isr.o: isr.c isr.h pic.h vga.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/isr.c -o $(WSL_DIR)/isr.o

pmm.o: pmm.c pmm.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/pmm.c -o $(WSL_DIR)/pmm.o

paging.o: paging.c paging.h pmm.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/paging.c -o $(WSL_DIR)/paging.o

heap.o: heap.c heap.h pmm.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/heap.c -o $(WSL_DIR)/heap.o

keyboard.o: keyboard.c keyboard.h isr.h io.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/keyboard.c -o $(WSL_DIR)/keyboard.o

process.o: process.c process.h heap.h isr.h tss.h paging.h vga.h vfs.h mmap.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/process.c -o $(WSL_DIR)/process.o

syscall.o: syscall.c syscall.h isr.h vga.h process.h elf_loader.h paging.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/syscall.c -o $(WSL_DIR)/syscall.o

disk.o: disk.c disk.h io.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/disk.c -o $(WSL_DIR)/disk.o

fs.o: fs.c fs.h disk.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/fs.c -o $(WSL_DIR)/fs.o

tss.o: tss.c tss.h gdt.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/tss.c -o $(WSL_DIR)/tss.o

elf_loader.o: elf_loader.c elf_loader.h elf.h fs.h pmm.h paging.h vga.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/elf_loader.c -o $(WSL_DIR)/elf_loader.o

vfs.o: vfs.c vfs.h fs.h vga.h process.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/vfs.c -o $(WSL_DIR)/vfs.o

pipe.o: pipe.c pipe.h vfs.h process.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/pipe.c -o $(WSL_DIR)/pipe.o

kernel.o: kernel.c vga.h gdt.h pic.h idt.h isr.h pmm.h paging.h heap.h \
          keyboard.h process.h syscall.h disk.h fs.h tss.h vfs.h pipe.h
	$(WSL_GCC) $(GCC_FLAGS) $(WSL_DIR)/kernel.c -o $(WSL_DIR)/kernel.o

# ── Link kernel ───────────────────────────────────────────────────────

kernel.bin: kernel_entry.o isr_stubs.o user_elf_blob.o \
            vga.o gdt.o pic.o idt.o isr.o pmm.o paging.o heap.o \
            keyboard.o process.o syscall.o disk.o fs.o tss.o \
            elf_loader.o vfs.o pipe.o kernel.o linker.ld
	$(WSL_LD) $(LD_FLAGS) -o $(WSL_DIR)/kernel.bin \
	    $(WSL_DIR)/kernel_entry.o  \
	    $(WSL_DIR)/isr_stubs.o    \
	    $(WSL_DIR)/user_elf_blob.o \
	    $(WSL_DIR)/vga.o          \
	    $(WSL_DIR)/gdt.o          \
	    $(WSL_DIR)/pic.o          \
	    $(WSL_DIR)/idt.o          \
	    $(WSL_DIR)/isr.o          \
	    $(WSL_DIR)/pmm.o          \
	    $(WSL_DIR)/paging.o       \
	    $(WSL_DIR)/heap.o         \
	    $(WSL_DIR)/keyboard.o     \
	    $(WSL_DIR)/process.o      \
	    $(WSL_DIR)/syscall.o      \
	    $(WSL_DIR)/disk.o         \
	    $(WSL_DIR)/fs.o           \
	    $(WSL_DIR)/tss.o          \
	    $(WSL_DIR)/elf_loader.o   \
	    $(WSL_DIR)/vfs.o          \
	    $(WSL_DIR)/pipe.o         \
	    $(WSL_DIR)/kernel.o

os.img: boot.bin kernel.bin
	copy /b boot.bin + kernel.bin os.img 2>nul || cat boot.bin kernel.bin > os.img

disk.img:
	$(WSL) dd if=/dev/zero of=$(WSL_DIR)/disk.img bs=512 count=4096 2>/dev/null

run: os.img disk.img
	$(QEMU) -fda os.img -hda disk.img

clean:
	del *.bin *.o *.img *.elf 2>nul || rm -f *.bin *.o *.img *.elf
