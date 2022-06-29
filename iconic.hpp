#pragma once

#include <stdint.h>

#ifndef ICONIC_API
#define ICONIC_API
#endif

typedef  uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef   int8_t  s8;
typedef  int16_t s16;
typedef  int32_t s32;
typedef  int64_t s64;
typedef    float f32;
typedef   double f64;

ICONIC_API void iconic_generate_win32_from_file(const char* output, const char* file_name);
ICONIC_API void iconic_generate_win32_from_data(const char* output, const void* file_data, u64 file_size);
