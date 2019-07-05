#pragma once

#include <vector>
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL\cl.hpp>

class Renderer {
public:

	void init(int image_width, int image_height);

	void draw(float *pixels);

	void cleanUp();

private:

	int image_width;
	int image_height;
	std::size_t global_work_size;
	std::size_t local_work_size;

	cl_float4* cpu_output;
	cl::CommandQueue queue;
	cl::Kernel kernel;
	cl::Context context;
	cl::Program program;
	cl::Buffer cl_output;

	void pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms);

	void pickDevice(cl::Device& device, const std::vector<cl::Device>& devices);

	void printErrorLog(const cl::Program& program, const cl::Device& device);

	void selectRenderMode(unsigned int& rendermode);

	inline float clamp(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
};