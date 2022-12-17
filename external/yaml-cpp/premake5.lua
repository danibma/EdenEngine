project "yaml-cpp"
	kind "StaticLib"
	language "C++"
    staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin/intermediate/")

	files
	{
		"src/**.h",
        "src/**.cpp",

        "include/**.h",
	}
    
    includedirs
    {
        "include"
    }

    defines
    {
        "YAML_CPP_STATIC_DEFINE"
    }

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++17"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

    filter "configurations:Dist"
		runtime "Release"
		optimize "on"
        symbols "off"
