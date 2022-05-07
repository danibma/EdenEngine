workspace "EdenEngine"
    configurations { "Debug", "Release" }
    targetdir "bin"
    startproject "EdenEngine"

    configurations 
    { 
        "Debug", 
        "Release"
    }

    flags 
    {
        "MultiProcessorCompile"
    }

    filter "language:C++ or language:C"
        architecture "x86_64"
    filter ""

outputdir = "%{cfg.buildcfg}"

group "ThirdParty"
include "external/imgui"
include "external/optick"
include "external/D3D12MemoryAllocator"
group ""

project "EdenEngine"
    location "EdenEngine"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("bin/" .. outputdir)
	objdir ("bin/intermediate/")

    files 
	{ 
		"%{prj.name}/src/**.h", 
		"%{prj.name}/src/**.c", 
		"%{prj.name}/src/**.hpp", 
		"%{prj.name}/src/**.cpp",
        "%{prj.name}/shaders/**.hlsl",
	}

    includedirs
	{
		"%{prj.name}/src",

		"%{wks.location}/external",
        "%{wks.location}/external/ImGui",
        "%{wks.location}/external/Optick/src",
	}

    links
	{ 
		"ImGui",
        "Optick",
        "D3D12MemoryAllocator",

        "d3d12.lib",
        "dxgi.lib",

        "%{wks.location}/external/dxc/dxcompiler.lib",
	}

    defines
	{
		"GLM_FORCE_DEPTH_ZERO_TO_ONE"
	}
    
    postbuildcommands 
    {
        '{COPY} "%{wks.location}/external/dxc/dxcompiler.dll" "%{cfg.targetdir}"',
    }

    filter { "files:**.hlsl" }
        flags {"ExcludeFromBuild"}

    filter "system:windows"
		systemversion "latest"

		defines 
		{ 
			"ED_PLATFORM_WINDOWS"
		}

    filter "configurations:Debug"
		symbols "On"
        kind "ConsoleApp"

		includedirs
		{

		}

		links
		{

		}

		defines 
		{
			"ED_DEBUG",
            "ED_TRACK_MEMORY"
		}

	filter "configurations:Release"
		optimize "On"
        kind "WindowedApp"

		includedirs
		{

		}

		defines
		{
			"ED_RELEASE",
			"ED_TRACK_MEMORY"
		}

		links
		{
            
		}

group ""