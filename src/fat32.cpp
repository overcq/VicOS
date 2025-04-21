#include <stdint.h>
#include <stddef.h>

// Forward declarations
void kprint(const char* str);
int disk_read_sector(uint32_t lba, uint8_t* buffer);
int disk_write_sector(uint32_t lba, const uint8_t* buffer);
int get_partition_info(int partition_num, uint32_t* start_lba, uint32_t* sector_count);

// String operations
void fat_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

void fat_memset(void* ptr, uint8_t value, size_t num) {
    uint8_t* p = (uint8_t*)ptr;
    for (size_t i = 0; i < num; i++) {
        p[i] = value;
    }
}

void fat_memcpy(void* dest, const void* src, size_t num) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < num; i++) {
        d[i] = s[i];
    }
}

// FAT32 structures
struct FAT32_BootSector {
    uint8_t  jump_code[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;          // Always 0 for FAT32
    uint16_t total_sectors_16;      // Always 0 for FAT32
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat_16;    // Always 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    // FAT32 specific fields
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} __attribute__((packed));

struct FAT32_FSInfo {
    uint32_t lead_signature;       // 0x41615252
    uint8_t  reserved1[480];
    uint32_t structure_signature;  // 0x61417272
    uint32_t free_count;           // Last known free cluster count, or 0xFFFFFFFF if unknown
    uint32_t next_free;            // Hint for next free cluster, or 0xFFFFFFFF if unknown
    uint8_t  reserved2[12];
    uint32_t trail_signature;      // 0xAA550000
} __attribute__((packed));

// FAT32 directory entry
struct FAT32_DirEntry {
    char     name[8];
    char     ext[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed));

// LFN (Long File Name) entry
struct FAT32_LFNEntry {
    uint8_t  sequence;
    uint16_t name1[5];
    uint8_t  attributes;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t cluster_low;
    uint16_t name3[2];
} __attribute__((packed));

// Directory attributes
#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LFN       0x0F

// FAT32 constants
#define FAT32_EOC          0x0FFFFFF8  // End of cluster chain marker
#define FAT32_BAD_CLUSTER  0x0FFFFFF7  // Bad cluster marker

// Globals for mounted filesystem
uint32_t fat32_partition_start = 0;
uint32_t fat32_fat_start = 0;
uint32_t fat32_data_start = 0;
uint32_t fat32_root_cluster = 0;
uint16_t fat32_bytes_per_sector = 0;
uint8_t  fat32_sectors_per_cluster = 0;
uint32_t fat32_sectors_per_fat = 0;
uint8_t  fat32_num_fats = 0;

// Calculate needed sectors for given cluster count
uint32_t calculate_fat_size(uint32_t cluster_count) {
    // 4 bytes per entry in FAT32
    uint32_t bytes_needed = cluster_count * 4;
    // Convert to sectors (rounding up)
    return (bytes_needed + 511) / 512;
}

// Create a FAT32 filesystem on partition 1
int create_fat32_filesystem() {
    uint32_t partition_start, partition_size;

    // Get partition information
    if (get_partition_info(1, &partition_start, &partition_size) < 0) {
        kprint("Failed to get partition information\n");
        return -1;
    }

    kprint("Creating FAT32 filesystem on partition 1...\n");

    // Calculate filesystem parameters
    uint8_t sectors_per_cluster;
    if (partition_size < 66600) {                   // < 32MB
        sectors_per_cluster = 1;
    } else if (partition_size < 133200) {           // < 64MB
        sectors_per_cluster = 2;
    } else if (partition_size < 266400) {           // < 128MB
        sectors_per_cluster = 4;
    } else if (partition_size < 532800) {           // < 256MB
        sectors_per_cluster = 8;
    } else if (partition_size < 16777216) {         // < 8GB
        sectors_per_cluster = 16;
    } else if (partition_size < 33554432) {         // < 16GB
        sectors_per_cluster = 32;
    } else {                                         // >= 16GB
        sectors_per_cluster = 64;
    }

    // Calculate number of clusters
    uint32_t reserved_sectors = 32;  // Reserved sectors before FAT
    uint32_t estimated_cluster_count = partition_size / sectors_per_cluster;

    // Calculate size of each FAT
    uint32_t sectors_per_fat = calculate_fat_size(estimated_cluster_count);

    // Recalculate cluster count based on FAT size
    uint32_t data_sectors = partition_size - reserved_sectors - (2 * sectors_per_fat);
    uint32_t cluster_count = data_sectors / sectors_per_cluster;

    // Prepare boot sector
    FAT32_BootSector boot_sector;
    fat_memset(&boot_sector, 0, sizeof(boot_sector));

    // Jump instruction (EB 58 90) - jump over the OEM name to the BIOS parameter block
    boot_sector.jump_code[0] = 0xEB;
    boot_sector.jump_code[1] = 0x58;
    boot_sector.jump_code[2] = 0x90;

    // OEM Name
    fat_strcpy(boot_sector.oem_name, "VICOS   ");

    // BIOS Parameter Block
    boot_sector.bytes_per_sector = 512;
    boot_sector.sectors_per_cluster = sectors_per_cluster;
    boot_sector.reserved_sectors = reserved_sectors;
    boot_sector.num_fats = 2;
    boot_sector.root_entries = 0;            // Always 0 for FAT32
    boot_sector.total_sectors_16 = 0;        // Always 0 for FAT32
    boot_sector.media_descriptor = 0xF8;     // Fixed disk
    boot_sector.sectors_per_fat_16 = 0;      // Always 0 for FAT32
    boot_sector.sectors_per_track = 63;      // Standard geometry
    boot_sector.num_heads = 255;             // Standard geometry
    boot_sector.hidden_sectors = partition_start;
    boot_sector.total_sectors_32 = partition_size;

    // FAT32 specific parameters
    boot_sector.sectors_per_fat_32 = sectors_per_fat;
    boot_sector.flags = 0;
    boot_sector.fat_version = 0;
    boot_sector.root_cluster = 2;             // Root directory starts at cluster 2
    boot_sector.fs_info_sector = 1;           // FSInfo structure is in sector 1
    boot_sector.backup_boot_sector = 6;       // Backup boot sector is at sector 6
    boot_sector.drive_number = 0x80;          // First hard drive
    boot_sector.boot_signature = 0x29;        // Extended boot signature
    boot_sector.volume_id = 0x12345678;       // Random volume ID
    fat_strcpy(boot_sector.volume_label, "VICOS     ");
    fat_strcpy(boot_sector.fs_type, "FAT32   ");

    // Write boot sector to partition
    uint8_t sector[512];
    fat_memcpy(sector, &boot_sector, sizeof(boot_sector));

    // Add boot sector signature
    sector[510] = 0x55;
    sector[511] = 0xAA;

    // Write to LBA 0 of the partition
    if (disk_write_sector(partition_start, sector) < 0) {
        kprint("Failed to write boot sector\n");
        return -1;
    }

    // Write backup boot sector
    if (disk_write_sector(partition_start + boot_sector.backup_boot_sector, sector) < 0) {
        kprint("Failed to write backup boot sector\n");
        return -1;
    }

    // Create and write FSInfo sector
    FAT32_FSInfo fs_info;
    fat_memset(&fs_info, 0, sizeof(fs_info));

    // FSInfo signatures
    fs_info.lead_signature = 0x41615252;
    fs_info.structure_signature = 0x61417272;
    fs_info.free_count = cluster_count - 1;  // Cluster 2 is used for root directory
    fs_info.next_free = 3;                   // Cluster 3 is the next free
    fs_info.trail_signature = 0xAA550000;

    // Write FSInfo to sector
    fat_memcpy(sector, &fs_info, sizeof(fs_info));

    // Write FSInfo sector
    if (disk_write_sector(partition_start + boot_sector.fs_info_sector, sector) < 0) {
        kprint("Failed to write FSInfo sector\n");
        return -1;
    }

    // Initialize FAT tables
    fat_memset(sector, 0, 512);

    // First FAT sector special entries
    uint32_t* fat = (uint32_t*)sector;
    fat[0] = 0x0FFFFF00 | boot_sector.media_descriptor;  // Media descriptor in low byte
    fat[1] = 0x0FFFFFFF;  // End of cluster chain marker for reserved clusters
    fat[2] = 0x0FFFFFFF;  // End of cluster chain for root directory (cluster 2)

    // Write first sector of each FAT
    uint32_t fat_start = partition_start + boot_sector.reserved_sectors;
    if (disk_write_sector(fat_start, sector) < 0) {
        kprint("Failed to write first FAT sector\n");
        return -1;
    }

    if (disk_write_sector(fat_start + boot_sector.sectors_per_fat_32, sector) < 0) {
        kprint("Failed to write second FAT first sector\n");
        return -1;
    }

    // Clear the rest of both FATs
    fat_memset(sector, 0, 512);
    for (uint32_t i = 1; i < boot_sector.sectors_per_fat_32; i++) {
        if (disk_write_sector(fat_start + i, sector) < 0) {
            kprint("Failed to clear FAT sector\n");
            return -1;
        }
        if (disk_write_sector(fat_start + boot_sector.sectors_per_fat_32 + i, sector) < 0) {
            kprint("Failed to clear second FAT sector\n");
            return -1;
        }
    }

    // Initialize root directory with volume label
    fat_memset(sector, 0, 512);
    FAT32_DirEntry* dir_entry = (FAT32_DirEntry*)sector;

    // Volume label entry
    fat_strcpy(dir_entry->name, "VICOS   ");
    fat_strcpy(dir_entry->ext, "   ");
    dir_entry->attributes = FAT_ATTR_VOLUME_ID;

    // Write root directory (cluster 2)
    uint32_t root_dir_sector = fat_start + (2 * boot_sector.sectors_per_fat_32) +
    ((2 - 2) * boot_sector.sectors_per_cluster);

    if (disk_write_sector(root_dir_sector, sector) < 0) {
        kprint("Failed to write root directory\n");
        return -1;
    }

    kprint("FAT32 filesystem created successfully\n");

    // Store filesystem parameters for future use
    fat32_partition_start = partition_start;
    fat32_fat_start = fat_start;
    fat32_data_start = fat_start + (2 * boot_sector.sectors_per_fat_32);
    fat32_root_cluster = 2;
    fat32_bytes_per_sector = 512;
    fat32_sectors_per_cluster = boot_sector.sectors_per_cluster;
    fat32_sectors_per_fat = boot_sector.sectors_per_fat_32;
    fat32_num_fats = boot_sector.num_fats;

    return 0;
}

// Convert cluster number to first sector LBA
uint32_t cluster_to_sector(uint32_t cluster) {
    return fat32_data_start + ((cluster - 2) * fat32_sectors_per_cluster);
}

// Read the FAT entry for a cluster
uint32_t read_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat32_fat_start + (fat_offset / fat32_bytes_per_sector);
    uint32_t entry_offset = fat_offset % fat32_bytes_per_sector;

