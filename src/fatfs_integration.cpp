// fatfs_integration.cpp
#include "ff.h"
#include "diskio.h"
#include <cstring>
#include <cstdio>

extern "C" {
    int disk_read_sector(unsigned long long sector, unsigned char* buffer);
    int disk_write_sector(unsigned long long sector, const unsigned char* buffer);
    unsigned long long get_partition_info(); // Returns total number of sectors
}

// Buffer sector size
#define SECTOR_SIZE 512

// Return disk status
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return 0; // Disk initialized
}

// Initialize disk
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return 0; // Success
}

// Read sectors
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;

    for (UINT i = 0; i < count; ++i) {
        if (disk_read_sector(sector + i, buff + (i * SECTOR_SIZE)) != 0) {
            return RES_ERROR;
        }
    }

    return RES_OK;
}

// Write sectors
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;

    for (UINT i = 0; i < count; ++i) {
        if (disk_write_sector(sector + i, buff + (i * SECTOR_SIZE)) != 0) {
            return RES_ERROR;
        }
    }

    return RES_OK;
}

// Disk I/O control
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv != 0) return RES_PARERR;

    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK; // Assume data is always flushed
        case GET_SECTOR_SIZE:
            *(WORD*)buff = SECTOR_SIZE;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1; // Erase block size in sectors (can tune later)
            return RES_OK;
        case GET_SECTOR_COUNT:
            *(LBA_t*)buff = get_partition_info();
            return RES_OK;
        default:
            return RES_PARERR;
    }
}
