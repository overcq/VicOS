#include <stdint.h>
#include "vstdint.h"
#include <stddef.h>
#include "fatfs_integration.h"

extern "C" {
    void fatfs_init();
    FRESULT fatfs_mkdir(const char* path);
    FRESULT fatfs_write_file(const char* path, const char* content, vic_size_t len);
    FRESULT fatfs_read_file(const char* path, char* buffer, vic_size_t buffer_size, vic_size_t* bytes_read);
    FRESULT fatfs_ls(const char* path);
    FRESULT fatfs_cd(const char* path);
    FRESULT fatfs_pwd(char* buffer, vic_size_t size);
}

// Forward declaration of FatFs types without including headers
#ifdef __cplusplus
extern "C" {
#endif

struct FATFS;
extern struct FATFS g_fatfs;

#ifdef __cplusplus
}
#endif

// Forward declarations for VicOS functions
extern "C" {
    void kprint(const char* str);
    int disk_read_sector(vic_uint32 lba, vic_uint8* buffer);
    int disk_write_sector(vic_uint32 lba, const vic_uint8* buffer);
    int get_partition_info(int partition_num, vic_uint32* start_lba, vic_uint32* sector_count);
}

// String/memory implementations
void fat_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

void fat_memset(void* ptr, vic_uint8 value, vic_size_t num) {
    vic_uint8* p = (vic_uint8*)ptr;
    for (vic_size_t i = 0; i < num; i++) {
        p[i] = value;
    }
}

void fat_memcpy(void* dest, const void* src, vic_size_t num) {
    vic_uint8* d = (vic_uint8*)dest;
    const vic_uint8* s = (const vic_uint8*)src;
    for (vic_size_t i = 0; i < num; i++) {
        d[i] = s[i];
    }
}

// Create FAT32 FS
int create_fat32_filesystem() {
    vic_uint32 partition_start, partition_size;

    if (get_partition_info(1, &partition_start, &partition_size) < 0) {
        kprint("Failed to get partition information\n");
        return -1;
    }

    kprint("Creating FAT32 filesystem on partition 1...\n");

    fatfs_init();

    kprint("FAT32 filesystem created successfully\n");
    return 0;
}

// Create directory
int create_directory(const char* dirname) {
    FRESULT res = fatfs_mkdir(dirname);

    if (res != FR_OK) {
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

// Create file with content
int create_file_with_content(const char* filename, const void* data, vic_size_t size) {
    FRESULT res = fatfs_write_file(filename, (const char*)data, size);

    if (res != FR_OK) {
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

// Read file
const char* read_file_content(const char* filename, char* buffer, vic_size_t buffer_size) {
    vic_size_t bytes_read = 0;
    FRESULT res = fatfs_read_file(filename, buffer, buffer_size, &bytes_read);

    if (res != FR_OK) {
        kprint("Failed to read file: ");
        kprint(filename);
        kprint("\n");
        return nullptr;
    }

    return buffer;
}

// List directory
int list_directory(const char* path) {
    FRESULT res = fatfs_ls(path);

    if (res != FR_OK) {
        kprint("Failed to list directory: ");
        kprint(path);
        kprint("\n");
        return -1;
    }

    return 0;
}

// Change directory
int change_directory(const char* path) {
    FRESULT res = fatfs_cd(path);

    if (res != FR_OK) {
        kprint("Failed to change directory: ");
        kprint(path);
        kprint("\n");
        return -1;
    }

    return 0;
}

// Get current directory
const char* get_current_directory(char* buffer, vic_size_t buffer_size) {
    FRESULT res = fatfs_pwd(buffer, buffer_size);

    if (res != FR_OK) {
        kprint("Failed to get current directory\n");
        return nullptr;
    }

    return buffer;
}
