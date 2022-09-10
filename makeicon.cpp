#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// We use the stb image libs for reading, resizing, and writing images for packing.
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_STATIC

#include <stb_image_resize.h>
#include <stb_image_write.h>
#include <stb_image.h>

#define MAKEICON_VERSION_MAJOR 1
#define MAKEICON_VERSION_MINOR 2

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
while(0)

#define CAST(t,x) ((t)(x))

typedef  uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint32_t u64;
typedef   int8_t  s8;
typedef  int16_t s16;
typedef  int32_t s32;
typedef  int64_t s64;
typedef    float f32;
typedef   double f64;

typedef s32 Platform;
enum Platform_
{
    Platform_Win32,
    Platform_OSX,
    Platform_iOS,
    Platform_Android,
    Platform_COUNT
};

static constexpr const char* PLATFORM_NAMES[Platform_COUNT] = { "win32", "osx", "ios", "android" };

static constexpr const char* MAKEICON_HELP_MESSAGE =
"makeicon [-resize] -sizes:x,y,z,w,... -input:x,y,z,w,... output\n"
"\n"
"    -sizes: ...  [Required]  Comma-separated input size(s) of icon image to generate for the output icon.\n"
"    -input: ...  [Required]  Comma-separated input image(s) and/or directories and/or .txt files containing file names to be used to generate the icon sizes.\n"
"    -resize      [Optional]  Whether to resize input images to match the specified sizes.\n"
"    -platform    [Optional]  Platform to generate icons for. Options are win32, osx, ios, android. Defaults to win32.\n"
"    -version     [Optional]  Prints out the current version number of the makeicon binary and exits.\n"
"    -help        [Optional]  Prints out this help/usage message for the program and exits.\n"
"     output      [Required]  The name of the icon that will be generated by the program.\n";

struct Argument
{
    std::string              name;
    std::vector<std::string> params; // Optional!
};

struct Options
{
    Platform                 platform = Platform_Win32;
    bool                     resize   = false;
    std::vector<s32>         sizes;
    std::vector<std::string> input;
    std::string              contents;
    std::string              output;
};

struct Image
{
    s32 width  = 0;
    s32 height = 0;
    s32 bpp    = 0;
    u8* data   = NULL;

    inline bool operator<(const Image& rhs) const
    {
        return ((width*height) < (rhs.width*rhs.height));
    }
};

