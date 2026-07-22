all: boot.img

boot.img: boot.asm
	nasm -f bin boot.asm -o boot.img

run: boot.img
	qemu-system-i386 -fda boot.img

clean:
	del boot.img 2>nul || rm -f boot.img
