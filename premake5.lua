function set_platform()
    -- Auto determine platfrom from action if necessary
    if platform == nil then
        if _ACTION == "vs2017" or _ACTION == "vs2019" then
            platform = "win32"
        elseif _ACTION == "xcode4" then
            platform = "osx"
        end
    end
    print("platform is " .. platform)
end

set_platform()

workspace("makeicon" .. platform)

    location ("build/" .. platform)
    configurations { "Debug", "Release" }

    project("makeicon")
    project_name = "makeicon"

    kind("ConsoleApp")
    language "C++"
    cppdialect "C++17"
    location("build/" .. platform)
    targetdir("bin/" .. platform)
    
    files
    {
        "source/**.*",
        "include/**.*"
    }
    
    includedirs
    {
        "third_party/stb"
    }