    uint8_t sector[512];
    if (disk_read_sector(fat_sector, sector) < 0) {
        return FAT32_BAD_CLUSTER;
    }

    return *((uint32_t*)(sector + entry_offset)) & 0x0FFFFFFF;
}

// Write a FAT entry
int write_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat32_fat_start + (fat_offset / fat32_bytes_per_sector);
    uint32_t entry_offset = fat_offset % fat32_bytes_per_sector;

    // Read the sector
    uint8_t sector[512];
    if (disk_read_sector(fat_sector, sector) < 0) {
        return -1;
    }

    // Update the entry
    uint32_t* entry = (uint32_t*)(sector + entry_offset);
    *entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);

    // Write to both FATs
    if (disk_write_sector(fat_sector, sector) < 0) {
        return -1;
    }

    // Update second FAT
    if (disk_write_sector(fat_sector + fat32_sectors_per_fat, sector) < 0) {
        return -1;
    }

    return 0;
}

// Allocate a new cluster
uint32_t allocate_cluster() {
    // Start from the root directory cluster and scan forward
    uint32_t cluster = fat32_root_cluster;

    // Find the first free cluster
    while (cluster < 0x0FFFFFF0) {
        uint32_t entry = read_fat_entry(cluster);
        if (entry == 0) {
            // Found a free cluster
            if (write_fat_entry(cluster, FAT32_EOC) < 0) {
                return 0; // Error
            }

            // Clear the cluster
            uint32_t sector = cluster_to_sector(cluster);
            uint8_t empty_sector[512];
            fat_memset(empty_sector, 0, 512);

            for (uint32_t i = 0; i < fat32_sectors_per_cluster; i++) {
                if (disk_write_sector(sector + i, empty_sector) < 0) {
                    return 0; // Error
                }
            }

            return cluster;
        }

        cluster++;
    }

    return 0; // No free clusters found
}

