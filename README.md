# Cedai Real Time Ray-Tracing Engine

Inspired by a non-euclidean minecraft ray-tracer made by [CNLohr](https://www.youtube.com/watch?v=0pmSPlYHxoY), I learned first the Vulkan graphics API and then the OpenCL compute framework with the goal of making a C++ raytracing video game from the engine up.

The application currently supports sphere, polygon and light rendering, skeletal animation and a framerate that gets higher with every commit. OpenCL kernel(s) are used for rendering, and two OpenGL programs are used for drawing and vertex processing respectively.

## Build notes:

Works on both windows and linux (only tested with intel hd graphics). Your OpenCL driver must support the cl_khr_gl_sharing and cl_khr_fp16 extensions (this check is done by the application anyway). Windows users: I've found that MSVC doesn't play nicely with the Intel OpenCL runtime, so I highly reccomend using clang or gcc to compile.

Run /vendor/premake5/premake with premake.lua to build your platforms configuration files, which can be done with my generate_make.sh or generate_vs_projects.bat scripts.
