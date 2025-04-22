// src/fatfs_integration.h
#ifndef FATFS_INTEGRATION_H
#define FATFS_INTEGRATION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// FatFs error codes (if not already defined elsewhere)
#ifndef FR_OK
typedef enum {
    FR_OK = 0,              // (0) Succeeded
    FR_DISK_ERR,            // (1) A hard error occurred in the low level disk I/O layer
    FR_INT_ERR,             // (2) Assertion failed
    FR_NOT_READY,           // (3) The physical drive cannot work
    FR_NO_FILE,             // (4) Could not find the file
    FR_NO_PATH,             // (5) Could not find the path
    FR_INVALID_NAME,        // (6) The path name format is invalid
    FR_DENIED,              // (7) Access denied
    // ... add more as needed
} FRESULT;
#endif

// Function declarations with unique names to avoid conflicts
void fatfs_init(void);
FRESULT fatfs_mkdir(const char* path);
FRESULT fatfs_write_file(const char* path, const char* content, size_t len);
FRESULT fatfs_read_file(const char* path, char* buffer, size_t buffer_size, size_t* bytes_read);
FRESULT fatfs_ls(const char* path);
FRESULT fatfs_cd(const char* path);
FRESULT fatfs_pwd(char* buffer, unsigned int size);
void process_fatfs_install(const char* args);

#ifdef __cplusplus
}
#endif

#endif // FATFS_INTEGRATION_H