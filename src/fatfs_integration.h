#ifndef FATFS_INTEGRATION_H
#define FATFS_INTEGRATION_H

#include <stddef.h>
#include <stdint.h>
#include "vstdint.h"

#ifdef __cplusplus
extern "C" {
#endif

// FatFs result type
typedef enum {
    FR_OK = 0,
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    // Add more as needed
} FRESULT;

// Function declarations
void fatfs_init(void);
FRESULT fatfs_mkdir(const char* path);
FRESULT fatfs_write_file(const char* path, const char* content, vic_size_t len);
FRESULT fatfs_read_file(const char* path, char* buffer, vic_size_t buffer_size, vic_size_t* bytes_read);
FRESULT fatfs_ls(const char* path);
FRESULT fatfs_cd(const char* path);
FRESULT fatfs_pwd(char* buffer, vic_size_t size);
void process_fatfs_install(const char* args);

#ifdef __cplusplus
}
#endif

#endif // FATFS_INTEGRATION_H
