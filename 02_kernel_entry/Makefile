# =============================================================================
# BobOS Component 2 — Makefile
#
# Build pipeline:
#   1. Assemble boot2.asm       → boot.bin        (512 bytes, the bootloader)
#   2. Assemble kernel_entry.asm → kernel_entry.o  (object file)
#   3. Compile kernel.c          → kernel.o         (object file)
#   4. Link kernel_entry.o + kernel.o → kernel.bin  (flat binary at 0x1000)
#   5. Concatenate boot.bin + kernel.bin → os.img   (bootable disk image)
#   6. Run os.img in QEMU
# =============================================================================

# Tool paths (adjust if yours are different)
NASM  = nasm
GCC   = gcc
LD    = ld
QEMU  = qemu-system-i386

# GCC flags explained:
#   -m32            compile for 32-bit x86
#   -ffreestanding  don't assume a C standard library or OS exists
#   -fno-pie        don't generate position-independent executable code
#   -c              compile only, don't link (we link manually with ld)
#   -O0             no optimization (easier to debug)
GCC_FLAGS = -m32 -ffreestanding -fno-pie -c -O0

# LD flags explained:
#   -m elf_i386         target 32-bit ELF format
#   -T linker.ld        use our custom linker script
#   --oformat binary    output a raw flat binary (no ELF headers)
LD_FLAGS  = -m elf_i386 -T linker.ld --oformat binary

all: os.img

# Step 1: Assemble the bootloader
boot.bin: boot2.asm
	$(NASM) -f bin boot2.asm -o boot.bin

# Step 2: Assemble the kernel entry stub into an ELF object file
kernel_entry.o: kernel_entry.asm
	$(NASM) -f elf32 kernel_entry.asm -o kernel_entry.o

# Step 3: Compile the C kernel into an ELF object file
kernel.o: kernel.c
	$(GCC) $(GCC_FLAGS) kernel.c -o kernel.o

# Step 4: Link both object files into a flat binary at address 0x1000
kernel.bin: kernel_entry.o kernel.o linker.ld
	$(LD) $(LD_FLAGS) -o kernel.bin kernel_entry.o kernel.o

# Step 5: Build the disk image
#   First write the bootloader to sector 1 (first 512 bytes)
#   Then append the kernel immediately after (sector 2 onward)
os.img: boot.bin kernel.bin
	copy /b boot.bin + kernel.bin os.img 2>nul || cat boot.bin kernel.bin > os.img

# Step 6: Run in QEMU
run: os.img
	$(QEMU) -fda os.img

clean:
	del *.bin *.o *.img 2>nul || rm -f *.bin *.o *.img
