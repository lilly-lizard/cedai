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
	cl_float3 color;
};

class Renderer {
public:

	void init(int image_width, int image_height);

	void render(float *pixels, const float view[4][4]);

	void cleanUp();

private:

	int image_width;
	int image_height;
	std::size_t global_work_size;
	std::size_t local_work_size;
	int sphere_count;
	Sphere* cpu_spheres;

	cl_float4* cpu_output;
	cl::CommandQueue queue;
	cl::Kernel kernel;
	cl::Context context;
	cl::Program program;

	cl::Buffer cl_output;
	cl::Buffer cl_spheres;

	void pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms);

	void pickDevice(cl::Device& device, const std::vector<cl::Device>& devices);

	void createSpheres();

	void printErrorLog(const cl::Program& program, const cl::Device& device);

	inline float clamp(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
};