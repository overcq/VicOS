#include <stdint.h>
#include "vstdint.h"
#include <stddef.h>

// Forward declarations
void kprint(const char* str);
void kputchar(char c);  // Add this forward declaration
int disk_read_sector(vic_uint32 lba, vic_uint8* buffer);
int disk_write_sector(vic_uint32 lba, const vic_uint8* buffer);
vic_uint32 disk_get_size();

// Partition types
#define PART_TYPE_EMPTY     0x00
#define PART_TYPE_FAT32     0x0C
#define PART_TYPE_FAT32_LBA 0x0C
#define PART_TYPE_VICOS     0x7F  // Custom type for VicOS

// MBR structure
struct MBRPartitionEntry {
    vic_uint8 bootable;       // 0x80 = bootable, 0x00 = not bootable
    vic_uint8 start_head;     // CHS addressing
    vic_uint8 start_sector;   // CHS addressing
    vic_uint8 start_cylinder; // CHS addressing
    vic_uint8 system_id;      // Partition type
    vic_uint8 end_head;       // CHS addressing
    vic_uint8 end_sector;     // CHS addressing
    vic_uint8 end_cylinder;   // CHS addressing
    vic_uint32 start_lba;     // LBA of first sector
    vic_uint32 sector_count;  // Number of sectors
} __attribute__((packed));

struct MBR {
    vic_uint8 bootstrap[446];
    MBRPartitionEntry partitions[4];
    vic_uint16 signature;     // 0xAA55
} __attribute__((packed));

// Convert LBA to CHS
void lba_to_chs(vic_uint32 lba, vic_uint8* head, vic_uint8* sector, vic_uint8* cylinder) {
    // Use a simplified approach with fixed geometry
    const vic_uint8 heads_per_cylinder = 16;
    const vic_uint8 sectors_per_track = 63;

    *head = (lba / sectors_per_track) % heads_per_cylinder;
    *sector = (lba % sectors_per_track) + 1;
    *cylinder = (lba / (sectors_per_track * heads_per_cylinder)) & 0xFF;
}

// Read the MBR
int read_mbr(MBR* mbr) {
    vic_uint8 sector[512];

    if (disk_read_sector(0, sector) < 0) {
        kprint("Failed to read MBR\n");
        return -1;
    }

    // Copy sector data to MBR structure
    for (vic_size_t i = 0; i < 512; i++) {
        ((vic_uint8*)mbr)[i] = sector[i];
    }

    // Check MBR signature
    if (mbr->signature != 0xAA55) {
        kprint("Invalid MBR signature\n");
        return -1;
    }

    return 0;
}

// Write the MBR
int write_mbr(const MBR* mbr) {
    vic_uint8 sector[512];

    // Copy MBR structure to sector data
    for (vic_size_t i = 0; i < 512; i++) {
        sector[i] = ((vic_uint8*)mbr)[i];
    }

    if (disk_write_sector(0, sector) < 0) {
        kprint("Failed to write MBR\n");
        return -1;
    }

    return 0;
}

// Create a new partition table
int create_partition_table() {
    MBR mbr;

    // Clear the MBR
    for (vic_size_t i = 0; i < sizeof(MBR); i++) {
        ((vic_uint8*)&mbr)[i] = 0;
    }

    // Set MBR signature
    mbr.signature = 0xAA55;

    // Write empty MBR
    return write_mbr(&mbr);
}

