#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <cstdlib>
#include <cstdio>
#include <cstdint>

#define MAKEICON_VERSION_MAJOR 1
#define MAKEICON_VERSION_MINOR 2

constexpr const char* MAKEICON_HELP_MESSAGE =
"makeicon [-resize] -sizes:x,y,z,w,... -input:x,y,z,w,... output\n"
"\n"
"    -sizes: ...  [Required]  Comma-separated input size(s) of icon image to generate for the output icon.\n"
"    -input: ...  [Required]  Comma-separated input image(s) and/or directories and/or .txt files containing file names to be used to generate the icon sizes.\n"
"    -resize      [Optional]  Whether to resize input images to match the specified sizes.\n"
"    -platform    [Optional]  Platform to generate icons for. Options are win32, osx, ios, android. Defaults to win32.\n"
// "    -detail      [Optional]  Prints out detailed information on what the program is doing.\n"
"    -version     [Optional]  Prints out the current version number of the makeicon binary and exits.\n"
"    -help        [Optional]  Prints out this help/usage message for the program and exits.\n"
"     output      [Required]  The name of the icon that will be generated by the program.\n";

#include "mi_utility.h"
#include "mi_platforms.h"

static void print_version_message ()
{
    fprintf(stdout, "makeicon v%d.%d\n", MAKEICON_VERSION_MAJOR, MAKEICON_VERSION_MINOR);
}
static void print_help_message ()
{
    fprintf(stdout, "%s\n", MAKEICON_HELP_MESSAGE);
}

static Argument format_argument (std::string arg_str)
{
    // @Improve: Handle ill-formed argument error cases!

    // Remove the '-' character from the argument.
    arg_str.erase(0,1);
    // Split the argument into its name and (optional) parameters.
    std::vector<std::string> tokens;
    tokenize_string(arg_str, ":", tokens);
    // Store the formatted argument information.
    Argument arg;
    if (!tokens.empty())
    {
        arg.name = tokens[0];
        if (tokens.size() > 1) // If we have parameters!
        {
            tokenize_string(tokens[1], ",", arg.params);
        }
    }
    return arg;
}

int main (int argc, char** argv)
{
    Options options;

    // Parse command line arguments given to the program, if there are not
    // any arguments provided then it makes sense to print the help message.
    // This parsing process populates the program options structure with
    // information needed in order to perform the desired program task.
    if (argc <= 1)
    {
        print_help_message();
        return EXIT_SUCCESS;
    }
    else
    {
        for (int i=1; i<argc; ++i)
        {
            // Handle options.
            if (argv[i][0] == '-')
            {
                Argument arg = format_argument(argv[i]);
                if (arg.name == "resize")
                {
                    options.resize = true;
                }
                else if (arg.name == "sizes")
                {
                    for (auto& param: arg.params)
                    {
                        // allows sizes to be specified through a json file (used for apple icon generation)
                        if(param.find(".json") != -1)
                            options.contents = param;
                        else
                            options.sizes.push_back(std::stoi(param));
                    }
                    if (options.sizes.empty() && options.contents.empty()) ERROR("No sizes provided with -sizes argument!");
                }
                else if (arg.name == "input")
                {
                    for (auto& param: arg.params)
                    {
                        if (std::filesystem::is_directory(param))
                        {
                            for(auto& p: std::filesystem::directory_iterator(param))
                            {
                                if (std::filesystem::is_regular_file(p))
                                {
                                    options.input.push_back(p.path().string());
                                }
                            }
                        }
                        else
                        {
                            if (std::filesystem::path(param).extension() == ".txt")
                            {
                               // If it's a text file we read each line and add those as file names for input.
                                std::ifstream file(param);
                                if (!file.is_open())
                                {
                                    ERROR("Failed to read .txt file passed in as input: %s", param.c_str());
                                }
                                else
                                {
                                    std::string line;
                                    while (getline(file, line))
                                    {
                                        if (std::filesystem::is_regular_file(line))
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
                    if (options.input.empty()) ERROR("No input provided with -input argument!");
                }
                else if (arg.name == "platform")
                {
                    std::string platform = arg.params[0];
                    for(size_t i = 0; i < e_platform::COUNT; ++i)
                    {
                        if(platform == platform_names[i])
                        {
                            options.platform = (Platform)i;
                            break;
                        }
                    }
                }
                else if (arg.name == "version")
                {
                    print_version_message();
                    return EXIT_SUCCESS;
                }
                else if (arg.name == "help")
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
                if (i < (argc-1))
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
    if (options.sizes.empty() && options.contents.empty())  ERROR("No icon sizes provided! Specify sizes using: -sizes:x,y,z,w...");
    if (options.input.empty())  ERROR("No input images provided! Specify input using: -input:x,y,z,w...");
    if (options.output.empty()) ERROR("No output name provded! Specify output name like so: makeicon ... outputname.ico");
    // The maximum size allows in an ICO file is 256x256! We also check for 0 or less as that would not be valid either...
    for (auto& size: options.sizes)
    {
        if (size > 256) ERROR("Invalid icon size '%d'! Maximum value allowed is 256 pixels.", size);
        else if (size <= 0) ERROR("Invalid icon size '%d'! Minimum value allowed is 1 pixel.", size);
    }

    // Sort the input images from largest to smallest.
    std::sort(options.input.begin(), options.input.end());

    // Takes the populated options structure and uses those options to generate an icon for the desired platform.
    return make_icon(options);
}
