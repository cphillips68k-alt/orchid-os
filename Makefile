CC = clang
LD = ld.lld
AS = clang

TARGET = i686-unknown-unknown

CFLAGS = -target $(TARGET) -std=c99 -ffreestanding -O2 -Wall -Wextra -I. \
         -mno-sse -mno-mmx -mno-sse2 -mno-80387 \
         -fno-stack-protector -fno-builtin

ASFLAGS = -target $(TARGET) -c

LDFLAGS = -T linker.ld -nostdlib

OBJ = boot.o gdt_flush.o idt_flush.o paging.o idt.o \
      kernel.o gdt.o pic.o paging_c.o pmm.o multiboot.o \
      vga.o printf.o lib.o

kernel.bin: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -f *.o kernel.bin

iso: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "Orchid OS" {' >> iso/boot/grub/grub.cfg
	echo '    multiboot2 /boot/kernel.bin' >> iso/boot/grub/grub.cfg
	echo '    boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o orchid.iso iso/

qemu: iso
	qemu-system-i386 -cdrom orchid.iso