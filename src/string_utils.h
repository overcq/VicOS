// src/string_utils.h
#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int snprintf(char* str, vic_size_t size, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif // STRING_UTILS_H