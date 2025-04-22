#include <stdint.h>
#include "vstdint.h"
#include <stddef.h>

// Forward declarations
void kprint(const char* str);

// I/O port functions
static inline void outb(vic_uint16 port, vic_uint8 val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline vic_uint8 inb(vic_uint16 port) {
    vic_uint8 ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait() {
    // Small delay for old hardware
    outb(0x80, 0);
}

static inline void insw(vic_uint16 port, void* addr, vic_size_t count) {
    asm volatile ("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

static inline void outsw(vic_uint16 port, void* addr, vic_size_t count) {
    asm volatile ("rep outsw" : "+S"(addr), "+c"(count) : "d"(port) : "memory");
}

// ATA ports
#define ATA_PRIMARY_DATA          0x1F0
#define ATA_PRIMARY_ERROR         0x1F1
#define ATA_PRIMARY_SECTOR_COUNT  0x1F2
#define ATA_PRIMARY_LBA_LO        0x1F3
#define ATA_PRIMARY_LBA_MID       0x1F4
#define ATA_PRIMARY_LBA_HI        0x1F5
#define ATA_PRIMARY_DRIVE_HEAD    0x1F6
#define ATA_PRIMARY_STATUS        0x1F7
#define ATA_PRIMARY_COMMAND       0x1F7

#define ATA_SECONDARY_DATA        0x170
#define ATA_SECONDARY_ERROR       0x171
#define ATA_SECONDARY_SECTOR_COUNT 0x172
#define ATA_SECONDARY_LBA_LO      0x173
#define ATA_SECONDARY_LBA_MID     0x174
#define ATA_SECONDARY_LBA_HI      0x175
#define ATA_SECONDARY_DRIVE_HEAD  0x176
#define ATA_SECONDARY_STATUS      0x177
#define ATA_SECONDARY_COMMAND     0x177

// ATA commands
#define ATA_CMD_READ_SECTORS      0x20
#define ATA_CMD_WRITE_SECTORS     0x30
#define ATA_CMD_IDENTIFY          0xEC

// Status register bits
#define ATA_SR_BSY   0x80  // Busy
#define ATA_SR_DRDY  0x40  // Drive ready
#define ATA_SR_DF    0x20  // Drive fault
#define ATA_SR_DSC   0x10  // Drive seek complete
#define ATA_SR_DRQ   0x08  // Data request ready
#define ATA_SR_CORR  0x04  // Corrected data
#define ATA_SR_IDX   0x02  // Index
#define ATA_SR_ERR   0x01  // Error

// Error register bits
#define ATA_ER_BBK   0x80  // Bad block
#define ATA_ER_UNC   0x40  // Uncorrectable data
#define ATA_ER_MC    0x20  // Media changed
#define ATA_ER_IDNF  0x10  // ID mark not found
#define ATA_ER_MCR   0x08  // Media change request
#define ATA_ER_ABRT  0x04  // Command aborted
#define ATA_ER_TK0NF 0x02  // Track 0 not found
#define ATA_ER_AMNF  0x01  // Address mark not found

// Drive select bits
#define ATA_MASTER   0xA0
#define ATA_SLAVE    0xB0

// Structure to hold drive information
struct DriveInfo {
    bool exists;
    char model[41];
    vic_uint32 size_mb;
    bool is_master;
    bool is_primary;
};

// Global drive info
#define MAX_DRIVES 4  // Primary master, primary slave, secondary master, secondary slave
DriveInfo detected_drives[MAX_DRIVES];
int active_drive = 0;  // Currently active drive

// Wait for BSY to clear
bool ata_wait_not_busy(vic_uint16 base_port) {
    // Wait up to 30 seconds for drive to be ready
    for (int i = 0; i < 30000; i++) {
        vic_uint8 status = inb(base_port + 7);  // STATUS register
        if (!(status & ATA_SR_BSY)) {
            return true;
        }

        // Short delay
        for (int j = 0; j < 10000; j++) {
            asm volatile("nop");
        }
    }

    return false;
}

// Wait for DRQ to set
bool ata_wait_drq(vic_uint16 base_port) {
    // Wait up to 30 seconds for drive to be ready
    for (int i = 0; i < 30000; i++) {
        vic_uint8 status = inb(base_port + 7);  // STATUS register
        if (status & ATA_SR_DRQ) {
            return true;
        }

        // Short delay
        for (int j = 0; j < 10000; j++) {
            asm volatile("nop");
        }
    }

    return false;
}

// Identify a drive
bool ata_identify(vic_uint16 base_port, vic_uint8 drive_select, DriveInfo* drive_info) {
    // Select the drive
    outb(base_port + 6, drive_select);  // DRIVE/HEAD register
    io_wait();

    // Reset the controller
    outb(base_port + 7, 0x08);  // COMMAND register - reset
    io_wait();

    // Wait for drive to be ready
    if (!ata_wait_not_busy(base_port)) {
        return false;
    }

    // Send IDENTIFY command
    outb(base_port + 2, 0);     // SECTOR COUNT
    outb(base_port + 3, 0);     // LBA LO
    outb(base_port + 4, 0);     // LBA MID
    outb(base_port + 5, 0);     // LBA HI
    outb(base_port + 7, ATA_CMD_IDENTIFY);  // COMMAND register

    // Check if drive exists
    vic_uint8 status = inb(base_port + 7);  // STATUS register
    if (status == 0) {
        // Drive doesn't exist
        return false;
    }

    // Wait for data to be ready or for an error
    bool error = false;
    while (1) {
        status = inb(base_port + 7);  // STATUS register

        if (status & ATA_SR_ERR) {
            error = true;
            break;
        }

        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            break;
        }
    }

    if (error) {
        // Check if it might be an ATAPI device
        vic_uint8 cl = inb(base_port + 4);  // LBA MID
        vic_uint8 ch = inb(base_port + 5);  // LBA HI

        if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96)) {
            // This is an ATAPI device, not a hard drive
            return false;
        }

        // Other error
        return false;
    }

    // Read the identification data
    vic_uint16 identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inb(base_port) | (inb(base_port) << 8);
    }

    // Extract the model number (bytes 54-93, words 27-46)
    for (int i = 0; i < 20; i++) {
        // Swap the bytes to get correct order
        drive_info->model[i*2] = (identify_data[27+i] >> 8) & 0xFF;
        drive_info->model[i*2+1] = identify_data[27+i] & 0xFF;
    }
    drive_info->model[40] = '\0';

    // Trim trailing spaces
    for (int i = 39; i >= 0; i--) {
        if (drive_info->model[i] == ' ') {
            drive_info->model[i] = '\0';
        } else {
            break;
        }
    }

    // Calculate size in MB
    // In modern drives, words 60-61 contain total LBA sectors
    vic_uint32 sectors = identify_data[60] | (identify_data[61] << 16);
    drive_info->size_mb = sectors / 2048;  // 512 bytes/sector * 2048 = 1MB

    return true;
}