static bool save_image(Image& image, std::string file_name, s32 resize_width = 0, s32 resize_height = 0)
{
    s32 output_width = (resize_width > 0) ? resize_width : image.width;
    s32 output_height = (resize_height > 0) ? resize_height : image.height;
    u8* output_data = image.data;

    // If the resize options were specified then resize first, this also means we need to free output_data after as we allocate new memory.
    bool free_output_memory = false;
    if(output_width != image.width || output_height != image.height)
    {
        output_data = CAST(u8*, malloc(output_width*output_height*image.bpp));
        if(!output_data)
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

    if(free_output_memory)
    {
        free(output_data);
    }

    return true;
}

static void resize_and_save_image(const std::string& filename, std::vector<Image>& input_images, s32 size, bool resize)
{
    // Search for matching image input size to save out as PNG.
    bool match_found = false;
    for(auto& image: input_images)
    {
        if(image.width == size && image.height == size)
        {
            match_found = true;
            save_image(image, filename);
            break;
        }
    }
    if(!match_found)
    {
        if(!resize)
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

static void free_image(Image& image)
{
    free(image.data);
    image.data = NULL;
}

static std::vector<u8> read_entire_binary_file(const std::string& file_name)
{
    std::ifstream file(file_name, std::ios::binary);
    std::vector<u8> content;
    content.resize(std::filesystem::file_size(file_name));
    file.read(CAST(char*, &content[0]), content.size()*sizeof(u8));
    return content;
}

static void tokenize_string(const std::string& str, const char* delims, std::vector<std::string>& tokens)
{
    size_t prev = 0;
    size_t pos;

    while((pos = str.find_first_of(delims, prev)) != std::string::npos)
    {
        if(pos > prev) tokens.push_back(str.substr(prev, pos-prev));
        prev = pos+1;
    }
    if(prev < str.length())
    {
        tokens.push_back(str.substr(prev, std::string::npos));
    }
}

static void print_version_message()
{
    fprintf(stdout, "makeicon v%d.%d\n", MAKEICON_VERSION_MAJOR, MAKEICON_VERSION_MINOR);
}

static void print_help_message()
{
    fprintf(stdout, "%s\n", MAKEICON_HELP_MESSAGE);
}

static Argument format_argument(std::string arg_str)
{
    // @Improve: Handle ill-formed argument error cases!

    // Remove the '-' character from the argument.
    arg_str.erase(0,1);
    // Split the argument into its name and (optional) parameters.
    std::vector<std::string> tokens;
    tokenize_string(arg_str, ":", tokens);
    // Store the formatted argument information.
    Argument arg;
    if(!tokens.empty())
    {
        arg.name = tokens[0];
        if(tokens.size() > 1) // If we have parameters!
        {
            tokenize_string(tokens[1], ",", arg.params);
        }
    }
    return arg;
}

static s32 make_icon_win32(const Options& options, std::vector<Image>& input_images);
static s32 make_icon_android(const Options& options, std::vector<Image>& input_images);
static s32 make_icon_apple(const Options& options, std::vector<Image>& input_images);

static s32 make_icon(const Options& options)
{
    std::vector<Image> input_images;

    // Load all of the input images into memory.
    for(auto& file_name: options.input)
    {
        Image image;
        image.data = stbi_load(file_name.c_str(), &image.width,&image.height,&image.bpp,4); // We force to 4-channel RGBA.
        image.bpp = 4;
        if(!image.data)
        {
            ERROR("Failed to load input image: %s", file_name.c_str());
        }
        else
        {
            // We warn about non-square images as they will be stretched to a square aspect.
            if(image.width != image.height)
            {
                WARNING("Image file '%s' is not square and will be stretched! Consider changing its size.", file_name.c_str());
            }
            // We warn if two images are passed in with the same size.
            for(auto& input: input_images)
            {
                if((input.width == image.width) && (input.height == image.height))
                {
                    WARNING("Two provided image files have the same siize of %dx%d! It is ambiguous which one will be used.", image.width,image.height);
                    break;
                }
            }

            input_images.push_back(image);
        }
    }

    s32 result = EXIT_FAILURE;

    // Run the icon generation code for the desired platform.
    switch(options.platform)
    {
        case Platform_Win32:
        {
            result = make_icon_win32(options, input_images);
        } break;
        case Platform_OSX:
        case Platform_iOS:
        {
            result = make_icon_apple(options, input_images);
        } break;
        case Platform_Android:
        {
            result = make_icon_android(options, input_images);
        } break;
        default:
        {
            WARNING("Unknown platform ID used for making icon: %d", options.platform);
        } break;
    }

    // Free all of the loaded to avoid memory leaking.
    for(auto& image: input_images)
    {
        free_image(image);
    }

    return result;
}

int main(int argc, char** argv)
{
    Options options;

    // Parse command line arguments given to the program, if there are not
    // any arguments provided then it makes sense to print the help message.
    // This parsing process populates the program options structure with
    // information needed in order to perform the desired program task.
    if(argc <= 1)
    {
        print_help_message();
        return EXIT_SUCCESS;
    }
    else
    {
        for(s32 i=1; i<argc; ++i)
        {
            // Handle options.
            if(argv[i][0] == '-')
            {
                Argument arg = format_argument(argv[i]);
                if(arg.name == "resize")
                {
                    options.resize = true;
                }
                else if(arg.name == "sizes")
                {
                    for(auto& param: arg.params)
                    {
                        // allows sizes to be specified through a json file (used for apple icon generation)
                        if(param.find(".json") != -1) options.contents = param;
                        else options.sizes.push_back(std::stoi(param));
                    }
                    if(options.sizes.empty() && options.contents.empty())
                    {
                        ERROR("No sizes provided with -sizes argument!");
                    }
                }
                else if(arg.name == "input")
                {
                    for(auto& param: arg.params)
                    {
                        if(std::filesystem::is_directory(param))
                        {
                            for(auto& p: std::filesystem::directory_iterator(param))
                            {
                                if(std::filesystem::is_regular_file(p))
                                {
                                    options.input.push_back(p.path().string());
                                }
                            }
                        }
                        else
                        {
                            if(std::filesystem::path(param).extension() == ".txt")
                            {
                               // If it's a text file we read each line and add those as file names for input.
                                std::ifstream file(param);
                                if(!file.is_open())
                                {
                                    ERROR("Failed to read .txt file passed in as input: %s", param.c_str());
                                }
                                else
                                {
                                    std::string line;
                                    while(getline(file, line))
                                    {
                                        if(std::filesystem::is_regular_file(line))
                                        {
                                            options.input.push_back(line);
                                        }
                                    }
                                }
                            }
                            else
                            {
                                options.input.push_back(param);
                            }
                        }
                    }
                    if(options.input.empty())
                    {
                        ERROR("No input provided with -input argument!");
                    }
                }
                else if(arg.name == "platform")
                {
                    std::string platform = arg.params[0];
                    for(Platform i=0; i<Platform_COUNT; ++i)
                    {
                        if(platform == PLATFORM_NAMES[i])
                        {
                            options.platform = i;
                            break;
                        }
                    }
                }
                else if(arg.name == "version")
                {
                    print_version_message();
                    return EXIT_SUCCESS;
                }
                else if(arg.name == "help")
                {
                    print_help_message();
                    return EXIT_SUCCESS;
                }
                else
                {
                    ERROR("Unknown argument: %s", arg.name.c_str());
                }
            }
            else // Handle output.
            {
                // If there are still arguments/options after the final output name parameter then we consider
                // the input ill-formed and we inform the user of how to format the arguments to the program.
                if(i < (argc-1))
                {
                    ERROR("Extra arguments after final '%s' parameter!", argv[i]);
                }
                else
                {
                    options.output = argv[i];
                }
            }
        }
    }

    // Ensure the options structure is populated with valid data. If there are no inputs, sizes, or ouput we cannot run!
    if(options.sizes.empty() && options.contents.empty())
        ERROR("No icon sizes provided! Specify sizes using: -sizes:x,y,z,w...");
    if(options.input.empty())
        ERROR("No input images provided! Specify input using: -input:x,y,z,w...");
    if(options.output.empty())
        ERROR("No output name provded! Specify output name like so: makeicon ... outputname.ico");

    // The maximum size allows in an ICO file is 256x256! We also check for 0 or less as that would not be valid either...
    for(auto& size: options.sizes)
    {
        if(size > 256)
            ERROR("Invalid icon size '%d'! Maximum value allowed is 256 pixels.", size);
        if(size <= 0)
            ERROR("Invalid icon size '%d'! Minimum value allowed is 1 pixel.", size);
    }

    // Sort the input images from largest to smallest.
    std::sort(options.input.begin(), options.input.end());

    // Takes the populated options structure and uses those options to generate an icon for the desired platform.
    return make_icon(options);
}

//
// Windows
//

// Windows ICO File Format: https://en.wikipedia.org/wiki/ICO_(file_format)#Outline

typedef u16 ImageType;
enum ImageType_
{
    ImageType_Ico = 1,
    ImageType_Cur = 2
};

#pragma pack(push,1)
struct IconDir
{
    u16 reserved;
    u16 type;
    u16 num_images;
};
#pragma pack(pop)

#pragma pack(push,1)
struct IconDirEntry
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

s32 make_icon_win32(const Options& options, std::vector<Image>& input_images)
{
    // Make sure the temporary directory, where we store all the icon PNGs, actually exists.
    std::filesystem::path temp_directory = "makeicon_temp/";
    if(!std::filesystem::exists(temp_directory))
    {
        std::filesystem::create_directory(temp_directory);
    }

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
    for(auto& size: options.sizes)
    {
        std::string temp_file_name = (temp_directory / (std::to_string(size) + ".png")).string();
        resize_and_save_image(temp_file_name, input_images, size, options.resize);
    }

    // Now we have all icons saved to the temporary directory we can package them into a final ICO file.

    // Header
    IconDir icon_header;
    icon_header.reserved = 0;
    icon_header.type = ImageType_Ico;
    icon_header.num_images = CAST(u16, options.sizes.size());

    // Directory
    size_t offset = sizeof(IconDir) + (sizeof(IconDirEntry) * options.sizes.size());
    std::vector<IconDirEntry> icon_directory;
    for(auto& size: options.sizes)
    {
        std::string temp_file_name = (temp_directory / (std::to_string(size) + ".png")).string();
        IconDirEntry icon_dir_entry;
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
    if(!output.is_open())
    {
        ERROR("Failed to save output file: %s", options.output.c_str());
    }
    else
    {
        output.write((CAST(char*, &icon_header)), sizeof(icon_header));
        for(auto& dir_entry: icon_directory)
        {
            output.write((CAST(char*, &dir_entry)), sizeof(dir_entry));
        }
        for(auto& size: options.sizes)
        {
            std::string temp_file_name = (temp_directory / (std::to_string(size) + ".png")).string();
            auto data = read_entire_binary_file(temp_file_name);
            output.write(CAST(char*, &data[0]), data.size());
        }
    }

    // The temporary directory is deleted.
    std::filesystem::remove_all(temp_directory);

    return EXIT_SUCCESS;
}

//
// Android
//

s32 make_icon_android(const Options& options, std::vector<Image>& input_images)
{
    // Android needs specific downsampled sizes for thumbnails.

    s32 sizes[5];
    sizes[0] = options.sizes[0];
    sizes[1] = (sizes[0] / 2) + (sizes[0] / 4);
    sizes[2] = (sizes[0] / 2);
    sizes[3] = (sizes[0] / 4) + (sizes[0] / 8);
    sizes[4] = (sizes[0] / 4);

    const char* directories[5] =
    {
        "mipmap-xxxhdpi/",
        "mipmap-xxhdpi/",
        "mipmap-xhdpi/",
        "mipmap-hdpi/",
        "mipmap-mdpi/"
    };

    // Create output directory.
    std::filesystem::path output_directory = options.output;
    if(!std::filesystem::exists(output_directory))
    {
        std::filesystem::create_directory(output_directory);
    }

    for(s32 i=0; i<5; ++i)
    {
        std::filesystem::path directory = options.output + "/" + directories[i];
        if(!std::filesystem::exists(directory))
        {
            std::filesystem::create_directory(directory);
        }
        resize_and_save_image(directory.string() + "ic_launcher.png", input_images, sizes[i], options.resize);
    }

    return EXIT_SUCCESS;
}

//
// Apple
//

s32 make_icon_apple(const Options& options, std::vector<Image>& input_images)
{
    if(options.contents.empty())
    {
        ERROR("No contents json file specified! Specify contents file using: -sizes:Contents.json...");
    }

    // Read in JSON contents file that specifies the required output images.
    FILE* file = fopen(options.contents.c_str(), "rb");
    if(!file) ERROR("Failed to open contents file!");

    fseek(file, 0L, SEEK_END);
    size_t len = CAST(size_t,ftell(file));

    fseek(file, 0L, SEEK_SET);

    char* buf = CAST(char*, malloc((len+1)*sizeof(char)));
    if(!buf) ERROR("Failed to allocate file buffer for JSON!");

    buf[len] = '\0';

    fread(buf, 1, len, file);

    fclose(file);

    std::stringstream ss(buf);
    std::string to;

    // Create output directory.
    std::filesystem::path output_directory = options.output;
    if(!std::filesystem::exists(output_directory))
    {
        std::filesystem::create_directory(output_directory);
    }

    // Iterate over the lines of json and find parameters for resizing and saving the images.
    std::string filename = "";
    f32 scale = 0;
    f32 size = 0;
    while(getline(ss, to, '\n'))
    {
        s32 start = to.find(": \"") + 3;

        if(to.find("filename") != -1)
        {
            s32 end = to.find("\",");
            filename = to.substr(start, (end - start));
        }
        else if(to.find("scale") != -1)
        {
            s32 end = to.find("x");
            scale = std::stof(to.substr(start, (end - start)));
        }
        else if(to.find("size") != -1)
        {
            s32 end = to.find("x");
            size = std::stof(to.substr(start, (end - start)));
        }

        // Erase all stored parameters on hitting the end of the json object.
        if(to.find('}') != -1)
        {
            filename = "";
            size = 0;
            scale = 0;
        }

        // Once all parameters are filled write out an image and reset.
        if(!filename.empty() && scale && size)
        {
            resize_and_save_image(options.output + "/" + filename, input_images, size * scale, options.resize);

            filename = "";
            scale = 0;
            size = 0;
        }
    }

    free(buf);

    // Copy the contents file to the output directory so all data is packaged together.
    std::string outputContentsPath = options.output + "/Contents.json";
    if(options.contents != outputContentsPath)
        std::filesystem::copy(options.contents, outputContentsPath);

    return EXIT_SUCCESS;
}