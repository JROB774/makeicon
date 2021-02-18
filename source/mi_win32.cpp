// Windows ICO File Format:
//  https://en.wikipedia.org/wiki/ICO_(file_format)#Outline

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

static int make_icon_win32 (const Options& options)
{
    // Free all of the loaded images on scope exit to avoid memory leaking.
    std::vector<Image> input_images;
    DEFER { for (auto& image: input_images) free_image(image); };

    // Make sure the temporary directory, where we store all the icon PNGs, actually exists.
    std::filesystem::path temp_directory = "makeicon_temp/";
    if (!std::filesystem::exists(temp_directory))
    {
        std::filesystem::create_directory(temp_directory);
    }
    // The temporary directory is deleted when we finish execution.
    DEFER { std::filesystem::remove_all(temp_directory); };

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
        // Search for matching image input size to save out as PNG.
        bool match_found = false;
        for (auto& image: input_images)
        {
            if (image.width == size && image.height == size)
            {
                match_found = true;
                save_image(image, temp_file_name);
                break;
            }
        }
        if (!match_found)
        {
            if (!options.resize)
            {
                // If no match was found and resize wasn't specified then we fail.
                ERROR("Size %d was specified but no input image of this size was provided! Potentially specify -resize to allow for reszing to this size.", size);
            }
            else
            {
                // If no match was found and resize was specified then we resize for this icon size (use the largest image).
                save_image(input_images.at(input_images.size()-1), temp_file_name, size,size);
            }
        }
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
