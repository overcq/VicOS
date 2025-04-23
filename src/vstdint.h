#ifndef VIC_STDINT_H
#define VIC_STDINT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

// Exact-width types
typedef __INT8_TYPE__        vic_int8;
typedef __INT16_TYPE__       vic_int16;
typedef __INT32_TYPE__       vic_int32;
typedef __INT64_TYPE__       vic_int64;

typedef __UINT8_TYPE__       vic_uint8;
typedef __UINT16_TYPE__      vic_uint16;
typedef __UINT32_TYPE__      vic_uint32;
typedef __UINT64_TYPE__      vic_uint64;

// Pointer types
typedef __INTPTR_TYPE__      vic_intptr;
typedef __UINTPTR_TYPE__     vic_uintptr;

// Max types
typedef __INTMAX_TYPE__      vic_intmax;
typedef __UINTMAX_TYPE__     vic_uintmax;

// Least types (guaranteed minimum widths)
typedef __INT_LEAST8_TYPE__   vic_int_least8;
typedef __INT_LEAST16_TYPE__  vic_int_least16;
typedef __INT_LEAST32_TYPE__  vic_int_least32;
typedef __INT_LEAST64_TYPE__  vic_int_least64;

typedef __UINT_LEAST8_TYPE__  vic_uint_least8;
typedef __UINT_LEAST16_TYPE__ vic_uint_least16;
typedef __UINT_LEAST32_TYPE__ vic_uint_least32;
typedef __UINT_LEAST64_TYPE__ vic_uint_least64;

// Fast types (minimum width, fastest for the platform)
typedef __INT_FAST8_TYPE__    vic_int_fast8;
typedef __INT_FAST16_TYPE__   vic_int_fast16;
typedef __INT_FAST32_TYPE__   vic_int_fast32;
typedef __INT_FAST64_TYPE__   vic_int_fast64;

typedef __UINT_FAST8_TYPE__   vic_uint_fast8;
typedef __UINT_FAST16_TYPE__  vic_uint_fast16;
typedef __UINT_FAST32_TYPE__  vic_uint_fast32;
typedef __UINT_FAST64_TYPE__  vic_uint_fast64;

// Size and difference types
typedef __SIZE_TYPE__         vic_size_t;
typedef __PTRDIFF_TYPE__      vic_ptrdiff_t;

#ifdef __cplusplus
}
#endif

#endif // VIC_STDINT_H
