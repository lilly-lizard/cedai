workspace "Cedai"
	architecture "x64" -- no 32 bit support

	configurations
	{
		"debug",
		"release"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
fbxdir = "C:/Program Files/Autodesk/FBX/FBX SDK/2019.2/"

engine_name = "Cedai_Engine"

project "Cedai_Engine" -- game engine
	location "Cedai_Engine"
	kind "ConsoleApp"
	language "C++"

	targetdir (engine_name .. "/bin/" .. outputdir)	-- binaries
	objdir (engine_name .. "/bin-int/" .. outputdir)	-- intermediate files

	files {
		engine_name .. "/src/**.hpp",	-- headers
		engine_name .. "/src/**.h",		-- headers
		engine_name .. "/src/**.cpp",	-- source files
		engine_name .. "/src/**.cl",	-- opencl kernels
		engine_name .. "/src/**.vert",	-- vert shaders
		engine_name .. "/src/**.frag",	-- frag shaders
		"vendor/gl3w/include/**.c",		-- gl3w.c
		"vendor/gl3w/include/GL/**.h"	-- gl3w.h and glcorearb.h
	}

	includedirs {
		engine_name .. "/src",
		"$(INTELOCLSDKROOT)/include",	-- opencl
		"vendor/glm",					-- glm
		"vendor/glfw/include",			-- glfw
		"vendor/gl3w/include",			-- gl3w
		"vendor/spdlog/include"			-- spdlog
	}

	libdirs {
		"$(INTELOCLSDKROOT)/lib/x64",	-- opencl
		"vendor/glfw/lib-vc2017"		-- glfw
	}

	links {
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

	filter "configurations:debug"
		defines "DEBUG"
		symbols "On"

	filter "configurations:release"
		defines "NDEBUG"
		optimize "On"

converter_name = "Cedai_Model_Converter"

project "Cedai_Model_Converter" -- model converter
	location "Cedai_Model_Converter"
	kind "ConsoleApp"
	language "C++"

	targetdir (converter_name .. "/bin/" .. outputdir)	-- binaries
	objdir (converter_name .. "/bin-int/" .. outputdir)	-- intermediate files
	
	files {
		converter_name .. "/src/**.hpp",	-- headers
		converter_name .. "/src/**.h",		-- headers
		converter_name .. "/src/**.cpp"		-- source files
	}

	includedirs {
		engine_name .. "/src",
		"vendor/glm", -- glm
		"C:/Program Files/Autodesk/FBX/FBX SDK/2019.2/include" -- fbx sdk
	}
	
	libdirs {
		"C:/Program Files/Autodesk/FBX/FBX SDK/2019.2/lib/vs2017/x64/%{cfg.buildcfg}"
 	}

	links {
		"libfbxsdk.lib"	-- fbx sdk
	}

	defines {
		"FBXSDK_SHARED"
	}

	postbuildcommands {
		'{COPY} "C:/Program Files/Autodesk/FBX/FBX SDK/2019.2/lib/vs2017/x64/%{cfg.buildcfg}/libfbxsdk.dll" %{cfg.buildtarget.directory}'
	}
	
	filter "system:windows"
		cppdialect "C++17" -- note we may need specific compile flag for other systems
		systemversion "latest"

	filter "configurations:debug"
		defines "DEBUG"
		symbols "On"

	filter "configurations:release"
		defines "NDEBUG"
		optimize "On"