// Detect all drives
void detect_all_drives() {
    // Initialize drive info
    for (int i = 0; i < MAX_DRIVES; i++) {
        detected_drives[i].exists = false;
    }

    // Primary master
    kprint("Detecting primary master... ");
    if (ata_identify(ATA_PRIMARY_DATA, ATA_MASTER, &detected_drives[0])) {
        detected_drives[0].exists = true;
        detected_drives[0].is_master = true;
        detected_drives[0].is_primary = true;
        kprint("Found: ");
        kprint(detected_drives[0].model);
        kprint("\n");
    } else {
        kprint("Not found\n");
    }

    // Primary slave
    kprint("Detecting primary slave... ");
    if (ata_identify(ATA_PRIMARY_DATA, ATA_SLAVE, &detected_drives[1])) {
        detected_drives[1].exists = true;
        detected_drives[1].is_master = false;
        detected_drives[1].is_primary = true;
        kprint("Found: ");
        kprint(detected_drives[1].model);
        kprint("\n");
    } else {
        kprint("Not found\n");
    }

    // Secondary master
    kprint("Detecting secondary master... ");
    if (ata_identify(ATA_SECONDARY_DATA, ATA_MASTER, &detected_drives[2])) {
        detected_drives[2].exists = true;
        detected_drives[2].is_master = true;
        detected_drives[2].is_primary = false;
        kprint("Found: ");
        kprint(detected_drives[2].model);
        kprint("\n");
    } else {
        kprint("Not found\n");
    }

    // Secondary slave
    kprint("Detecting secondary slave... ");
    if (ata_identify(ATA_SECONDARY_DATA, ATA_SLAVE, &detected_drives[3])) {
        detected_drives[3].exists = true;
        detected_drives[3].is_master = false;
        detected_drives[3].is_primary = false;
        kprint("Found: ");
        kprint(detected_drives[3].model);
        kprint("\n");
    } else {
        kprint("Not found\n");
    }
}

// Convert number to string
void num_to_str(vic_uint32 num, char* str) {
    int i = 0;

    // Handle 0 case
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    // Convert to string in reverse
    while (num > 0) {
        str[i++] = '0' + (num % 10);
        num /= 10;
    }

    // Null terminate
    str[i] = '\0';

    // Reverse the string
    for (int j = 0; j < i / 2; j++) {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }
}

// Disk initialization
int disk_initialize() {
    kprint("Initializing disk subsystem...\n");

    // Detect all drives
    detect_all_drives();

    // Count how many exist
    int count = 0;
    for (int i = 0; i < MAX_DRIVES; i++) {
        if (detected_drives[i].exists) {
            count++;
        }
    }

    char count_str[16];
    num_to_str(count, count_str);
    kprint("Found ");
    kprint(count_str);
    kprint(" ATA disk drive(s)\n");

    // Set active drive to first found
    for (int i = 0; i < MAX_DRIVES; i++) {
        if (detected_drives[i].exists) {
            active_drive = i;
            break;
        }
    }

    return count;
}

