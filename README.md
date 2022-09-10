# makeicon

```
makeicon [-help] [-version] [-resize] [-platform:name] -sizes:x,y,z... -input:x,y,z... output
```

A command-line utility for easily generating application icons from image files for
a variety of different platforms. Icon generation is available for the following
platforms: **Windows**, **iOS**, **MacOS**, **Android**.

## Building

The makeicon application is simple and can be compiled from the command-line.

### Windows

```
cl -EHsc -std:c++17 -I third_party/stb makeicon.cpp -Fe:makeicon.exe
```

### MacOS

```
clang++ -std=c++17 -I third_party/stb makeicon.cpp -o makeicon
```

## Releases

There are prebuilt binaries available for both Windows and MacOS, these are available
in the `binaries` folder if you wish to include this project in your codebase as a
submodule. These binaries are always the latest version. There are also versioned
binaries available for download from this repository's releases tab.

## License

This project is available under the MIT License, Copyright (C) Joshua Robertson 2021 - 2022.