// Add a cluster to a chain
int extend_cluster_chain(uint32_t start_cluster) {
    if (start_cluster < 2) {
        return -1;
    }

    // Find the end of the chain
    uint32_t current = start_cluster;
    uint32_t next = read_fat_entry(current);

    while (next < FAT32_EOC) {
        current = next;
        next = read_fat_entry(current);
    }

    // Allocate a new cluster
    uint32_t new_cluster = allocate_cluster();
    if (new_cluster == 0) {
        return -1;
    }

    // Link the new cluster to the chain
    if (write_fat_entry(current, new_cluster) < 0) {
        return -1;
    }

    return 0;
}

// Convert a filename to 8.3 format
void filename_to_83(const char* filename, char* name83) {
    int i, j;

    // Initialize name and extension with spaces
    for (i = 0; i < 11; i++) {
        name83[i] = ' ';
    }

    // Copy name
    for (i = 0; i < 8 && filename[i] && filename[i] != '.'; i++) {
        name83[i] = filename[i];
    }

    // Find extension
    const char* ext = filename;
    while (*ext && *ext != '.') {
        ext++;
    }

    if (*ext == '.') {
        ext++; // Skip the dot

        // Copy extension
        for (j = 0; j < 3 && ext[j]; j++) {
            name83[8 + j] = ext[j];
        }
    }

    // Convert to uppercase
    for (i = 0; i < 11; i++) {
        if (name83[i] >= 'a' && name83[i] <= 'z') {
            name83[i] = name83[i] - 'a' + 'A';
        }
    }
}

