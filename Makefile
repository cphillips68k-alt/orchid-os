ASM = nasm
QEMU = qemu-system-x86_64

all: os-image.bin

boot.bin: boot.asm
	$(ASM) -f bin -o boot.bin boot.asm

kernel.bin: kernel.asm
	$(ASM) -f bin -o kernel.bin kernel.asm

os-image.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > os-image.bin
	# Pad to at least 33 sectors (bootloader = 1, kernel = 32)
	dd if=/dev/zero bs=512 count=33 >> os-image.bin 2>/dev/null || true
	truncate -s 16896 os-image.bin 2>/dev/null || dd if=/dev/zero bs=1 count=$$((16896 - $$(stat -c%s os-image.bin))) >> os-image.bin

run: os-image.bin
	$(QEMU) -drive format=raw,file=os-image.bin -monitor stdio

clean:
	rm -f *.bin