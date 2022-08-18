// Windows ICO File Format:
//  https://en.wikipedia.org/wiki/ICO_(file_format)#Outline
#include "mi_utility.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

enum Image_Type: u16
{
    IMAGE_TYPE_ICO=1,
    IMAGE_TYPE_CUR=2
};

#pragma pack(push,1)
struct Icon_Dir
{
    u16 reserved;
    u16 type;
    u16 num_images;
};
#pragma pack(pop)

#pragma pack(push,1)
struct Icon_Dir_Entry
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

struct Image
{
    int width = 0, height = 0, bpp = 0;
    u8* data = NULL;
    inline bool operator< (const Image& rhs) const
    {
        return ((width*height) < (rhs.width*rhs.height));
    }
};

static bool save_image (Image& image, std::string file_name, int resize_width=0, int resize_height=0)
{
    int output_width = (resize_width > 0) ? resize_width : image.width;
    int output_height = (resize_height > 0) ? resize_height : image.height;
    u8* output_data = image.data;
    // If the resize options were specified then resize first, this also means we need to free output_data after as we allocate new memory.
    bool free_output_memory = false;
    if (output_width != image.width || output_height != image.height)
    {
        output_data = CAST(u8*, malloc(output_width*output_height*image.bpp));
        if (!output_data)
        {
            return false;
        }
        else
        {
            stbir_resize_uint8_srgb(image.data, image.width, image.height, image.width*image.bpp,
                output_data, output_width, output_height, output_width*image.bpp, image.bpp, 3,0);
            free_output_memory = true;
        }
    }
    stbi_write_png(file_name.c_str(), output_width, output_height, image.bpp, output_data, output_width*image.bpp);
    if (free_output_memory)
    {
        free(output_data);
    }
    return true;
}

static void free_image (Image& image)
{
    free(image.data);
    image.data = NULL;
}

void resize_and_save_image(const std::string& filename, std::vector<Image>& input_images, int size, bool resize)
{
    // Search for matching image input size to save out as PNG.
    bool match_found = false;
    for (auto& image: input_images)
    {
        if (image.width == size && image.height == size)
        {
            match_found = true;
            save_image(image, filename);
            break;
        }
    }
    if (!match_found)
    {
        if (!resize)
        {
            // If no match was found and resize wasn't specified then we fail.
            ERROR("Size %d was specified but no input image of this size was provided! Potentially specify -resize to allow for reszing to this size.", size);
        }
        else
        {
            // If no match was found and resize was specified then we resize for this icon size (use the largest image).
            save_image(input_images.at(input_images.size()-1), filename, size,size);
        }
    }
}

int make_icon_win32 (const Options& options, std::vector<Image>& input_images)
{
    // Make sure the temporary directory, where we store all the icon PNGs, actually exists.
    std::filesystem::path temp_directory = "makeicon_temp/";
    if (!std::filesystem::exists(temp_directory))
    {
        std::filesystem::create_directory(temp_directory);
    }
    // The temporary directory is deleted when we finish execution.
    DEFER { std::filesystem::remove_all(temp_directory); };

    // Save out images to the temporary directroy at the correct sizes the user wants for their icon.
    //
    // The reason we do this is because we accept images in a variety of different formats and the
    // ICO file format only accepts BMP or PNG files, furthermore, using BMPs requires extra work in
    // order to store them in an ICO (stripping the header, etc.) so we just convert all images to
    // these temporary PNGs to be directly embedded into the ICO file without any futher processing.
    //
    // If an imput image directly matches a desired size to be emebedded into the ICO then it is copied
    // directly to the folder, otherwise a fatal error occurs. However, if the resize options was
    // specified then the largest size input image is resized to the desired size first, before copying.
    for (auto& size: options.sizes)
    {
        std::string temp_file_name = (temp_directory / (std::to_string(size) + ".png")).string();
        resize_and_save_image(temp_file_name, input_images, size, options.resize);
    }

    // Now we have all icons saved to the temporary directory we can package them into a final ICO file.

    // Header
    Icon_Dir icon_header;
    icon_header.reserved = 0;
    icon_header.type = IMAGE_TYPE_ICO;
    icon_header.num_images = CAST(u16, options.sizes.size());

    // Directory
    size_t offset = sizeof(Icon_Dir) + (sizeof(Icon_Dir_Entry) * options.sizes.size());
    std::vector<Icon_Dir_Entry> icon_directory;
    for (auto& size: options.sizes)
    {
        std::string temp_file_name = (temp_directory / (std::to_string(size) + ".png")).string();
        Icon_Dir_Entry icon_dir_entry;
        icon_dir_entry.width = CAST(u8, size); // Values of 256 (the max) will turn into 0 on cast, which is what the ICO spec wants.
        icon_dir_entry.height = CAST(u8, size);
        icon_dir_entry.num_colors = 0;
        icon_dir_entry.reserved = 0;
        icon_dir_entry.color_planes = 0;
        icon_dir_entry.bpp = 4*8; // We force to 4-channel RGBA!
        icon_dir_entry.size = CAST(u32, std::filesystem::file_size(temp_file_name));
        icon_dir_entry.offset = CAST(u32, offset);
        icon_directory.push_back(icon_dir_entry);
        offset += icon_dir_entry.size;
    }

    // Save
    std::ofstream output(options.output, std::ios::binary|std::ios::trunc);
    if (!output.is_open())
    {
        ERROR("Failed to save output file: %s", options.output.c_str());
    }
    else
    {
        output.write((CAST(char*, &icon_header)), sizeof(icon_header));
        for (auto& dir_entry: icon_directory)
        {
            output.write((CAST(char*, &dir_entry)), sizeof(dir_entry));
        }
        for (auto& size: options.sizes)
        {
            std::string temp_file_name = (temp_directory / (std::to_string(size) + ".png")).string();
            auto data = read_entire_binary_file(temp_file_name);
            output.write(CAST(char*, &data[0]), data.size());
        }
    }

    return EXIT_SUCCESS;
}

