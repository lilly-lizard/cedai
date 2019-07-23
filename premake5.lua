workspace "Cedai"
	architecture "x64" -- no 32 bit support

	configurations
	{
		"debug",
		"release"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Cedai_Engine" -- game engine
	location "Cedai_Engine"
	kind "ConsoleApp"
	language "C++"

	targetdir ("Cedai_Engine/bin/" .. outputdir)	-- binaries
	objdir ("Cedai_Engine/bin-int/" .. outputdir)	-- intermediate files

	files
	{
		"Cedai_Engine/src/**.hpp",		-- headers
		"Cedai_Engine/src/**.h",		-- headers
		"Cedai_Engine/src/**.cpp",		-- source files
		"Cedai_Engine/src/**.cl",		-- opencl kernels
		"Cedai_Engine/src/**.vert",		-- vert shaders
		"Cedai_Engine/src/**.frag",		-- frag shaders
		"vendor/gl3w/include/**.c",		-- gl3w.c
		"vendor/gl3w/include/GL/**.h"	-- gl3w.h and glcorearb.h
	}

	includedirs
	{
		"Cedai_Engine/src",
		"$(INTELOCLSDKROOT)/include",	-- opencl
		"vendor/glm",					-- glm
		"vendor/glfw/include",			-- glfw
		"vendor/gl3w/include",			-- gl3w
		"vendor/spdlog/include"			-- spdlog
	}

	libdirs
	{
		"$(INTELOCLSDKROOT)/lib/x64",	-- opencl
		"vendor/glfw/lib-vc2017"		-- glfw
	}

	links
	{
		"OpenCL.lib",
		"glfw3.lib"
	}

	filter "system:windows"
		cppdialect "C++17" -- note we may need specific compile flag for other systems
		systemversion "latest"

		defines
		{
			"CD_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "DEBUG"
		symbols "On"

	filter "configurations:Release"
		defines "NDEBUG"
		optimize "On"