// Create a file or directory
int create_file(const char* filename, uint8_t attributes, uint32_t size, uint32_t first_cluster) {
    // Convert filename to 8.3 format
    char name83[11];
    filename_to_83(filename, name83);

    // Find a free directory entry in the root directory
    uint32_t cluster = fat32_root_cluster;
    uint32_t sector = cluster_to_sector(cluster);

    uint8_t sector_buffer[512];
    if (disk_read_sector(sector, sector_buffer) < 0) {
        return -1;
    }

    // Find a free entry
    FAT32_DirEntry* dir_entry = (FAT32_DirEntry*)sector_buffer;
    int entry_index = -1;

    for (int i = 0; i < 16; i++) { // Assume 16 entries per sector
        if (dir_entry[i].name[0] == 0 || dir_entry[i].name[0] == 0xE5) {
            // Found a free entry
            entry_index = i;
            break;
        }
    }

    if (entry_index == -1) {
        // No free entry found in the first sector
        // In a real implementation, we would search more sectors or extend the directory
        kprint("No free directory entry found\n");
        return -1;
    }

    // Create the directory entry
    dir_entry = &((FAT32_DirEntry*)sector_buffer)[entry_index];

    // Copy name and extension
    for (int i = 0; i < 8; i++) {
        dir_entry->name[i] = name83[i];
    }

    for (int i = 0; i < 3; i++) {
        dir_entry->ext[i] = name83[8 + i];
    }

    dir_entry->attributes = attributes;
    dir_entry->file_size = size;

    // Set cluster number
    dir_entry->cluster_low = first_cluster & 0xFFFF;
    dir_entry->cluster_high = (first_cluster >> 16) & 0xFFFF;

    // Set creation/modification time (fixed for simplicity)
    dir_entry->create_date = 0x4876; // January 1, 2021
    dir_entry->create_time = 0x0000; // 00:00:00
    dir_entry->modify_date = 0x4876;
    dir_entry->modify_time = 0x0000;

    // Write the updated directory sector
    if (disk_write_sector(sector, sector_buffer) < 0) {
        return -1;
    }

    return 0;
}