int make_icon_apple(const Options& options, std::vector<Image>& input_images)
{
    if(options.contents.empty()) ERROR("No contents json file specified! Specify contents file using: -sizes:Contents.json...");
    
    // read in json contents file that specifies the required output images
    FILE* file = fopen(options.contents.c_str(), "rb");
    
    if (!file) ERROR("Failed to open contents file!");
    
    fseek(file, 0L, SEEK_END);
    size_t len = (size_t)ftell(file);
    
    fseek(file, 0L, SEEK_SET);
    
    char* buf = new char[len + 1];
    buf[len] = '\0';
    
    fread(buf, 1, len, file);
    
    fclose(file);
    
    std::stringstream ss(buf);
    std::string to;
    
    // create output directory
    std::filesystem::path output_directory = options.output;
    if(!std::filesystem::exists(output_directory))
    {
        std::filesystem::create_directory(output_directory);
    }
    
    // iterate over the lines of json and find parameters for resizing and saving the images
    std::string filename = "";
    int scale = 0;
    int size = 0;
    while(getline(ss, to, '\n'))
    {
        int start = to.find(": \"") + 3;
        
        if(to.find("filename") != -1)
        {
            int end = to.find("\",");
            filename = to.substr(start, (end - start));
        }
        else if(to.find("scale") != -1)
        {
            int end = to.find("x");
            scale = std::stoi(to.substr(start, (end - start)));
        }
        else if (to.find("size") != -1)
        {
            int end = to.find("x");
            size = std::stoi(to.substr(start, (end - start)));
        }
        
        // erase all stored parameters on hitting the end of the json object
        if(to.find('}') != -1)
        {
            filename = "";
            size = 0;
            scale = 0;
        }
        
        // once all parameters are filled write out an image and reset
        if(!filename.empty() && scale && size)
        {
            resize_and_save_image(options.output + "/" + filename, input_images, size * scale, options.resize);
            
            filename = "";
            scale = 0;
            size = 0;
        }
    }
    delete[] buf;
    
    // copy the contents file to the output directory so all data is packaged together
    std::string outputContentsPath = options.output + "/Contents.json";
    if(options.contents != outputContentsPath)
        std::filesystem::copy(options.contents, outputContentsPath);
    
    return EXIT_SUCCESS;
}

int make_icon_android(const Options& options, std::vector<Image>& input_images)
{
    // android needs specific downsampled sizes for thumbnails
    
    int sizes[5];
    sizes[0] = options.sizes[0];
    sizes[1] = (sizes[0] / 2) + (sizes[0] / 4);
    sizes[2] = (sizes[0] / 2);
    sizes[3] = (sizes[0] / 4) + (sizes[0] / 8);
    sizes[4] = (sizes[0] / 4);
    
    const char* directories[5] = {
        "mipmap-xxxhdpi/",
        "mipmap-xxhdpi/",
        "mipmap-xhdpi/",
        "mipmap-hdpi/",
        "mipmap-mdpi/"
    };
    
    // create output directory
    std::filesystem::path output_directory = options.output;
    if(!std::filesystem::exists(output_directory))
    {
        std::filesystem::create_directory(output_directory);
    }
    
    for(int i = 0; i < 5; ++i)
    {
        std::filesystem::path directory = options.output + "/" + directories[i];
        if (!std::filesystem::exists(directory))
        {
            std::filesystem::create_directory(directory);
        }
        resize_and_save_image(directory.string() + "ic_launcher.png", input_images, sizes[i], options.resize);
    }
    
    return EXIT_SUCCESS;
}

int make_icon(const Options& options)
{
    // Free all of the loaded images on scope exit to avoid memory leaking.
    std::vector<Image> input_images;
    DEFER { for (auto& image: input_images) free_image(image); };

    // Load all of the input images into memory.
    for (auto& file_name: options.input)
    {
        Image image;
        image.data = stbi_load(file_name.c_str(), &image.width,&image.height,&image.bpp,4); // We force to 4-channel RGBA.
        image.bpp = 4;
        if (!image.data)
        {
            ERROR("Failed to load input image: %s", file_name.c_str());
        }
        else
        {
            // We warn about non-square images as they will be stretched to a square aspect.
            if (image.width != image.height)
            {
                WARNING("Image file '%s' is not square and will be stretched! Consider changing its size.", file_name.c_str());
            }
            // We warn if two images are passed in with the same size.
            for (auto& input: input_images)
            {
                if ((input.width == image.width) && (input.height == image.height))
                {
                    WARNING("Two provided image files have the same siize of %dx%d! It is ambiguous which one will be used.", image.width,image.height);
                    break;
                }
            }

            input_images.push_back(image);
        }
    }
    
    switch(options.platform)
    {
        case e_platform::win32:
            return make_icon_win32(options, input_images);
            break;
        case e_platform::osx:
        case e_platform::ios:
            return make_icon_apple(options, input_images);
            break;
        case e_platform::android:
            return make_icon_android(options, input_images);
            break;
        case e_platform::COUNT:
            break;
    }
    return EXIT_FAILURE;
}
