#pragma once

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.hpp>
#include <vector>
#include <string>

// the total size of a struct must be a multiple of float4 (cl_float3 is the size of float4)
struct Sphere {
	cl_float radius;
	cl_float dummy1;
	cl_float dummy2;
	cl_float dummy3;
	cl_float3 position;
	cl_uchar3 color;
};

class Interface;

class Renderer {
public:

	void init(int image_width, int image_height, Interface* interface);

	void render(const float view[4][4]);

	void cleanUp();

private:

	const int foo = 0;

	cl::Platform platform;
	cl::Device device;
	cl::Context context;
	cl::CommandQueue queue;

	cl::Kernel rayGenKernel;
	cl::Kernel sphereKernel;
	cl::Kernel drawKernel;

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

	int sphere_count;
	int light_count;
	Sphere* cpu_spheres;
	Sphere* cpu_lights;

	cl_float16 cl_view;
	cl_float3 cl_pos;

	void createPlatform();
	void createDevive();

	void pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms);
	void pickDevice(cl::Device& device, const std::vector<cl::Device>& devices);

	void createContext(Interface* interface);

	void createBuffers(cl_GLenum gl_texture_target, cl_GLuint gl_texture);
	void createSpheres();
	void createLights();

	void createKernels();
	void createKernel(const char* filename, cl::Kernel& kernel, const char* entryPoint);
	void setWorkGroups();

	void checkCLError(cl_int err, std::string message);
	void printErrorLog(const cl::Program& program, const cl::Device& device);
};