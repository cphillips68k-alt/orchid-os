CC = gcc
CFLAGS = -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin \
         -fno-stack-protector -nostartfiles -nodefaultlibs -O0 -c
AS = nasm
ASFLAGS = -f elf32
LD = ld

all: os-image.bin

entry.o: entry.asm
	$(AS) $(ASFLAGS) -o entry.o entry.asm

kernel.o: kernel.c
	$(CC) $(CFLAGS) -o kernel.o kernel.c

kernel.bin: entry.o kernel.o linker.ld
	$(LD) -m elf_i386 -T linker.ld -o kernel.bin entry.o kernel.o

boot.bin: boot.asm
	$(AS) -f bin -o boot.bin boot.asm

os-image.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > os-image.bin
	dd if=/dev/zero bs=512 count=32 >> os-image.bin

run: os-image.bin
	qemu-system-x86_64 -drive format=raw,file=os-image.bin -monitor stdio

clean:
	rm -f *.o *.bin os-image.bin