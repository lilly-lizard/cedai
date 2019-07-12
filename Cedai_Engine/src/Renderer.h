#pragma once

#include "tools/Sphere.h"

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.hpp>
#include <vector>
#include <string>

class Interface;

class Renderer {
public:

	void init(int image_width, int image_height, Interface* interface,
		std::vector<Sphere>& spheres, std::vector<Sphere>& lights);

	void queueRender(const float view[4][4]);
	void queueFinish();

	void cleanUp();

private:

	const int foo = 0;

	cl::Platform platform;
	cl::Device device;
	cl::Context context;
	cl::CommandQueue queue;

	cl::Kernel kernel;

	cl::Event textureDone;
	cl::Event rayGenDone;
	cl::Event sphereDone;
	cl::Event drawDone;

	std::vector<cl::Event> sphereWaits{ rayGenDone };
	std::vector<cl::Event> drawWaits{ textureDone, sphereDone };
	std::vector<cl::Event> textureWaits{ drawDone };

	int image_width;
	int image_height;
	cl::NDRange global_work_pixels;
	cl::NDRange local_work_pixels = cl::NDRange(16, 16);
	cl::NDRange global_work_spheres;
	cl::NDRange local_work_spheres = cl::NDRange(16, 16, 1);

	cl::Buffer cl_spheres;
	cl::Image2D cl_rays;
	cl::Image2DArray cl_sphere_t; // sphere intersection t values
	cl::ImageGL cl_output;
	std::vector<cl::Memory> gl_objects{ cl_output };

	int sphere_count = 0;
	int light_count = 0;

	cl_float16 cl_view;
	cl_float3 cl_pos;

	void createPlatform();
	void createDevive();

	void pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms);
	void pickDevice(cl::Device& device, const std::vector<cl::Device>& devices);

	void createContext(Interface* interface);
	void createQueue();

	void createBuffers(cl_GLenum gl_texture_target, cl_GLuint gl_texture,
			std::vector<Sphere>& spheres, std::vector<Sphere>& lights);

	void createKernels();
	void createKernel(const char* filename, cl::Kernel& kernel, const char* entryPoint);
	void setWorkGroups();

	void checkCLError(cl_int err, std::string message);
	void printErrorLog(const cl::Program& program, const cl::Device& device);
};