// Set the active drive (0-3)
bool set_active_drive(int drive_index) {
    if (drive_index < 0 || drive_index >= MAX_DRIVES) {
        return false;
    }

    if (!detected_drives[drive_index].exists) {
        return false;
    }

    active_drive = drive_index;
    return true;
}

// Read a sector from the active drive
int disk_read_sector(vic_uint32 lba, vic_uint8* buffer) {
    // Get the base port and drive select for the active drive
    vic_uint16 base_port = detected_drives[active_drive].is_primary ? ATA_PRIMARY_DATA : ATA_SECONDARY_DATA;
    vic_uint8 drive_select = detected_drives[active_drive].is_master ? ATA_MASTER : ATA_SLAVE;

    // Select drive
    outb(base_port + 6, (drive_select | ((lba >> 24) & 0x0F)));  // DRIVE/HEAD register

    // Wait for drive to be ready
    if (!ata_wait_not_busy(base_port)) {
        kprint("Drive not ready during read\n");
        return -1;
    }

    // Set parameters
    outb(base_port + 2, 1);                 // SECTOR COUNT - Read 1 sector
    outb(base_port + 3, lba & 0xFF);        // LBA LO
    outb(base_port + 4, (lba >> 8) & 0xFF); // LBA MID
    outb(base_port + 5, (lba >> 16) & 0xFF);// LBA HI

    // Send read command
    outb(base_port + 7, ATA_CMD_READ_SECTORS);  // COMMAND register

    // Wait for data
    if (!ata_wait_drq(base_port)) {
        kprint("Drive not ready for data transfer during read\n");
        return -1;
    }

    // Read the data
    for (int i = 0; i < 256; i++) {
        vic_uint16 data = inb(base_port) | (inb(base_port) << 8);
        buffer[i*2] = data & 0xFF;
        buffer[i*2 + 1] = (data >> 8) & 0xFF;
    }

    return 0;
}

// Write a sector to the active drive
int disk_write_sector(vic_uint32 lba, const vic_uint8* buffer) {
    // Get the base port and drive select for the active drive
    vic_uint16 base_port = detected_drives[active_drive].is_primary ? ATA_PRIMARY_DATA : ATA_SECONDARY_DATA;
    vic_uint8 drive_select = detected_drives[active_drive].is_master ? ATA_MASTER : ATA_SLAVE;

    // Select drive
    outb(base_port + 6, (drive_select | ((lba >> 24) & 0x0F)));  // DRIVE/HEAD register

    // Wait for drive to be ready
    if (!ata_wait_not_busy(base_port)) {
        kprint("Drive not ready during write\n");
        return -1;
    }

    // Set parameters
    outb(base_port + 2, 1);                 // SECTOR COUNT - Write 1 sector
    outb(base_port + 3, lba & 0xFF);        // LBA LO
    outb(base_port + 4, (lba >> 8) & 0xFF); // LBA MID
    outb(base_port + 5, (lba >> 16) & 0xFF);// LBA HI

    // Send write command
    outb(base_port + 7, ATA_CMD_WRITE_SECTORS);  // COMMAND register

    // Wait for DRQ (ready for data)
    if (!ata_wait_drq(base_port)) {
        kprint("Drive not ready for data transfer during write\n");
        return -1;
    }

    // Write the data
    for (int i = 0; i < 256; i++) {
        vic_uint16 data = buffer[i*2] | (buffer[i*2 + 1] << 8);
        outb(base_port, data & 0xFF);
        outb(base_port, (data >> 8) & 0xFF);
    }

    return 0;
}

// Get drive information
int disk_get_drive_info(int drive_index, bool* exists, char* model, vic_uint32* size_mb) {
    if (drive_index < 0 || drive_index >= MAX_DRIVES) {
        return -1;
    }

    *exists = detected_drives[drive_index].exists;

    if (*exists) {
        // Copy model
        int i;
        for (i = 0; detected_drives[drive_index].model[i] != '\0'; i++) {
            model[i] = detected_drives[drive_index].model[i];
        }
        model[i] = '\0';

        // Copy size
        *size_mb = detected_drives[drive_index].size_mb;
    }

    return 0;
}

// Get size of the active drive in sectors
vic_uint32 disk_get_size() {
    if (!detected_drives[active_drive].exists) {
        return 0;
    }

    return detected_drives[active_drive].size_mb * 2048;  // 2048 sectors per MB
}

// Disk detection entry point
void disk_detect() {
    kprint("Detecting disk drives...\n");

    // Display info about detected drives
    for (int i = 0; i < MAX_DRIVES; i++) {
        if (detected_drives[i].exists) {
            kprint("Drive ");
            char drive_num[2] = {static_cast<char>('0' + i), '\0'};
            kprint(drive_num);
            kprint(": ");
            kprint(detected_drives[i].model);
            kprint(" (");

            char size_str[16];
            num_to_str(detected_drives[i].size_mb, size_str);
            kprint(size_str);
            kprint(" MB)\n");
        }
    }
}
