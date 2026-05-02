CC = gcc
CFLAGS = -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin \
         -fno-stack-protector -nostartfiles -nodefaultlibs -O0 -c
AS = nasm
ASFLAGS = -f elf32
LD = ld
OBJCOPY = objcopy

# Object files in their directories
KERNEL_OBJS = asm/entry.o kernel/kernel.o
USER_OBJS  = user/vga.o user/keyboard.o user/ramdisk.o user/vfs.o \
             user/shell.o user/runtime.o
ALL_OBJS = $(KERNEL_OBJS) $(USER_OBJS)

all: os-image.bin

# Assembly files
asm/entry.o: asm/entry.asm
	$(AS) $(ASFLAGS) -o asm/entry.o asm/entry.asm

# Kernel C files
kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -o $@ $<

# User C files
user/%.o: user/%.c
	$(CC) $(CFLAGS) -o $@ $<

# Link
kernel.elf: $(ALL_OBJS) linker.ld
	$(LD) -m elf_i386 -T linker.ld -o kernel.elf $(ALL_OBJS)

# Flat binary
kernel.bin: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel.bin

# Bootloader (flat binary, stays at root)
boot.bin: asm/boot.asm
	$(AS) -f bin -o boot.bin asm/boot.asm

# Disk image
os-image.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > os-image.bin
	dd if=/dev/zero bs=512 count=32 >> os-image.bin 2>/dev/null

run: os-image.bin
	qemu-system-x86_64 -drive format=raw,file=os-image.bin -monitor stdio

clean:
	rm -f asm/*.o kernel/*.o user/*.o *.elf *.bin os-image.bin