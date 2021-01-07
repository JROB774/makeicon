#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

// We use the stb image libs for reading, resizing, and writing images for packing.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_STATIC
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "stb/stb_image_resize.h"

typedef  uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;

// C++ implementation of defer functionality. This can be used to defer blocks of
// code to be executed during exit of the current scope. Useful for freeing any
// resources that have been allocated without worrying about multiple paths or
// having to deal with C++'s RAII model for implementing this kind of behaviour.

#define DeferJoin( a, b) DeferJoin2(a, b)
#define DeferJoin2(a, b) DeferJoin1(a, b)
#define DeferJoin1(a, b) a##b

#ifdef __COUNTER__
#define Defer \
const auto& DeferJoin(defer, __COUNTER__) = DeferHelp() + [&]()
#else
#define Defer \
const auto& DeferJoin(defer, __LINE__) = DeferHelp() + [&]()
#endif

template<typename T>
struct DeferType
{
    T lambda;

    DeferType (T lambda): lambda(lambda) {}
   ~DeferType () { lambda(); }

    // No copy!
    DeferType& operator= (const DeferType& d) = delete;
    DeferType (const DeferType& d) = delete;
};
struct DeferHelp
{
    template<typename T>
    DeferType<T> operator+ (T type) { return type; }
};

////////////////////////////////////////////////////////////////////////////////

#define FATAL_ERROR(msg)           \
do                                 \
{                                  \
std::cerr << "[makeicon] error: "; \
std::cerr << msg << std::endl;     \
return EXIT_FAILURE;               \
}                                  \
while (0)

enum class ImageType: U16 { ICO=1, CUR=2 };

// Windows ICO File Format:
//  https://en.wikipedia.org/wiki/ICO_(file_format)#Outline
#pragma pack(push,1)
struct IconDir
{
    U16 reserved;
    U16 type;
    U16 numImages;
};
struct IconDirEntry
{
    U8  width;
    U8  height;
    U8  numColors;
    U8  reserved;
    U16 colorPlanes;
    U16 bpp;
    U32 size;
    U32 offset;
};
#pragma pack(pop)

struct Image
{
    int width = 0, height = 0, bpp = 0;
    U8* data = NULL;

    inline bool operator< (const Image& rhs) const
    {
        return (width*height) < (rhs.width*rhs.height);
    }
    inline bool Save (std::string fileName, int resizeWidth=0, int resizeHeight=0)
    {
        int outputWidth = width, outputHeight = height;
        U8* outputData = data;
        if (resizeWidth > 0) outputWidth = resizeWidth;
        if (resizeHeight > 0) outputHeight = resizeHeight;
        // If the resize options were specified then resize first, this also means we need to free outputData after as we allocate new memory.
        bool freeOutputMemory = false;
        if (outputWidth != width || outputHeight != height) {
            outputData = static_cast<U8*>(malloc(outputWidth*outputHeight*bpp));
            if (!outputData) {
                return false;
            } else {
                stbir_resize_uint8_srgb(data, width, height, width*bpp, outputData, outputWidth, outputHeight, outputWidth*bpp, bpp, 3,0);
                freeOutputMemory = true;
            }
        }
        stbi_write_png(fileName.c_str(), outputWidth, outputHeight, bpp, outputData, outputWidth*bpp);
        if (freeOutputMemory) {
            free(outputData);
        }
        return true;
    }
    inline void Free ()
    {
        free(data);
        data = NULL;
    }
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
    bool resize = false;
};

static std::vector<U8> ReadEntireBinaryFile (std::string fileName)
{
    std::ifstream file(fileName, std::ios::binary);
    std::vector<U8> content;
    content.resize(std::filesystem::file_size(fileName));
    file.read(reinterpret_cast<char*>(&content[0]), content.size()*sizeof(U8));
    return content;
}

static void TokenizeString (const std::string& str, const char* delims, std::vector<std::string>& tokens)
{
    size_t prev = 0;
    size_t pos;

    while ((pos = str.find_first_of(delims, prev)) != std::string::npos) {
        if (pos > prev) tokens.push_back(str.substr(prev, pos-prev));
        prev = pos+1;
    }
    if (prev < str.length()) {
        tokens.push_back(str.substr(prev, std::string::npos));
    }
}

