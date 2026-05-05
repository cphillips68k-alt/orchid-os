#!/bin/bash
# makedisk.sh - Create a FAT32 test disk image for Orchid OS

DISK="disk.img"
MNT="mnt"
SIZE=64

echo "Creating ${SIZE}MB disk image..."
dd if=/dev/zero of=$DISK bs=1M count=$SIZE status=progress

echo "Formatting as FAT32..."
mkfs.fat -F 32 $DISK

echo "Mounting..."
mkdir -p $MNT
sudo mount $DISK $MNT

echo "Creating test files..."
sudo sh -c 'echo "Hello from Orchid OS!" > '"$MNT"'/HELLO.TXT'
sudo sh -c 'echo "This is a test file on Orchid OS." > '"$MNT"'/README.TXT'
sudo sh -c 'echo "Orchid kernel is Ring 0." > '"$MNT"'/ORCHID.TXT'
sudo mkdir -p "$MNT/TESTDIR"
sudo sh -c 'echo "Nested file inside a directory." > '"$MNT"'/TESTDIR/NESTED.TXT'
sudo mkdir -p "$MNT/APPS"
sudo sh -c 'echo "Applications go here." > '"$MNT"'/APPS/INFO.TXT'

echo "Unmounting..."
sudo umount $MNT
rmdir $MNT

echo "Done. $DISK is ready."
echo "Run 'make qemu' to boot with the disk attached."