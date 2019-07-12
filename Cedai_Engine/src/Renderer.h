#pragma once

#include "tools/Sphere.h"
#include "tools/Polygon.h"

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.hpp>
#include <vector>
#include <string>

class Interface;

class Renderer {
public:

	void init(int image_width, int image_height, Interface* interface,
		std::vector<cd::Sphere>& spheres, std::vector<cd::Sphere>& lights,
		std::vector<cl_float3>& vertices, std::vector<cd::Polygon>& polygons);

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
	cl::NDRange global_work;
	cl::NDRange local_work;

	cl::Buffer cl_spheres;
	cl::Buffer cl_vertices;
	cl::Buffer cl_polygons;
	cl::ImageGL cl_output;
	std::vector<cl::Memory> gl_objects{ cl_output };

	int sphere_count = 0;
	int light_count = 0;
	int vertex_count = 0;
	int polygon_count = 0;

	void createPlatform();
	void createDevive();

	void pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms);
	void pickDevice(cl::Device& device, const std::vector<cl::Device>& devices);

	void createContext(Interface* interface);
	void createQueue();

	void createBuffers(cl_GLenum gl_texture_target, cl_GLuint gl_texture,
			std::vector<cd::Sphere>& spheres, std::vector<cd::Sphere>& lights,
			std::vector<cl_float3>& vertices, std::vector<cd::Polygon>& polygons);

	void createKernels();
	void createKernel(const char* filename, cl::Kernel& kernel, const char* entryPoint);
	void setWorkGroups();

	void checkCLError(cl_int err, std::string message);
	void printErrorLog(const cl::Program& program, const cl::Device& device);
};