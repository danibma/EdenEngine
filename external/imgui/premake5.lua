project "ImGui"
	kind "StaticLib"
	language "C++"
    staticruntime "off"

	targetdir ("bin/%{prj.name}")
	objdir ("bin/obj/" .. outputdir)

	files
	{
		"imconfig.h",
		"imgui.h",
		"imgui.cpp",
		"imgui_draw.cpp",
		"imgui_internal.h",
		"imgui_tables.cpp",
		"imgui_widgets.cpp",
		"imstb_rectpack.h",
		"imstb_textedit.h",
		"imstb_truetype.h",
		"imgui_demo.cpp",
        "backends/imgui_impl_dx12.h",
        "backends/imgui_impl_dx12.cpp",
        "backends/imgui_impl_win32.h",
        "backends/imgui_impl_win32.cpp",
		"ImguiHelper.h",
		"ImguiHelper.cpp",
		"ImGuizmo.h",
		"ImGuizmo.cpp",
	}
    
    includedirs
    {
        "%{wks.location}/external/ImGui"
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
