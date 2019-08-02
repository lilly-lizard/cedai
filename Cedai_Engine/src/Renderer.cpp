#include "Renderer.hpp"
#include "tools/Config.hpp"

#include "Interface.hpp"
#include "PrimitiveProcessor.hpp"
#include "tools/Log.hpp"

#ifdef CD_PLATFORM_WINDOWS
#	define GLFW_EXPOSE_NATIVE_WGL
#endif
#ifdef CD_PLATFORM_LINUX
#	define GLFW_EXPOSE_NATIVE_GLX
#	define GLFW_EXPOSE_NATIVE_X11
#endif
#include "GLFW/glfw3native.h"

#include <iostream>
#include <fstream>
#include <CL/cl_gl.h>

#define KERNEL_PATH "kernels/kernel.cl"
#define KERNEL_ENTRY "render"

// PUBLIC FUNCTIONS

void Renderer::init(int image_width, int image_height,
		Interface* interface, PrimitiveProcessor* vertexProcessor,
		std::vector<cd::Sphere>& spheres, std::vector<cd::Sphere>& lights, std::vector<cl_uchar4>& polygon_colors) {
	CD_INFO("Initialising renderer...");

	this->image_width = image_width;
	this->image_height = image_height;

	createPlatform();
	createDevive();

	createContext(interface);
	createQueue();

	createBuffers(interface->getTexTarget(), interface->getTexHandle(), vertexProcessor->getVertexBuffer(),
		spheres, lights, polygon_colors);
	createKernels();
	setWorkGroupSizes();

	queue.finish();
}

void Renderer::renderQueue(const float view[4][4], float seconds) {
	static cl_float16 cl_view;
	static cl_float3 cl_pos;
	static cl_float cl_time;

	cl_view = {{ view[0][0], view[0][1], view[0][2], 0,
				 view[1][0], view[1][1], view[1][2], 0,
				 view[2][0], view[2][1], view[2][2], 0,
				 0, 0, 0, 0 }};
	cl_pos = {{ view[3][0], view[3][1], view[3][2] }};
	cl_time = seconds;

	kernel.setArg(0, cl_view);
	kernel.setArg(1, cl_pos);
	kernel.setArg(2, cl_time);

	queue.enqueueAcquireGLObjects(&gl_objects);
	queue.enqueueNDRangeKernel(kernel, 0, global_work, local_work);
}

void Renderer::renderBarrier() {
	queue.enqueueReleaseGLObjects(&gl_objects);
	queue.finish();
}

void Renderer::resize(int image_width, int image_height, Interface *interface) {
	this->image_width = image_width;
	this->image_height = image_height;
	setWorkGroupSizes();

	// trust that OpenCL handles buffer release
	gl_objects.clear();
	createOutputImage(interface->getTexTarget(), interface->getTexHandle());
	setOutArg();
	setGLObjects();
}

void Renderer::cleanUp() {
	
}

// INIT FUNCTIONS

void Renderer::createPlatform() {

	// Get all available OpenCL platforms (e.g. AMD OpenCL, Nvidia CUDA, Intel OpenCL)
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	if (platforms.size() < 1) {
		CD_ERROR("Renderer init error: no opencl implementation found");
		throw std::runtime_error("no opencl platform");
	}
	std::cout << "~ " << platforms.size() << " available OpenCL platforms: \n~ \n";
	for (int i = 0; i < platforms.size(); i++)
		std::cout << "~ \t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << std::endl;

	// Pick one platform
	pickPlatform(platform, platforms);
	std::cout << "~ \n~ using OpenCL platform: \t" << platform.getInfo<CL_PLATFORM_NAME>() << std::endl;
}

void Renderer::createDevive() {

	// Get available OpenCL devices on platform
	std::vector<cl::Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

	std::cout << "~ \n~ " << devices.size() << " available OpenCL devices on this platform: " << "\n~ \n";
	for (int i = 0; i < devices.size(); i++) {
		std::cout << "~ \t" << i + 1 << ": " << devices[i].getInfo<CL_DEVICE_NAME>() << std::endl;
		std::cout << "~ \t\tMax compute units: " << devices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << std::endl;
		std::cout << "~ \t\tMax constant buffer size: " << devices[i].getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() << std::endl;
		std::cout << "~ \t\tMax constant args: " << devices[i].getInfo<CL_DEVICE_MAX_CONSTANT_ARGS>() << std::endl;
		std::cout << "~ \t\tMax mem alloc: " << devices[i].getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>() << std::endl;
		std::cout << "~ \t\tMax work group size: " << devices[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << "\n~ \n";
	}

	// Pick one device
	pickDevice(device, devices);
	std::cout << "~ \n~ OpenCL using device: \t" << device.getInfo<CL_DEVICE_NAME>() << std::endl;
}

void Renderer::pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms) {
	platform = platforms[0];
	return;
}

