ASM = nasm
QEMU = qemu-system-x86_64

all: os-image.bin

boot.bin: boot.asm
	$(ASM) -f bin -o boot.bin boot.asm

kernel.bin: kernel.asm
	$(ASM) -f bin -o kernel.bin kernel.asm

os-image.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > os-image.bin
	# Pad to 33 sectors (32 for kernel + 1 for bootloader = 16896 bytes)
	dd if=/dev/zero bs=512 count=32 >> os-image.bin 2>/dev/null

run: os-image.bin
	$(QEMU) -drive format=raw,file=os-image.bin -monitor stdio

clean:
	rm -f *.bin os-image.bin