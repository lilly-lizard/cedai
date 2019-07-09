
clang++ -std=c++17 -w -I ..\vendor\spdlog\include\ -I ..\vendor\SDL2\include\ -I ..\vendor\SDL2\glm\ -I $(INTELOCLSDKROOT)\include -L ..\vendor\SDL2\lib\x64\SDL2.lib -L $(INTELOCLSDKROOT)\lib\x64\OpenCL.lib src\Cedai.cpp src\Interface.cpp src\Renderer.cpp src\tools\Log.cpp -o cedai.exe
PAUSE