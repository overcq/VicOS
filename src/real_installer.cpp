#include <stdint.h>
#include <stddef.h>

// Forward declarations
void kprint(const char* str);
void kputchar(char c);
extern "C" void clear_screen();

// Forward declarations for disk functions
int disk_initialize();
void disk_detect();
int disk_get_drive_info(int drive_index, bool* exists, char* model, uint32_t* size_mb);
bool set_active_drive(int drive_index);
int create_vicos_partition();
int create_fat32_filesystem();
int create_directory(const char* dirname);
int create_file_with_content(const char* filename, const void* data, uint32_t size);

// Forward declaration for keyboard input
extern "C" char keyboard_read_char();

// Storage device structure
#define MAX_STORAGE_DEVICES 8

struct StorageDevice {
    bool detected;
    char name[8];      // e.g., "hda", "sdb"
    char model[41];    // Model string from device
    uint32_t size_mb;  // Size in MB
    int drive_index;   // Index in ATA array
};

StorageDevice storage_devices[MAX_STORAGE_DEVICES];
int num_storage_devices = 0;

// String utilities
void ri_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Simple conversion of number to string
void int_to_str(uint32_t num, char* str) {
    // Special case for zero
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    // Convert digits in reverse order
    int i = 0;
    while (num > 0) {
        str[i++] = '0' + (num % 10);
        num /= 10;
    }

    // Add null terminator
    str[i] = '\0';

    // Reverse the string
    for (int j = 0; j < i / 2; j++) {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }
}

// Scan for storage devices - this uses actual ATA detection
void scan_storage_devices() {
    kprint("Scanning for storage devices...\n");

    // Reset the device list
    num_storage_devices = 0;

    // Initialize disk subsystem and detect drives
    if (disk_initialize() <= 0) {
        kprint("No storage devices detected or disk initialization failed.\n");
        return;
    }

    // Get information about all detected ATA drives
    for (int i = 0; i < 4; i++) {  // Up to 4 ATA drives
        bool exists;
        char model[41];
        uint32_t size_mb;

        if (disk_get_drive_info(i, &exists, model, &size_mb) == 0 && exists) {
            // This drive exists, add it to our list
            StorageDevice* dev = &storage_devices[num_storage_devices++];
            dev->detected = true;
            dev->drive_index = i;
            dev->size_mb = size_mb;

            // Copy model string
            ri_strcpy(dev->model, model);

            // Create a device name based on index
            // Primary master = hda, primary slave = hdb, etc.
            dev->name[0] = 'h';
            dev->name[1] = 'd';
            dev->name[2] = 'a' + i;
            dev->name[3] = '\0';
        }
    }

    // Show results
    if (num_storage_devices > 0) {
        kprint("Found ");
        char num_str[16];
        int_to_str(num_storage_devices, num_str);
        kprint(num_str);
        kprint(" storage devices\n");
    } else {
        kprint("No storage devices detected\n");
    }
}

// Display available storage devices
void display_storage_devices() {
    if (num_storage_devices == 0) {
        kprint("No storage devices detected.\n");
        return;
    }

    kprint("\nStorage Devices Available for Installation:\n");
    kprint("-------------------------------------\n\n");

    for (int i = 0; i < num_storage_devices; i++) {
        StorageDevice* dev = &storage_devices[i];

        char num_str[16];
        int_to_str(i + 1, num_str);

        kprint(num_str);
        kprint(": /dev/");
        kprint(dev->name);
        kprint(" [");
        kprint(dev->model);
        kprint("]\n   ");

        // Display size
        int_to_str(dev->size_mb, num_str);
        kprint(num_str);
        kprint(" MB (");

        // Calculate in GB for readability
        if (dev->size_mb >= 1024) {
            int_to_str(dev->size_mb / 1024, num_str);
            kprint(num_str);
            kprint(" GB");
        } else {
            kprint("< 1 GB");
        }
        kprint(")\n\n");
    }
}

// Get character input with echo
char get_char_with_echo() {
    char c = keyboard_read_char();
    kputchar(c); // Echo character
    return c;
}

// Get a number from user input
int get_number_input() {
    char c = get_char_with_echo();

    // Convert ASCII to integer
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    return 0; // Default/invalid input
}

