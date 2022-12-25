project "D3D12MemoryAllocator"
	kind "StaticLib"
	language "C++"
    staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin/intermediate/")

	files
	{
		"D3D12MemAlloc.cpp",
        "D3D12MemAlloc.h"
	}

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++17"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:DebugEditor"
		runtime "Debug"
		symbols "on"

	filter "configurations:Profiling"
		runtime "Release"
		optimize "on"

	filter "configurations:ProfilingEditor"
		runtime "Release"
		optimize "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"