// Create a new VicOS partition that uses the entire disk
int create_vicos_partition() {
    MBR mbr;

    // Read current MBR
    if (read_mbr(&mbr) < 0) {
        // If reading fails, create a new partition table
        if (create_partition_table() < 0) {
            kprint("Failed to create partition table\n");
            return -1;
        }

        // Read the newly created MBR
        if (read_mbr(&mbr) < 0) {
            kprint("Failed to read newly created MBR\n");
            return -1;
        }
    }

    // Get the disk size in sectors
    vic_uint32 total_sectors = disk_get_size();
    if (total_sectors == 0) {
        kprint("Failed to get disk size\n");
        return -1;
    }

    // Set up partition 1 to use the entire disk (minus the MBR)
    mbr.partitions[0].bootable = 0x80; // Bootable
    mbr.partitions[0].system_id = PART_TYPE_FAT32_LBA;
    mbr.partitions[0].start_lba = 2048; // Standard starting position for alignment
    mbr.partitions[0].sector_count = total_sectors - 2048;

    // Set CHS values
    lba_to_chs(mbr.partitions[0].start_lba,
               &mbr.partitions[0].start_head,
               &mbr.partitions[0].start_sector,
               &mbr.partitions[0].start_cylinder);

    lba_to_chs(mbr.partitions[0].start_lba + mbr.partitions[0].sector_count - 1,
               &mbr.partitions[0].end_head,
               &mbr.partitions[0].end_sector,
               &mbr.partitions[0].end_cylinder);

    // Clear other partitions
    for (int i = 1; i < 4; i++) {
        mbr.partitions[i].bootable = 0;
        mbr.partitions[i].system_id = PART_TYPE_EMPTY;
        mbr.partitions[i].start_lba = 0;
        mbr.partitions[i].sector_count = 0;
        mbr.partitions[i].start_head = 0;
        mbr.partitions[i].start_sector = 0;
        mbr.partitions[i].start_cylinder = 0;
        mbr.partitions[i].end_head = 0;
        mbr.partitions[i].end_sector = 0;
        mbr.partitions[i].end_cylinder = 0;
    }

    // Write updated MBR
    if (write_mbr(&mbr) < 0) {
        kprint("Failed to write updated MBR\n");
        return -1;
    }

    kprint("VicOS partition created successfully\n");
    return 0;
}

// Print partition table
void print_partition_table() {
    MBR mbr;

    if (read_mbr(&mbr) < 0) {
        kprint("Failed to read MBR\n");
        return;
    }

    kprint("Partition Table:\n");
    kprint("----------------\n");

    for (int i = 0; i < 4; i++) {
        MBRPartitionEntry* part = &mbr.partitions[i];

        if (part->system_id == PART_TYPE_EMPTY) {
            continue;
        }

        kprint("Partition ");
        char num = '1' + i;
        kputchar(num);
        kprint(":\n");

        kprint("  Bootable: ");
        kprint(part->bootable == 0x80 ? "Yes\n" : "No\n");

        kprint("  Type: 0x");
        char hex[3];
        hex[0] = "0123456789ABCDEF"[(part->system_id >> 4) & 0xF];
        hex[1] = "0123456789ABCDEF"[part->system_id & 0xF];
        hex[2] = '\0';
        kprint(hex);

        if (part->system_id == PART_TYPE_FAT32 || part->system_id == PART_TYPE_FAT32_LBA) {
            kprint(" (FAT32)\n");
        } else if (part->system_id == PART_TYPE_VICOS) {
            kprint(" (VicOS)\n");
        } else {
            kprint("\n");
        }

        kprint("  Start LBA: ");
        // Simple conversion to string
        char lba_str[16];
        int idx = 0;
        vic_uint32 temp = part->start_lba;
        do {
            lba_str[idx++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp > 0);

        // Reverse the string
        for (int j = 0; j < idx/2; j++) {
            char tmp = lba_str[j];
            lba_str[j] = lba_str[idx-j-1];
            lba_str[idx-j-1] = tmp;
        }
        lba_str[idx] = '\0';
        kprint(lba_str);
        kprint("\n");

        kprint("  Size: ");
        // Calculate size in MB
        vic_uint32 size_mb = part->sector_count / 2048; // 512 bytes/sector * 2048 = 1MB
        idx = 0;
        temp = size_mb;
        do {
            lba_str[idx++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp > 0);

        // Reverse the string
        for (int j = 0; j < idx/2; j++) {
            char tmp = lba_str[j];
            lba_str[j] = lba_str[idx-j-1];
            lba_str[idx-j-1] = tmp;
        }
        lba_str[idx] = '\0';
        kprint(lba_str);
        kprint(" MB\n");
    }
}

// Get partition information
int get_partition_info(int partition_num, vic_uint32* start_lba, vic_uint32* sector_count) {
    if (partition_num < 1 || partition_num > 4) {
        return -1;
    }

    MBR mbr;

    if (read_mbr(&mbr) < 0) {
        return -1;
    }

    MBRPartitionEntry* part = &mbr.partitions[partition_num - 1];

    if (part->system_id == PART_TYPE_EMPTY) {
        return -1;
    }

    *start_lba = part->start_lba;
    *sector_count = part->sector_count;

    return 0;
}
