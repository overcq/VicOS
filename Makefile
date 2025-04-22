CXX = g++
CC = gcc
AS = nasm
LD = ld

# Add the custom include path before anything else
CXXFLAGS = -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -I./src -I./src/fatfs
CFLAGS = -m32 -ffreestanding -fno-pic -fno-pie -O0 -Wall -Wextra -fno-stack-protector -I./src -I./src/fatfs
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

# File paths
KERNEL_SRC = src/kernel.cpp
VSHELL_SRC = src/vshellhandler.cpp
FS_SRC = src/filesystem.cpp
NANO_SRC = src/vnano.cpp
DISK_DRIVER_SRC = src/disk_driver.cpp
PARTITION_SRC = src/partition_manager.cpp
FAT32_SRC = src/fat32_modified.cpp
FATFS_INTEGRATION_SRC = src/fatfs_integration.cpp
INSTALLER_SRC = src/real_installer.cpp
KEYBOARD_SRC = src/keyboard.cpp
BOOT_SRC = src/boot.s
STRING_UTILS_SRC = src/string_utils.c

# Build directory
BUILD_DIR = build
ISO_DIR = iso
GRUB_DIR = grub
KERNEL_ELF = $(BUILD_DIR)/kernel.elf

# Targets
all: $(KERNEL_ELF)

$(BUILD_DIR)/boot.o: $(BOOT_SRC)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kernel.o: $(KERNEL_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/vshell.o: $(VSHELL_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/filesystem.o: $(FS_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/vnano.o: $(NANO_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/disk_driver.o: $(DISK_DRIVER_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/partition_manager.o: $(PARTITION_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/fat32.o: $(FAT32_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/fatfs_integration.o: $(FATFS_INTEGRATION_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/real_installer.o: $(INSTALLER_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: $(KEYBOARD_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/string_utils.o: $(STRING_UTILS_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/vshell.o $(BUILD_DIR)/filesystem.o $(BUILD_DIR)/vnano.o $(BUILD_DIR)/disk_driver.o $(BUILD_DIR)/partition_manager.o $(BUILD_DIR)/fat32.o $(BUILD_DIR)/fatfs_integration.o $(BUILD_DIR)/real_installer.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/string_utils.o linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/vshell.o $(BUILD_DIR)/filesystem.o $(BUILD_DIR)/vnano.o $(BUILD_DIR)/disk_driver.o $(BUILD_DIR)/partition_manager.o $(BUILD_DIR)/fat32.o $(BUILD_DIR)/fatfs_integration.o $(BUILD_DIR)/real_installer.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/string_utils.o

iso: all
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf
	cp $(GRUB_DIR)/grub.cfg $(ISO_DIR)/boot/grub/
	grub-mkrescue -o $(BUILD_DIR)/vic-os.iso $(ISO_DIR)

run: all
	qemu-system-i386 -kernel $(KERNEL_ELF)

run-iso: iso
	qemu-system-i386 -cdrom $(BUILD_DIR)/vic-os.iso

clean:
	rm -rf $(BUILD_DIR)/*.o $(KERNEL_ELF) $(BUILD_DIR)/vic-os.iso
