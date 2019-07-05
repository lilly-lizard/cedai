workspace "Cedai"
	architecture "x64" -- no 32 bit support

	configurations
	{
		"Debug",
		"Release"
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
		"Cedai_Engine/src/**.h",	-- all headers
		"Cedai_Engine/src/**.cpp",	-- all source files
		"Cedai_Engine/src/**.cl"	-- all opencl files
	}

	includedirs
	{
		"Cedai_Engine/src",
		"$(INTELOCLSDKROOT)/include",	-- opencl
		"vendor/glm",					-- glm
		"vendor/SDL2/include"			-- SDL2
	}

	libdirs
	{
		"$(INTELOCLSDKROOT)/lib/x64",	-- opencl
		"vendor/SDL2/lib/x64"			-- SDL2
	}

	links
	{
		"OpenCL.lib",
		"SDL2.lib"
	}

	postbuildcommands
	{
		'{COPY} %{wks.location}/vendor/SDL2/lib/x64/SDL2.dll %{cfg.buildtarget.directory}'
	}
	
	filter "system:windows"
		cppdialect "C++17" -- note we may need specific compile flag for other systems
		systemversion "latest"

		defines
		{
			"CD_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "CD_DEBUG"
		symbols "On"

	filter "configurations:Release"
		defines "CD_RELEASE"
		optimize "On"
