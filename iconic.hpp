#pragma once

#include <stdint.h>

#include "iconic_stb_image_read.h"
#include "iconic_stb_image_resize.h"
#include "iconic_stb_image_write.h"

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

typedef u32 Iconic_Format;
enum Iconic_Format_
{
    ICONIC_FORMAT_NONE    =      0,
    ICONIC_FORMAT_WIN32   = 1 << 0,
    ICONIC_FORMAT_OSX     = 1 << 1,
    ICONIC_FORMAT_ANDROID = 1 << 2,
    ICONIC_FORMAT_IOS     = 1 << 3,
};

struct Iconic_Descriptor
{
    Iconic_Format icon_formats;
    const char**  inputs;
    u32           input_count;
    const char*   output;
};

ICONIC_API bool iconic_generate_icon(const Iconic_Descriptor& desc);