static void PrintHelpMessage ()
{
    constexpr const char* helpMessage =
    "makeicon [-resize] -sizes:x,y,z,w,... -input:x,y,z,w,... output\n"
    "\n"
    "    -sizes: ...  [Required]  Comma-separated input size(s) of icon image to generate for the output icon.\n"
    "    -input: ...  [Required]  Comma-separated input image(s) to be used to generate the icon sizes.\n"
    "    -resize      [Optional]  Whether to resize input images to match the specified sizes.\n"
    // "    -detail      [Optional]  Prints out detailed information on what the program is doing.\n"
    "    -help        [Optional]  Prints out this help/usage message for the program.\n"
    "     output      [Required]  The name of the icon that will be generated by the program.\n";
    std::cout << helpMessage << std::endl;
}

static Argument FormatArgument (std::string argStr)
{
    // @Improve: Handle ill-formed argument error cases!

    // Remove the '-' character from the argument.
    argStr.erase(0,1);
    // Split the argument into its name and (optional) parameters.
    std::vector<std::string> tokens;
    TokenizeString(argStr, ":", tokens);
    // Store the formatted argument information.
    Argument arg;
    if (!tokens.empty()) {
        arg.name = tokens[0];
        if (tokens.size() > 1) { // If we have parameters!
            TokenizeString(tokens[1], ",", arg.params);
        }
    }
    return arg;
}

