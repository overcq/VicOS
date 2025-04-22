// src/disk_interface.h
#ifndef DISK_INTERFACE_H
#define DISK_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

// Functions that need to be implemented by the OS
int disk_read_sector(unsigned int lba, unsigned char* buffer);
int disk_write_sector(unsigned int lba, const unsigned char* buffer);
int get_partition_info(int partition_num, unsigned int* start_lba, unsigned int* sector_count);
void kprint(const char* str);

#ifdef __cplusplus
}
#endif

#endif // DISK_INTERFACE_H