// Get a string from user input
void get_string_input(char* buffer, int max_len) {
    int pos = 0;

    while (pos < max_len - 1) {
        char c = keyboard_read_char();

        if (c == '\n' || c == '\r') {
            kputchar('\n');
            break; // End of input
        }

        if (c == '\b') {
            // Handle backspace
            if (pos > 0) {
                pos--;
                kprint("\b \b"); // Erase the character on screen
            }
            continue;
        }

        // Add character to buffer
        buffer[pos++] = c;
        kputchar(c); // Echo character
    }

    buffer[pos] = '\0'; // Null terminate
}

// Get yes/no input
bool get_yes_no_input() {
    while (true) {
        char c = get_char_with_echo();

        if (c == 'y' || c == 'Y') {
            return true;
        }

        if (c == 'n' || c == 'N') {
            return false;
        }

        // Ignore other characters
    }
}

// Create VicOS example files
int create_example_files() {
    // Create directory structure
    if (create_directory("HOME") < 0) {
        kprint("Failed to create HOME directory\n");
        return -1;
    }

    if (create_directory("BIN") < 0) {
        kprint("Failed to create BIN directory\n");
        return -1;
    }

    if (create_directory("ETC") < 0) {
        kprint("Failed to create ETC directory\n");
        return -1;
    }

    // Create README file
    const char* readme_content =
    "Welcome to VicOS!\n\n"
    "This is a basic operating system developed as a learning project.\n"
    "You can explore the filesystem and use basic commands.\n\n"
    "Some available commands:\n"
    "- help: Show available commands\n"
    "- ls: List files and directories\n"
    "- cd: Change directory\n"
    "- cat: Display file contents\n"
    "- mkdir: Create a directory\n"
    "- touch: Create a file\n";

    if (create_file_with_content("README.TXT", readme_content, 255) < 0) {
        kprint("Failed to create README.TXT\n");
        return -1;
    }

    // Create a welcome message
    const char* welcome_content =
    "VicOS has been successfully installed!\n"
    "You can now boot from this drive to start VicOS.\n";

    if (create_file_with_content("WELCOME.TXT", welcome_content, 100) < 0) {
        kprint("Failed to create WELCOME.TXT\n");
        return -1;
    }

    // Create a configuration file
    const char* config_content =
    "# VicOS Configuration\n"
    "VERSION=0.3\n"
    "HOSTNAME=vicos\n"
    "SHELL=vshell\n";

    if (create_file_with_content("ETC/CONFIG", config_content, 60) < 0) {
        kprint("Failed to create ETC/CONFIG\n");
        return -1;
    }

    return 0;
}

// Progress indicator
void show_progress(const char* task, int current, int total) {
    kprint(task);
    kprint(" [");

    const int bar_length = 20;
    int progress = current * bar_length / total;

    for (int i = 0; i < bar_length; i++) {
        if (i < progress) {
            kputchar('#');
        } else {
            kputchar('-');
        }
    }

    kprint("] ");

    char percent[4];
    int_to_str((current * 100) / total, percent);
    kprint(percent);
    kprint("%\n");
}

// Main installation function
int install_to_device(int device_index) {
    if (device_index < 0 || device_index >= num_storage_devices) {
        kprint("Invalid device selection\n");
        return -1;
    }

    StorageDevice* dev = &storage_devices[device_index];

    kprint("Installing VicOS to /dev/");
    kprint(dev->name);
    kprint("\n\n");

    // Set this as the active drive for operations
    if (!set_active_drive(dev->drive_index)) {
        kprint("Failed to set active drive\n");
        return -1;
    }

    // Step 1: Create partition
    kprint("Step 1: Creating partition...\n");
    if (create_vicos_partition() < 0) {
        kprint("Failed to create partition\n");
        return -1;
    }

    // Step 2: Create FAT32 filesystem
    kprint("Step 2: Creating FAT32 filesystem...\n");
    if (create_fat32_filesystem() < 0) {
        kprint("Failed to create filesystem\n");
        return -1;
    }

    // Step 3: Create directory structure and files
    kprint("Step 3: Creating VicOS files...\n");
    if (create_example_files() < 0) {
        kprint("Failed to create example files\n");
        return -1;
    }

    // Step 4: Install bootloader (in a full implementation)
    kprint("Step 4: Installing bootloader...\n");

    // Simulate bootloader installation
    for (int i = 0; i <= 10; i++) {
        show_progress("Installing bootloader", i, 10);

        // Small delay
        for (int j = 0; j < 10000000; j++) {
            asm volatile("nop");
        }
    }

    kprint("\nVicOS has been successfully installed to /dev/");
    kprint(dev->name);
    kprint("!\n");
    kprint("You can now boot from this drive to start VicOS.\n");

    return 0;
}

