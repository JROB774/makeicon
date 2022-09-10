#pragma once

// We use the stb image libs for reading, resizing, and writing images for packing.
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_STATIC
#include "stb/stb_image_resize.h"
#include "stb/stb_image_write.h"
#include "stb/stb_image.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>

#define JOIN( a, b) JOIN2(a, b)
#define JOIN2(a, b) JOIN1(a, b)
#define JOIN1(a, b) a##b

#define CAST(t,x) ((t)(x))

// C++ implementation of defer functionality. This can be used to defer blocks of
// code to be executed during exit of the current scope. Useful for freeing any
// resources that have been allocated without worrying about multiple paths or
// having to deal with C++'s RAII model for implementing this kind of behaviour.

// If we have __COUNTER__ then is is preferred over __LINE__ as it's more robust.
#ifdef __COUNTER__
#define DEFER const auto& JOIN(defer, __COUNTER__) = Defer_Help() + [&]()
#else
#define DEFER const auto& JOIN(defer, __LINE__) = Defer_Help() + [&]()
#endif

template<typename T>
struct Defer_Type
{
    T lambda;
    Defer_Type (T lambda): lambda(lambda) {}
   ~Defer_Type () { lambda(); }
    Defer_Type& operator= (const Defer_Type& d) = delete;
    Defer_Type (const Defer_Type& d) = delete;
};
struct Defer_Help
{
    template<typename T>
    Defer_Type<T> operator+ (T type) { return type; }
};

typedef  uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint32_t u64;
typedef   int8_t  s8;
typedef  int16_t s16;
typedef  int32_t s32;
typedef  int64_t s64;

namespace e_platform
{
    enum platform
    {
        win32,
        osx,
        ios,
        android,
        COUNT
    };
}
typedef e_platform::platform Platform;

const std::string platform_names[e_platform::COUNT] =
{
    "win32",
    "osx",
    "ios",
    "android"
};

struct Argument
{
    std::string name;
    std::vector<std::string> params; // Optional!
};

struct Options
{
    std::vector<int> sizes;
    std::vector<std::string> input;
    std::string output;
    std::string contents;
    bool resize = false;
    Platform platform = e_platform::win32;
};

#define ERROR(...)                       \
do                                       \
{                                        \
fprintf(stderr, "[makeicon] error: ");   \
fprintf(stderr, __VA_ARGS__);            \
fprintf(stderr, "\n");                   \
abort();                                 \
}                                        \
while (0)
#define WARNING(...)                     \
do                                       \
{                                        \
fprintf(stderr, "[makeicon] warning: "); \
fprintf(stderr, __VA_ARGS__);            \
fprintf(stderr, "\n");                   \
}                                        \
while (0)

static std::vector<u8> read_entire_binary_file (std::string file_name)
{
    std::ifstream file(file_name, std::ios::binary);
    std::vector<u8> content;
    content.resize(std::filesystem::file_size(file_name));
    file.read(CAST(char*, &content[0]), content.size()*sizeof(u8));
    return content;
}

static void tokenize_string (const std::string& str, const char* delims, std::vector<std::string>& tokens)
{
    size_t prev = 0;
    size_t pos;

    while ((pos = str.find_first_of(delims, prev)) != std::string::npos)
    {
        if (pos > prev) tokens.push_back(str.substr(prev, pos-prev));
        prev = pos+1;
    }
    if (prev < str.length())
    {
        tokens.push_back(str.substr(prev, std::string::npos));
    }
}
