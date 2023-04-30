workspace "EdenEngine"
    targetdir "bin"
    startproject "EdenEngine"

    configurations 
    { 
        "Debug", 
		"DebugEditor", 
        "Profiling",
		"ProfilingEditor",
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
include "external/D3D12MemoryAllocator"
include "external/yaml-cpp"
group ""

project "EdenEngine"
    location "EdenEngine"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("bin/" .. outputdir)
	objdir ("bin/intermediate/")

    flags { "FatalWarnings" }

    files 
	{ 
		"%{prj.name}/src/**.h", 
		"%{prj.name}/src/**.c", 
		"%{prj.name}/src/**.hpp", 
		"%{prj.name}/src/**.cpp",
        "%{prj.name}/shaders/**.hlsl",
        "%{prj.name}/shaders/**.hlsli",
        "%{prj.name}/resource.rc"
	}

    includedirs
	{
		"%{prj.name}/src",

		"%{wks.location}/external",
        "%{wks.location}/external/ImGui",
        "%{wks.location}/external/yaml-cpp/include",
	}

    links
	{ 
		"ImGui",
        "D3D12MemoryAllocator",
        "yaml-cpp",

        "d3d12.lib",
        "dxgi.lib",
        "dxguid.lib",
        "%{wks.location}/external/WinPixEventRuntime/WinPixEventRuntime.lib",

        "%{wks.location}/external/dxc/dxcompiler.lib",
	}

    defines
	{
		"GLM_FORCE_DEPTH_ZERO_TO_ONE"
	}
    
    postbuildcommands 
    {
        '{COPY} "%{wks.location}/external/dxc/dxcompiler.dll" "%{cfg.targetdir}"',
        '{COPY} "%{wks.location}/external/dxc/dxil.dll" "%{cfg.targetdir}"',
        '{COPY} "%{wks.location}/external/WinPixEventRuntime/WinPixEventRuntime.dll" "%{cfg.targetdir}"',
    }

    filter { "files:**.hlsl or files:**.hlsli" }
        flags {"ExcludeFromBuild"}

    filter "system:windows"
		systemversion "latest"

		defines 
		{ 
			"ED_PLATFORM_WINDOWS"
		}

    filter "configurations:Debug"
		symbols "On"
        kind "WindowedApp"

		defines 
		{
			"ED_DEBUG",
            "ED_TRACK_MEMORY"
		}

	filter "configurations:DebugEditor"
		symbols "On"
        kind "WindowedApp"

		defines 
		{
			"ED_DEBUG",
            "ED_TRACK_MEMORY",
			"WITH_EDITOR"
		}

    filter "configurations:Profiling"
        optimize "On"
        kind "WindowedApp"

        defines
        {
            "ED_PROFILING",
            "ED_TRACK_MEMORY"
        }

	filter "configurations:ProfilingEditor"
        optimize "On"
        kind "WindowedApp"

        defines
        {
            "ED_PROFILING",
            "ED_TRACK_MEMORY",
			"WITH_EDITOR"
        }

	filter "configurations:Release"
		optimize "On"
        kind "WindowedApp"

		defines
		{
			"ED_RELEASE",
		}

group ""