static int GenerateIconWin32 (const Options& options)
{
    // Free all of the loaded images on scope exit to avoid memory leaking.
    std::vector<Image> inputImages;
    Defer { for (auto& image: inputImages) image.Free(); };

    // Make sure the temporary directory, where we store all the icon PNGs, actually exists.
    std::filesystem::path tempDirectory = "makeicon_temp/";
    if (!std::filesystem::exists(tempDirectory)) {
        std::filesystem::create_directory(tempDirectory);
    }
    // The temporary directory is deleted when we finish execution.
    Defer { std::filesystem::remove_all(tempDirectory); };

    // Load all of the input images into memory.
    for (auto& fileName: options.input) {
        Image image;
        image.data = stbi_load(fileName.c_str(), &image.width,&image.height,&image.bpp,4); // We force to 4-channel RGBA.
        image.bpp = 4;
        if (!image.data) {
            FATAL_ERROR("Failed to load input image: " << fileName);
        } else {
            // We warn about non-square images as they will be stretched to a square aspect.
            if (image.width != image.height) {
                std::cout << "[makeicon] warning: Image file '" << fileName << "' is not square and will be stretched! Consider changing its size." << std::endl;
            }
            // We warn if two images are passed in with the same size.
            for (auto& input: inputImages) {
                if ((input.width == image.width) && (input.height == image.height)) {
                    std::cout << "[makeicon] warning: Two provided image files have the same size of " << image.width << "! It is ambiguous which one will be used." << std::endl;
                    break;
                }
            }

            inputImages.push_back(image);
        }
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
    for (auto& size: options.sizes) {
        std::string tempFileName = (tempDirectory / (std::to_string(size) + ".png")).string();
        // Search for matching image input size to save out as PNG.
        bool matchFound = false;
        for (auto& image: inputImages) {
            if (image.width == size && image.height == size) {
                matchFound = true;
                image.Save(tempFileName);
                break;
            }
        }
        if (!matchFound) {
            if (!options.resize) {
                // If no match was found and resize wasn't specified then we fail.
                FATAL_ERROR("Size " << size << " was specified but no input image of this size was provided! Potentially specify -resize to allow for reszing to this size.");
            } else {
                // If no match was found and resize was specified then we resize for this icon size.
                inputImages.at(inputImages.size()-1).Save(tempFileName, size,size); // Use the largest image.
            }
        }
    }

    // Now we have all icons saved to the temporary directory we can package them into a final ICO file.

    // Header
    IconDir iconHeader;
    iconHeader.reserved = 0;
    iconHeader.type = static_cast<U16>(ImageType::ICO);
    iconHeader.numImages = static_cast<U16>(options.sizes.size());

    // Directory
    size_t offset = sizeof(IconDir) + (sizeof(IconDirEntry) * options.sizes.size());
    std::vector<IconDirEntry> iconDirectory;
    for (auto& size: options.sizes) {
        std::string tempFileName = (tempDirectory / (std::to_string(size) + ".png")).string();
        IconDirEntry iconDirEntry;
        iconDirEntry.width = static_cast<U8>(size); // Values of 256 (the max) will turn into 0 on cast, which is what the ICO spec wants.
        iconDirEntry.height = static_cast<U8>(size);
        iconDirEntry.numColors = 0;
        iconDirEntry.reserved = 0;
        iconDirEntry.colorPlanes = 0;
        iconDirEntry.bpp = 4*8; // We force to 4-channel RGBA!
        iconDirEntry.size = static_cast<U32>(std::filesystem::file_size(tempFileName));
        iconDirEntry.offset = static_cast<U32>(offset);
        iconDirectory.push_back(iconDirEntry);
        offset += iconDirEntry.size;
    }

    // Save
    std::ofstream output(options.output, std::ios::binary|std::ios::trunc);
    if (!output.is_open()) {
        FATAL_ERROR("Failed to save output file '" << options.output << "'!");
    } else {
        output.write((reinterpret_cast<char*>(&iconHeader)), sizeof(iconHeader));
        for (auto& dirEntry: iconDirectory) {
            output.write((reinterpret_cast<char*>(&dirEntry)), sizeof(dirEntry));
        }
        for (auto& size: options.sizes) {
            std::string tempFileName = (tempDirectory / (std::to_string(size) + ".png")).string();
            auto data = ReadEntireBinaryFile(tempFileName);
            output.write(reinterpret_cast<char*>(&data[0]), data.size());
        }
    }

    return EXIT_SUCCESS;
}

int main (int argc, char** argv)
{
    Options options;

    // Parse command line arguments given to the program, if there are not
    // any arguments provided then it makes sense to print the help message.
    // This parsing process populates the program options structure with
    // information needed in order to perform the desired program task.
    if (argc <= 1) {
        PrintHelpMessage();
        return EXIT_SUCCESS;
    } else {
        for (int i=1; i<argc; ++i) {
            // Handle options.
            if (argv[i][0] == '-') {
                Argument arg = FormatArgument(argv[i]);
                if (arg.name == "resize") {
                    options.resize = true;
                } else if (arg.name == "sizes") {
                    for (auto& param: arg.params) options.sizes.push_back(std::stoi(param));
                    if (options.sizes.empty()) FATAL_ERROR("No sizes provided with -sizes argument!");
                } else if (arg.name == "input") {
                    for (auto& param: arg.params) options.input.push_back(param);
                    if (options.input.empty()) FATAL_ERROR("No input provided with -input argument!");
                } else if (arg.name == "help") {
                    PrintHelpMessage();
                    return EXIT_SUCCESS;
                } else {
                    FATAL_ERROR("Unknown argument: '" << arg.name << "'!");
                }
            // Handle output.
            } else {
                // If there are still arguments/options after the final output name parameter then we consider
                // the input ill-formed and we inform the user of how to format the arguments to the program.
                if (i < (argc-1)) {
                    FATAL_ERROR("Extra arguments after final '" << argv[i] << "' parameter!");
                } else {
                    options.output = argv[i];
                }
            }
        }
    }

    // Ensure the options structure is populated with valid data. If there are no inputs, sizes, or ouput we cannot run!
    if (options.sizes.empty())  FATAL_ERROR("No icon sizes provided! Specify sizes using: -sizes:x,y,z,w...");
    if (options.input.empty())  FATAL_ERROR("No input images provided! Specify input using: -input:x,y,z,w...");
    if (options.output.empty()) FATAL_ERROR("No output name provded! Specify output name like so: makeicon ... outputname.ico");
    // The maximum size allows in an ICO file is 256x256! We also check for 0 or less as that would not be valid either...
    for (auto& size: options.sizes) {
        if (size > 256) {
            FATAL_ERROR("Invalid icon size '" << size << "'! Maximum value allowed is 256 pixels.");
        } else if (size <= 0) {
            FATAL_ERROR("Invalid icon size '" << size << "'! Minimum value allowed is 1 pixel.");
        }
    }

    // Sort the input images from largest to smallest.
    std::sort(options.input.begin(), options.input.end());

    // Takes the populated options structure and uses those options to generate an icon for the desired platform.
    // @Incomplete: Currently we only handle generating windows icons but in the future we would probably want
    // the platform type as a command-line argument that will determine what icon type gets generated when run.
    return GenerateIconWin32(options);
}

/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2021 Joshua Robertson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.

*******************************************************************************/
