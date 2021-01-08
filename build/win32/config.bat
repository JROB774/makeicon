@echo off

:: Can be either "Debug" or "Release"
set BuildMode=Release
:: Can be either "x86" or "amd64"
set Architecture=amd64

set Libraries=setargv.obj

set IncludeDirs=
set LibraryDirs=

set Defines=-D_CRT_SECURE_NO_WARNINGS -DPLATFORM_WIN32

set CompilerFlags=-Zc:__cplusplus -std:c++17 -nologo -W4 -MT -Oi -EHsc -Z7
set LinkerFlags=-opt:ref -incremental:no -force:multiple -subsystem:console

set CompilerWarnings=-wd4100 -wd4505 -wd4189 -wd4201
set LinkerWarnings=-ignore:4099

set ResourceFile=
set ResourcePath=

set InputResource=
set InputSource=source\makeicon.cpp

set OutputPath=binary\win32\
set OutputName=makeicon-%Architecture%

if %BuildMode%==Release (
    set Defines=%Defines% -DNDEBUG
)
if %BuildMode%==Debug (
    set OutputName=%OutputName%-Debug
    set Defines=%Defines% -DBUILD_DEBUG
    set InputResource=
)

if %Architecture%==x86 (
    set CompilerFlags=%CompilerFlags% -arch:IA32
    set LinkerFlags=%LinkerFlags%,5.1
)

set OutputExecutable=%OutputPath%%OutputName%
