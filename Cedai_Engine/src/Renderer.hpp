#pragma once

#include "tools/Config.hpp"
#include "model/Sphere.hpp"

#include <CL/cl.hpp>
#include <vector>
#include <string>

class Interface;
class PrimitiveProcessor;

class Renderer {
public:

	void init(int image_width, int image_height,
		Interface* interface, PrimitiveProcessor* vertexProcessor,
		std::vector<cd::Sphere>& spheres, std::vector<cd::Sphere>& lights, std::vector<cl_uchar4>& polygon_colors);

	void renderQueue(const float view[4][4], float seconds);
	void renderBarrier();

	void resize(int image_width, int image_height, Interface *interface);

	void cleanUp();

private:

	cl::Platform platform;
	cl::Device device;
	cl::Context context;
	cl::CommandQueue queue;

	cl::Kernel kernel;

	int image_width = 0, image_height = 0;
	cl::NDRange global_work;
	cl::NDRange local_work;

	cl::Buffer cl_spheres;
	cl::Buffer cl_gl_vertices;
	cl::Buffer cl_polygons;
	cl::ImageGL cl_output;
	std::vector<cl::Memory> gl_objects;

	int sphere_count = 0;
	int light_count = 0;
	int polygon_count = 0;

	void createPlatform();
	void createDevive();

	void pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms);
	void pickDevice(cl::Device& device, const std::vector<cl::Device>& devices);

	void createContext(Interface* interface);
	void createQueue();

	void createBuffers(cl_GLenum gl_texture_target, cl_GLuint gl_texture, cl_GLuint gl_vert_buffer,
			std::vector<cd::Sphere>& spheres, std::vector<cd::Sphere>& lights, std::vector<cl_uchar4>& polygon_colors);
	void createOutputImage(cl_GLenum gl_texture_target, cl_GLuint gl_texture);
	void setGLObjects();

	void createKernels();
	void setOutArg();
	void createKernel(const char* filename, cl::Kernel& kernel, const char* entryPoint);
	void setWorkGroupSizes();

	void printErrorLog(const cl::Program& program, const cl::Device& device);
};
