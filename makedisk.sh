#!/bin/bash
# makedisk.sh - Create a FAT32 test disk image for Orchid OS

DISK="disk.img"
MNT="mnt"
SIZE=64

echo "Creating ${SIZE}MB disk image..."
dd if=/dev/zero of=$DISK bs=1M count=$SIZE status=progress

echo "Formatting as FAT32..."
doas mkfs.fat -F 32 $DISK

echo "Mounting..."
mkdir -p $MNT
doas mount $DISK $MNT

echo "Creating test files..."
doas sh -c 'echo "Hello from Orchid OS!" > '"$MNT"'/HELLO.TXT'
doas sh -c 'echo "This is a test file on Orchid OS." > '"$MNT"'/README.TXT'
doas sh -c 'echo "Orchid kernel is Ring 0." > '"$MNT"'/ORCHID.TXT'
doas mkdir -p "$MNT/TESTDIR"
doas sh -c 'echo "Nested file inside a directory." > '"$MNT"'/TESTDIR/NESTED.TXT'
doas mkdir -p "$MNT/APPS"
doas sh -c 'echo "Applications go here." > '"$MNT"'/APPS/INFO.TXT'

echo "Unmounting..."
doas umount $MNT
rmdir $MNT

echo "Done. $DISK is ready."
echo "Run 'make qemu' to boot with the disk attached."