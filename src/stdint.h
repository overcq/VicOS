#ifndef _STDINT_H
#define _STDINT_H

// Ensure these types are only defined if not already defined elsewhere
#ifndef int8_t
typedef signed char        int8_t;
#endif

#ifndef int16_t
typedef short              int16_t;
#endif

#ifndef int32_t
typedef int                int32_t;
#endif

#ifndef int64_t
typedef long long          int64_t;
#endif

#ifndef uint8_t
typedef unsigned char      uint8_t;
#endif

#ifndef uint16_t
typedef unsigned short     uint16_t;
#endif

#ifndef uint32_t
typedef unsigned int       uint32_t;
#endif

#ifndef uint64_t
typedef unsigned long long uint64_t;
#endif

// Custom types based on pointer sizes (replace 'long' and 'unsigned long' with appropriate types)
#ifndef intptr_t
typedef int               intptr_t;
#endif

#ifndef uintptr_t
typedef unsigned int      uintptr_t;
#endif

#ifndef intmax_t
typedef long long         intmax_t;
#endif

#ifndef uintmax_t
typedef unsigned long long uintmax_t;
#endif

#endif // _STDINT_H
