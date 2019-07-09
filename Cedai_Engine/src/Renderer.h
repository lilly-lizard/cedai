#pragma once

#include <vector>
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL\cl.hpp>

// the total size of a struct must be a multiple of float4 (cl_float3 is the size of float4)
struct Sphere {
	cl_float radius;
	cl_float dummy1;
	cl_float dummy2;
	cl_float dummy3;
	cl_float3 position;
	cl_uchar3 color;
};

class Renderer {
public:

	void init(int image_width, int image_height);

	void render(uint8_t *pixels, const float view[4][4]);

	void cleanUp();

private:

	cl::Program program;
	cl::Device device;
	cl::Context context;
	cl::CommandQueue queue;

	cl::Kernel mainKernel;

	cl::Kernel rayGenKernel;
	cl::Kernel sphereKernel;
	cl::Kernel drawKernel;

	int image_width;
	int image_height;
	cl::NDRange global_work_pixels;
	cl::NDRange local_work_pixels;
	cl::NDRange global_work_spheres;
	cl::NDRange local_work_spheres;

	cl::Buffer cl_spheres;
	cl::Buffer cl_rays;
	cl::Buffer cl_sphere_t; // sphere intersection t values
	cl::Buffer cl_output;

	cl_float16 cl_view;
	cl_float3 cl_pos;
	int sphere_count;
	int light_count;
	Sphere* cpu_spheres;
	Sphere* cpu_lights;
	cl_uchar4* cpu_output;

	void pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms);

	void pickDevice(cl::Device& device, const std::vector<cl::Device>& devices);

	void createKernel(const char* filename, cl::Kernel &kernel, const char* entryPoint);

	void createSpheres();
	void createLights();

	void printErrorLog(const cl::Program& program, const cl::Device& device);

	inline float clamp(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
};