// Create a directory
int create_directory(const char* dirname) {
    // Allocate a cluster for the directory
    uint32_t cluster = allocate_cluster();
    if (cluster == 0) {
        return -1;
    }

    // Initialize the directory with . and .. entries
    uint8_t sector_buffer[512];
    fat_memset(sector_buffer, 0, 512);

    FAT32_DirEntry* dir_entry = (FAT32_DirEntry*)sector_buffer;

    // . entry (current directory)
    fat_strcpy(dir_entry[0].name, ".       ");
    fat_strcpy(dir_entry[0].ext, "   ");
    dir_entry[0].attributes = FAT_ATTR_DIRECTORY;
    dir_entry[0].cluster_low = cluster & 0xFFFF;
    dir_entry[0].cluster_high = (cluster >> 16) & 0xFFFF;

    // .. entry (parent directory - root in this case)
    fat_strcpy(dir_entry[1].name, "..      ");
    fat_strcpy(dir_entry[1].ext, "   ");
    dir_entry[1].attributes = FAT_ATTR_DIRECTORY;
    dir_entry[1].cluster_low = fat32_root_cluster & 0xFFFF;
    dir_entry[1].cluster_high = (fat32_root_cluster >> 16) & 0xFFFF;

    // Write the directory sector
    uint32_t sector = cluster_to_sector(cluster);
    if (disk_write_sector(sector, sector_buffer) < 0) {
        return -1;
    }

    // Create directory entry in the root
    return create_file(dirname, FAT_ATTR_DIRECTORY, 0, cluster);
}

// Write file content
int write_file_content(uint32_t cluster, const void* data, uint32_t size) {
    uint32_t bytes_per_cluster = fat32_sectors_per_cluster * fat32_bytes_per_sector;
    uint32_t clusters_needed = (size + bytes_per_cluster - 1) / bytes_per_cluster;

    // Ensure we have enough clusters
    uint32_t current_cluster = cluster;
    uint32_t remaining_clusters = clusters_needed;

    while (--remaining_clusters > 0) {
        if (extend_cluster_chain(current_cluster) < 0) {
            return -1;
        }

        uint32_t next = read_fat_entry(current_cluster);
        if (next >= FAT32_EOC) {
            return -1;
        }

        current_cluster = next;
    }

    // Write the data
    const uint8_t* data_ptr = (const uint8_t*)data;
    uint32_t bytes_written = 0;
    current_cluster = cluster;

    while (bytes_written < size) {
        uint32_t sector = cluster_to_sector(current_cluster);

        for (uint32_t i = 0; i < fat32_sectors_per_cluster && bytes_written < size; i++) {
            uint8_t sector_buffer[512];

            // Clear the sector
            fat_memset(sector_buffer, 0, 512);

            // Copy data to the sector
            uint32_t bytes_to_write = size - bytes_written;
            if (bytes_to_write > 512) {
                bytes_to_write = 512;
            }

            fat_memcpy(sector_buffer, data_ptr, bytes_to_write);
            data_ptr += bytes_to_write;
            bytes_written += bytes_to_write;

            // Write the sector
            if (disk_write_sector(sector + i, sector_buffer) < 0) {
                return -1;
            }
        }

        if (bytes_written < size) {
            // Move to the next cluster
            current_cluster = read_fat_entry(current_cluster);
            if (current_cluster >= FAT32_EOC) {
                return -1;
            }
        }
    }

    return 0;
}

// Create a file with content
int create_file_with_content(const char* filename, const void* data, uint32_t size) {
    // Allocate the first cluster
    uint32_t cluster = allocate_cluster();
    if (cluster == 0) {
        return -1;
    }

    // Create file entry
    if (create_file(filename, FAT_ATTR_ARCHIVE, size, cluster) < 0) {
        return -1;
    }

    // Write the content
    return write_file_content(cluster, data, size);
}
