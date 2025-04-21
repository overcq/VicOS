#!/bin/bash

# VicOS ISO Builder Script
# This script builds VicOS and packages it as a bootable ISO

# Display banner
echo "====================================="
echo "      VicOS ISO Builder Script       "
echo "====================================="

# Create necessary directories if they don't exist
mkdir -p build
mkdir -p iso/boot/grub

# Clean previous build files
echo "[1/10] Cleaning previous build files..."
rm -rf build/*.o build/kernel.elf build/vic-os.iso
rm -rf iso/boot/kernel.elf

# Compile boot.s
echo "[2/10] Compiling boot.s..."
nasm -f elf32 src/boot.s -o build/boot.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile boot.s"
    exit 1
fi

# Compile kernel.cpp
echo "[3/10] Compiling kernel.cpp..."
g++ -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -c src/kernel.cpp -o build/kernel.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile kernel.cpp"
    exit 1
fi

# Compile vshellhandler.cpp
echo "[4/10] Compiling vshellhandler.cpp..."
g++ -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -c src/vshellhandler.cpp -o build/vshell.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile vshellhandler.cpp"
    exit 1
fi

# Compile filesystem.cpp
echo "[5/10] Compiling filesystem.cpp..."
g++ -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -c src/filesystem.cpp -o build/filesystem.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile filesystem.cpp"
    exit 1
fi

# Compile vnano.cpp
echo "[6/10] Compiling vnano.cpp..."
g++ -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -c src/vnano.cpp -o build/vnano.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile vnano.cpp"
    exit 1
fi

# Compile disk_driver.cpp
echo "[7/10] Compiling disk_driver.cpp..."
g++ -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -c src/disk_driver.cpp -o build/disk_driver.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile disk_driver.cpp"
    exit 1
fi

# Compile partition_manager.cpp
echo "[8/10] Compiling partition_manager.cpp..."
g++ -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -c src/partition_manager.cpp -o build/partition_manager.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile partition_manager.cpp"
    exit 1
fi

# Compile fat32.cpp
echo "[9/10] Compiling fat32.cpp..."
g++ -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -c src/fat32.cpp -o build/fat32.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile fat32.cpp"
    exit 1
fi

# Compile real_installer.cpp
echo "[10/10] Compiling real_installer.cpp..."
g++ -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -c src/real_installer.cpp -o build/real_installer.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile real_installer.cpp"
    exit 1
fi

# Link the kernel
echo "[11/11] Linking kernel..."
ld -m elf_i386 -T linker.ld -o build/kernel.elf build/boot.o build/kernel.o build/vshell.o build/filesystem.o build/vnano.o build/disk_driver.o build/partition_manager.o build/fat32.o build/real_installer.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to link kernel"
    exit 1
fi

# Create the ISO
echo "Creating bootable ISO..."
mkdir -p iso/boot/grub
cp build/kernel.elf iso/boot/kernel.elf
cp grub/grub.cfg iso/boot/grub/

grub-mkrescue -o build/vic-os.iso iso
if [ $? -ne 0 ]; then
    echo "Error: Failed to create ISO"
    exit 1
fi

# Success message
echo "====================================="
echo "Build completed successfully!"
echo "ISO file created at: build/vic-os.iso"
echo "====================================="
echo ""
echo "To run the OS in QEMU:"
echo "  qemu-system-i386 -cdrom build/vic-os.iso"
echo ""
echo "To run the kernel directly in QEMU:"
echo "  qemu-system-i386 -kernel build/kernel.elf"
echo "====================================="
