#include <stdint.h>
#include <stddef.h>
#include "fatfs_integration.h"
extern "C" {
    void fatfs_init();
    int fatfs_mkdir(const char* path);
    int fatfs_write_file(const char* path, const char* content, unsigned int len);
    int fatfs_read_file(const char* path, char* buffer, unsigned int buffer_size, unsigned int* bytes_read);
    int fatfs_ls(const char* path);
    int fatfs_cd(const char* path);
    int fatfs_pwd(char* buffer, unsigned int size);
}

// Forward declaration of FatFs types without including the headers directly
#ifdef __cplusplus
extern "C" {
#endif

// Forward declare only the necessary types from FatFs
typedef int FRESULT;
typedef unsigned int UINT;


// Forward declare the global filesystem object
struct FATFS;  // Forward declaration
extern struct FATFS g_fatfs;

#ifdef __cplusplus
}
#endif

// Forward declarations for VicOS functions
void kprint(const char* str);
int disk_read_sector(uint32_t lba, uint8_t* buffer);
int disk_write_sector(uint32_t lba, const uint8_t* buffer);
int get_partition_info(int partition_num, uint32_t* start_lba, uint32_t* sector_count);

// String operations - kept for compatibility
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

// Create a FAT32 filesystem on partition 1 using FatFS
int create_fat32_filesystem() {
    uint32_t partition_start, partition_size;

    // Get partition information
    if (get_partition_info(1, &partition_start, &partition_size) < 0) {
        kprint("Failed to get partition information\n");
        return -1;
    }

    kprint("Creating FAT32 filesystem on partition 1...\n");

    // Initialize FatFS
    fatfs_init();

    // The formatting is now handled by fatfs_init() if needed

    kprint("FAT32 filesystem created successfully\n");
    return 0;
}

// Create a directory using FatFS
int create_directory(const char* dirname) {
    FRESULT res = fatfs_mkdir(dirname);

    if (res != 0) { // FR_OK is 0
        kprint("Failed to create directory: ");
        kprint(dirname);
        kprint("\n");
        return -1;
    }

    kprint("Directory created: ");
    kprint(dirname);
    kprint("\n");
    return 0;
}

// Create a file with content using FatFS
int create_file_with_content(const char* filename, const void* data, uint32_t size) {
    FRESULT res = fatfs_write_file(filename, (const char*)data, size);

    if (res != 0) { // FR_OK is 0
        kprint("Failed to create file: ");
        kprint(filename);
        kprint("\n");
        return -1;
    }

    kprint("File created: ");
    kprint(filename);
    kprint("\n");
    return 0;
}

// Read file content using FatFS
const char* read_file_content(const char* filename, char* buffer, size_t buffer_size) {
    size_t bytes_read;
    FRESULT res = fatfs_read_file(filename, buffer, buffer_size, &bytes_read);

    if (res != 0) { // FR_OK is 0
        kprint("Failed to read file: ");
        kprint(filename);
        kprint("\n");
        return nullptr;
    }

    return buffer;
}

// List directory contents using FatFS
int list_directory(const char* path) {
    FRESULT res = fatfs_ls(path);

    if (res != 0) { // FR_OK is 0
        kprint("Failed to list directory: ");
        kprint(path);
        kprint("\n");
        return -1;
    }

    return 0;
}

// Change current directory using FatFS
int change_directory(const char* path) {
    FRESULT res = fatfs_cd(path);

    if (res != 0) { // FR_OK is 0
        kprint("Failed to change directory: ");
        kprint(path);
        kprint("\n");
        return -1;
    }

    return 0;
}

// Get current working directory using FatFS
const char* get_current_directory(char* buffer, size_t buffer_size) {
    FRESULT res = fatfs_pwd(buffer, (UINT)buffer_size);

    if (res != 0) { // FR_OK is 0
        kprint("Failed to get current directory\n");
        return nullptr;
    }

    return buffer;
}