#include "iconic.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// Windows ICO File Format: https://en.wikipedia.org/wiki/ICO_(file_format)#Outline

typedef u16 Ico_Image_Type;
enum Ico_Image_Type_
{
    ICO_IMAGE_TYPE_ICO = 1,
    ICO_IMAGE_TYPE_CUR = 2,
};

#pragma pack(push,1)
struct Ico_Header
{
    u16 reserved;
    u16 type;
    u16 num_images;
};
#pragma pack(pop)

#pragma pack(push,1)
struct Ico_Entry
{
    u8  width;
    u8  height;
    u8  num_colors;
    u8  reserved;
    u16 color_planes;
    u16 bpp;
    u32 size;
    u32 offset;
};
#pragma pack(pop)

static void* read_entire_file(const char* file_name, u64& size)
{
    assert(size);
    FILE* file = fopen(file_name, "rb");
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);
    void* data = malloc(size);
    assert(data);
    fread(data, size, 1, file);
    fclose(file);
    return data;
}

ICONIC_API void iconic_generate_win32_from_file(const char* output, const char* file_name)
{
    // Load our input image.
    u64 file_size;
    void* file_data = read_entire_file(file_name, file_size);
    iconic_generate_win32_from_data(output, file_data, file_size);
}

ICONIC_API void iconic_generate_win32_from_data(const char* output, const void* file_data, u64 file_size)
{
    Ico_Header header = {};
    header.type = ICO_IMAGE_TYPE_ICO;
    header.num_images = 1; // @Incomplee: We will support more later...

    Ico_Entry entry;
    entry.width = 0; // @Incomplete: Assume 256...
    entry.height = 0; // @Incomplete: Assume 256...
    entry.size = file_size;
    entry.offset = sizeof(header) + sizeof(entry);

    FILE* icon_file = fopen(output, "wb");
    assert(icon_file);

    fwrite(&header, sizeof(header), 1, icon_file);
    fwrite(&entry, sizeof(entry), 1, icon_file);
    fwrite(file_data, file_size, 1, icon_file);

    fclose(icon_file);
}
