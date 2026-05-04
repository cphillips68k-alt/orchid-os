CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-ld

CFLAGS = -std=c99 -ffreestanding -O2 -Wall -Wextra -I.
ASFLAGS = --32
LDFLAGS = -T linker.ld -ffreestanding -nostdlib -lgcc

OBJ = boot.o gdt_flush.o kernel.o gdt.o idt.o pic.o paging.o pmm.o vga.o printf.o lib.o

kernel.bin: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -f *.o kernel.bin

iso: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "YourOS" {' >> iso/boot/grub/grub.cfg
	echo '    multiboot2 /boot/kernel.bin' >> iso/boot/grub/grub.cfg
	echo '    boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o your-os.iso iso/

qemu: iso
	qemu-system-i386 -cdrom your-os.iso