void Renderer::pickDevice(cl::Device& device, const std::vector<cl::Device>& devices) {
	for (cl::Device dev : devices) {
		std::string extensions = dev.getInfo<CL_DEVICE_EXTENSIONS>();
		bool isGPU = dev.getInfo< CL_DEVICE_TYPE>() == CL_DEVICE_TYPE_GPU;
		bool glSharing = extensions.find("cl_khr_gl_sharing") != std::string::npos;
		bool halfFloat = extensions.find("cl_khr_fp16") != std::string::npos;
		std::string devName = dev.getInfo<CL_DEVICE_NAME>();
		CD_TRACE("cl device {}: is GPU = {}; gl sharing = {}; half float support = {}", devName, isGPU, glSharing, halfFloat);
		
		if (isGPU && glSharing && halfFloat) {
			// CL_DEVICE_TYPE_GPU 16 x 16 x 1 = 256
			// CL_DEVICE_TYPE_CPU 128 x 60 x 1 < 8192
			device = dev;
			return;
		}
	}

	CD_ERROR("Renderer init error: no suitable opencl device found");
	throw std::runtime_error("no suitable opencl device");
}

void Renderer::createContext(Interface* interface) {
#	ifdef CD_PLATFORM_WINDOWS
	cl_context_properties contextProps[] = {
		CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
		CL_GL_CONTEXT_KHR,   (cl_context_properties)glfwGetWGLContext(interface->getWindow()),	// WGL Context
		CL_WGL_HDC_KHR,      (cl_context_properties)glfwGetWGLDC(interface->getWindow()),		// WGL HDC
		0 };

	cl_int result;
	context = cl::Context(device, contextProps, NULL, NULL, &result);
	checkCLError(result, "Error during windows opencl context creation");

#	else
#	ifdef CD_PLATFORM_LINUX
	cl_context_properties contextProps[] = {
		CL_GL_CONTEXT_KHR,   (cl_context_properties)glfwGetGLXContext(interface->getWindow()),	// GLX Context
		CL_GLX_DISPLAY_KHR,  (cl_context_properties)glfwGetX11Display(),						// GLX Display
		CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
		0 };

	cl_int result;
	context = cl::Context(device, contextProps, NULL, NULL, &result);
	checkCLError(result, "Error during linux opencl context creation");

#	else
	CD_ERROR("Unsupported platform: only windows and linux are supported at this time.");
	throw std::runtime_error("platform error");
#	endif // CD_PLATFORM_LINUX
#	endif // CD_PLATFORM_WINDOWS
}

void Renderer::createQueue() {
	cl_int res;
	queue = cl::CommandQueue(context, device, 0, &res); // CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
	checkCLError(res, "Failed openCL queue creation");
}

void Renderer::createBuffers(cl_GLenum gl_texture_target, cl_GLuint gl_texture, cl_GLuint gl_vert_buffer,
		std::vector<cd::Sphere>& spheres, std::vector<cd::Sphere>& lights, std::vector<cl_uchar4>&polygon_colors) {
	sphere_count = spheres.size();
	light_count = lights.size();
	polygon_count = polygon_colors.size();
	cl_int result;

	// spheres and lights
	cl_spheres = cl::Buffer(context, CL_MEM_READ_ONLY, (sphere_count + light_count) * sizeof(cd::Sphere), NULL, &result);
	CD_INFO("sphere bytes = {}", (sphere_count + light_count) * sizeof(cd::Sphere));
	checkCLError(result, "sphere buffer create");
	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(cd::Sphere), spheres.data());
	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, sphere_count * sizeof(cd::Sphere), light_count * sizeof(cd::Sphere), lights.data());

	// gl vertices
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl_vert_buffer);
	cl_gl_vertices = cl::BufferGL(context, CL_MEM_READ_WRITE, gl_vert_buffer, &result);
	checkCLError(result, "gl vertex buffer create");

	// polygons
	cl_polygons = cl::Buffer(context, CL_MEM_READ_ONLY, polygon_count * sizeof(cl_uchar4), NULL, &result);
	CD_INFO("polygon bytes = {}", polygon_count * sizeof(cl_uchar4));
	checkCLError(result, "polygon buffer create");
	queue.enqueueWriteBuffer(cl_polygons, CL_TRUE, 0, polygon_count * sizeof(cl_uchar4), polygon_colors.data());

	// output image
	createOutputImage(gl_texture_target, gl_texture);
}

void Renderer::createOutputImage(cl_GLenum gl_texture_target, cl_GLuint gl_texture) {
	cl_int result;
	glBindTexture(gl_texture_target, gl_texture);
	cl_output = cl::ImageGL(context, CL_MEM_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS, gl_texture_target, 0, gl_texture, &result);
	checkCLError(result, "Error during cl_output creation");
}

void Renderer::setGLObjects() {
	gl_objects.push_back(cl_gl_vertices);
	gl_objects.push_back(cl_output);
}

