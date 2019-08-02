workspace "Cedai"
	architecture "x64" -- no 32 bit support

	configurations {
		"debug",
		"release"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

engine_name = "Cedai_Engine"

project "Cedai_Engine" -- game engine
	location "Cedai_Engine"
	kind "ConsoleApp"
	language "C++"

	targetdir (engine_name .. "/bin/" .. outputdir)	-- binaries
	objdir (engine_name .. "/bin-int/" .. outputdir)	-- intermediate files

	files {
		engine_name .. "/src/**.hpp",		-- headers
		engine_name .. "/src/**.h",			-- headers
		engine_name .. "/src/**.cpp",		-- source files
		engine_name .. "/kernels/**.cl",	-- opencl kernels
		engine_name .. "/shaders/**.vert",	-- vert shaders
		engine_name .. "/shaders/**.frag",	-- frag shaders
	}

	includedirs {
		engine_name .. "/src",
		engine_name .. "/vendor/OpenCL-Headers",		-- opencl
		engine_name .. "/vendor/glm",					-- glm
		engine_name .. "/vendor/glfw_custom/include",	-- glfw
		engine_name .. "/vendor/gl3w/include",			-- gl3w
		engine_name .. "/vendor/spdlog/include"			-- spdlog
	}
	
	libdirs {
		engine_name .. "/vendor/gl3w/lib/",			-- gl3w
		engine_name .. "/vendor/glfw_custom/lib"	-- glfw
	}

	links {
		"gl3w", -- todo: compile linux library
		"glfw3",
		"OpenCL"
	}

	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"

		defines { "CD_PLATFORM_WINDOWS" }

		libdirs {
			"$(INTELOCLSDKROOT)/lib/x64",	-- opencl on intel
			"$(AMDAPPSDKROOT)/lib/x86_64",	-- opencl on amd
			"$(CUDA_LIB_PATH)"				-- opencl on nvidia
		}

	filter "system:linux"
		cppdialect "C++17"

		defines { "CD_PLATFORM_LINUX" }

		libdirs {
			"/usr/lib/x86_64-linux-gnu/"
		}

		links {
			"dl",		-- glfw
			"X11",		-- glfw X11
			"pthread"	-- glfw
		}

	filter "configurations:debug"
		defines "DEBUG"
		symbols "On"

	filter "configurations:release"
		defines "NDEBUG"
		optimize "On"

converter_name = "Cedai_Model_Converter"

fbxdir = "C:/Program Files/Autodesk/FBX/FBX SDK/2019.2/"

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
		converter_name .. "/src",
		engine_name .. "vendor/glm", -- glm
		"C:/Program Files/Autodesk/FBX/FBX SDK/2019.2/include" -- fbx sdk
	}

	libdirs {
		"C:/Program Files/Autodesk/FBX/FBX SDK/2019.2/lib/vs2017/x64/%{cfg.buildcfg}"
 	}

	links { "libfbxsdk.lib" }

	defines { "FBXSDK_SHARED" }

	postbuildcommands {
		'{COPY} "C:/Program Files/Autodesk/FBX/FBX SDK/2019.2/lib/vs2017/x64/%{cfg.buildcfg}/libfbxsdk.dll" %{cfg.buildtarget.directory}'
	}

	filter "system:windows"
		cppdialect "C++17" -- note we may need specific compile flag for other systems
		systemversion "latest"

		defines { "CD_PLATFORM_WINDOWS" }

	filter "configurations:debug"
		defines "DEBUG"
		symbols "On"

	filter "configurations:release"
		defines "NDEBUG"
		optimize "On"


-- NOTES:
-- linux linking: https://stackoverflow.com/questions/16710047/usr-bin-ld-cannot-find-lnameofthelibrary