// Allow manual device entry
bool get_manual_drive() {
    kprint("\nEnter device name (e.g., sdb for USB drive): /dev/");

    char device_name[32];
    get_string_input(device_name, sizeof(device_name));

    if (device_name[0] == '\0') {
        kprint("No device specified. Installation aborted.\n");
        return false;
    }

    // Add the manually entered device
    StorageDevice* drive = &storage_devices[0];
    drive->detected = true;
    drive->drive_index = 0;  // Will use primary master

    // Copy device name
    ri_strcpy(drive->name, device_name);

    // Set other fields
    ri_strcpy(drive->model, "Manual entry - User-specified device");
    drive->size_mb = 0; // Unknown size

    num_storage_devices = 1;
    return true;
}

// Main installer entry point
void process_perm_install(const char* /* command */) {
    clear_screen();

    kprint("VicOS Permanent Installation\n");
    kprint("==========================\n\n");

    // Display options
    kprint("Installation Options:\n");
    kprint("1: Auto-detect drives\n");
    kprint("2: Enter drive manually\n");
    kprint("3: Cancel installation\n\n");
    kprint("Enter your choice (1-3): ");

    int option = get_number_input();
    kprint("\n\n");

    if (option == 3) {
        kprint("Installation cancelled.\n");
        kprint("Press any key to return to shell...");
        get_char_with_echo();
        return;
    }

    if (option == 2) {
        // Manual drive entry
        if (!get_manual_drive()) {
            kprint("Press any key to return to shell...");
            get_char_with_echo();
            return;
        }
    } else {
        // Auto-detect drives
        scan_storage_devices();

        if (num_storage_devices == 0) {
            kprint("No storage devices detected. Please try manual entry.\n");
            kprint("Press any key to return to shell...");
            get_char_with_echo();
            return;
        }
    }

    // Display available drives
    display_storage_devices();

    // Get user selection
    if (num_storage_devices > 1) {
        kprint("Select a drive for installation (1-");
        char num_str[16];
        int_to_str(num_storage_devices, num_str);
        kprint(num_str);
        kprint("): ");

        int selection = get_number_input();
        kprint("\n");

        if (selection < 1 || selection > num_storage_devices) {
            kprint("Invalid selection. Installation aborted.\n");
            kprint("Press any key to return to shell...");
            get_char_with_echo();
            return;
        }

        // Confirm installation
        kprint("\nWARNING: This will erase ALL data on the selected drive!\n");
        kprint("Are you sure you want to install VicOS to /dev/");
        kprint(storage_devices[selection - 1].name);
        kprint("? (y/n): ");

        bool confirmed = get_yes_no_input();
        kprint("\n\n");

        if (!confirmed) {
            kprint("Installation cancelled.\n");
            kprint("Press any key to return to shell...");
            get_char_with_echo();
            return;
        }

        // Perform installation
        int result = install_to_device(selection - 1);

        if (result < 0) {
            kprint("\nInstallation failed. Please try again.\n");
        } else {
            kprint("\nInstallation completed successfully!\n");
        }
    } else {
        // Only one drive available, confirm installation
        kprint("\nWARNING: This will erase ALL data on /dev/");
        kprint(storage_devices[0].name);
        kprint("!\n");
        kprint("Are you sure you want to install VicOS to this drive? (y/n): ");

        bool confirmed = get_yes_no_input();
        kprint("\n\n");

        if (!confirmed) {
            kprint("Installation cancelled.\n");
            kprint("Press any key to return to shell...");
            get_char_with_echo();
            return;
        }

        // Perform installation
        int result = install_to_device(0);

        if (result < 0) {
            kprint("\nInstallation failed. Please try again.\n");
        } else {
            kprint("\nInstallation completed successfully!\n");
        }
    }

    kprint("Press any key to return to shell...");
    get_char_with_echo();
}