void Renderer::createKernels() {

	createKernel(KERNEL_PATH, kernel, KERNEL_ENTRY);

	/* arg 0 = viewer position */
	/* arg 1 = view matrix */
	/* arg 2 = time (s) */

	kernel.setArg(3, sphere_count);
	kernel.setArg(4, light_count);
	kernel.setArg(5, polygon_count);

	kernel.setArg(6, cl_spheres);
	kernel.setArg(7, cl_gl_vertices);
	kernel.setArg(8, cl_polygons);

	setOutArg();
}

void Renderer::setOutArg() {
	kernel.setArg(9, cl_output);
}

void Renderer::createKernel(const char* filename, cl::Kernel& kernel, const char* entryPoint) {

	// Convert the OpenCL source code to a string
	std::string source;
	std::ifstream file(filename);
	if (!file) {
		CD_ERROR("{} file not found!\nExiting...", filename);
		throw std::runtime_error("error: missing kernel file");
	}
	while (!file.eof()) {
		char line[256];
		file.getline(line, 255);
		source += line;
		source += "\n";
	}

	const char* kernel_source = source.c_str();

	// compiler options
	std::string options = "-cl-fast-relaxed-math -cl-denorms-are-zero -Werror"; //  -cl-std=CL1.2

	// Create an OpenCL program by performing runtime source compilation for the chosen device
	cl::Program program = cl::Program(context, kernel_source);
	cl_int result = program.build({ device }, options.c_str());
	if (result) CD_ERROR("Error during openCL compilation {} error: ({})", filename, result);
	if (result == CL_BUILD_PROGRAM_FAILURE) printErrorLog(program, device);

	// Create a kernel (entry point in the OpenCL source program)
	kernel = cl::Kernel(program, entryPoint);
}

void Renderer::setWorkGroupSizes() {

	// TODO: query CL_DEVICE_MAX_WORK_GROUP_SIZE
	int x = 16;
	int y = 16;

#	ifdef HALF_RESOLUTION
	global_work = cl::NDRange(image_width / 2, image_height / 2);
#	else
	global_work = cl::NDRange(image_width, image_height);
#	endif
	local_work = cl::NDRange(x, y);
}

// HELPER FUNCTIONS

void Renderer::checkCLError(cl_int err, std::string message) {
	if (err) {
		CD_ERROR("{}. error code = ({})", message, err);
		throw std::runtime_error("renderer error");
	}
}

void Renderer::printErrorLog(const cl::Program& program, const cl::Device& device) {

	// Get the error log and print to console
	std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
	CD_ERROR("Build log:\n" + buildlog);

	throw std::runtime_error("opencl compilation error");
}

/*
notes:
cpp reference: https://github.khronos.org/OpenCL-CLHPP/
html spec: https://www.khronos.org/registry/OpenCL/specs/2.2/html/OpenCL_API.html

vector data types: http://www.informit.com/articles/article.aspx?p=1732873&seqNum=3
work group info: https://stackoverflow.com/questions/3957125/questions-about-global-and-local-work-size

CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS = 3
CL_DEVICE_MAX_WORK_ITEM_SIZES = {256, 256, 256}
CL_DEVICE_MAX_WORK_GROUP_SIZE = 256 (>= product of work item sizes)

allocate local mem: http://www.openclblog.com/2014/10/allocating-local-memory.html
buffer vs image: https://stackoverflow.com/questions/9903855/buffer-object-and-image-buffer-object-in-opencl
buffer image local: https://community.amd.com/thread/169203
global to local: https://stackoverflow.com/questions/17724836/how-do-i-make-a-strided-copy-from-global-to-local-memory

intensity write: https://www.khronos.org/registry/OpenCL/sdk/1.0/docs/man/xhtml/write_image.html
async_work_group_copy struct: https://stackoverflow.com/questions/37981455/using-async-work-group-copy-with-a-custom-data-type

opencl with opengl: https://software.intel.com/en-us/articles/opencl-and-opengl-interoperability-tutorial
intel opencl reading material: https://software.intel.com/en-us/iocl-opg

context creation: http://sa10.idav.ucdavis.edu/docs/sa10-dg-opencl-gl-interop.pdf
use opengl event: https://software.intel.com/en-us/articles/sharing-surfaces-between-opencl-and-opengl-43-on-intel-processor-graphics-using-implicit
cl_unorm_8: https://stackoverflow.com/questions/31718492/meaning-of-cl-unorm-int8-for-cl-image-format-image-channel-data-type-and-its-di

opencl raytracer: https://www.reddit.com/r/raytracing/comments/55wqu7/my_realtime_opencl_ray_tracer_uses_bvh/
one kernel or multiple? register pressure https://stackoverflow.com/questions/9504828/write-multiple-kernels-or-a-single-kernel

INTEL OCL OPTIMIZATION RECOMMENDATIONS: https://software.intel.com/en-us/iocl-opg
ocl2 in kernel queueing (advenced pipeline): https://software.intel.com/en-us/articles/gpu-quicksort-in-opencl-20-using-nested-parallelism-and-work-group-scan-functions
*/
