
#include "Renderer.h"
#include "tools/Log.h"

#include <iostream>
#include <fstream>

#define KERNEL_PATH "src/kernels/opencl_kernel.cl"
#define RAY_GEN_PATH "src/kernels/ray_gen.cl"

void Renderer::init(int image_width, int image_height) {
	CD_INFO("Initialising renderer...");

	this->image_width = image_width;
	this->image_height = image_height;

	// Get all available OpenCL platforms (e.g. AMD OpenCL, Nvidia CUDA, Intel OpenCL)
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	std::cout << "~ Available OpenCL platforms : \n~ \n";
	for (int i = 0; i < platforms.size(); i++)
		std::cout << "~ \t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << std::endl;

	// Pick one platform
	cl::Platform platform;
	pickPlatform(platform, platforms);
	std::cout << "~ \n~ Using OpenCL platform: \t" << platform.getInfo<CL_PLATFORM_NAME>() << std::endl;

	// Get available OpenCL devices on platform
	std::vector<cl::Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

	std::cout << "~ Available OpenCL devices on this platform: " << "\n~ \n";
	for (int i = 0; i < devices.size(); i++) {
		std::cout << "~ \t" << i + 1 << ": " << devices[i].getInfo<CL_DEVICE_NAME>() << std::endl;
		std::cout << "~ \t\tMax compute units: " << devices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << std::endl;
		std::cout << "~ \t\tMax work group size: " << devices[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << "\n~ \n";
	}

	// Pick one device
	pickDevice(device, devices);
	std::cout << "~ \n~ Using OpenCL device: \t" << device.getInfo<CL_DEVICE_NAME>() << std::endl;
	std::cout << "~ \t\t\tMax compute units: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << std::endl;
	std::cout << "~ \t\t\tMax work group size: " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << std::endl;

	// Create an OpenCL context and command queue on that device.
	context = cl::Context(device);
	queue = cl::CommandQueue(context, device);

	createKernel(RAY_GEN_PATH, rayGenKernel, "ray_gen");
	createKernel(KERNEL_PATH, mainKernel, "render_kernel");

	// spheres and lights
	createSpheres();
	createLights();
	cl_spheres = cl::Buffer(context, CL_MEM_READ_ONLY, sphere_count * sizeof(Sphere));
	cl_lights = cl::Buffer(context, CL_MEM_READ_ONLY, light_count * sizeof(Sphere));
	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);
	queue.enqueueWriteBuffer(cl_lights, CL_TRUE, 0, light_count * sizeof(Sphere), cpu_lights);

	// inter-kernel buffers
	cl_rays = cl::Buffer(context, CL_MEM_READ_WRITE, image_width * image_height * sizeof(cl_float3));

	// output image
	cpu_output = new cl_float3[image_width * image_height];
	cl_output = cl::Buffer(context, CL_MEM_WRITE_ONLY, image_width * image_height * sizeof(cl_float3));

	/* rayGenKernel arg 0 = view matrix */
	rayGenKernel.setArg(1, image_width);
	rayGenKernel.setArg(2, image_height);
	rayGenKernel.setArg(3, cl_rays);

	/* mainKernel arg 0 = view matrix */
	mainKernel.setArg(1, image_width);
	mainKernel.setArg(2, image_height);
	mainKernel.setArg(3, cl_rays);
	mainKernel.setArg(4, cl_spheres);
	mainKernel.setArg(5, sphere_count);
	mainKernel.setArg(6, cl_lights);
	mainKernel.setArg(7, light_count);
	mainKernel.setArg(8, cl_output);

	global_work_size = image_width * image_height;
	local_work_size = 64;
}

void Renderer::render(float *pixels, const float view[4][4]) {
	cl_float16 cl_view = {{	view[0][0], view[0][1], view[0][2], view[0][3],
							view[1][0], view[1][1], view[1][2], view[1][3],
							view[2][0], view[2][1], view[2][2], view[2][3],
							view[3][0], view[3][1], view[3][2], view[3][3] }};

	rayGenKernel.setArg(0, cl_view);
	mainKernel.setArg(0, cl_view);

	// launch the kernels
	queue.enqueueNDRangeKernel(rayGenKernel, NULL, global_work_size, local_work_size);
	queue.finish();
	queue.enqueueNDRangeKernel(mainKernel, NULL, global_work_size, local_work_size);
	queue.finish();

	// read and copy OpenCL output to CPU
	queue.enqueueReadBuffer(cl_output, CL_TRUE, 0, image_width * image_height * sizeof(cl_float3), cpu_output);

	for (int i = 0; i < image_width * image_height; i++) {
		pixels[3 * i]	  = cpu_output[i].s[0];
		pixels[3 * i + 1] =	cpu_output[i].s[1];
		pixels[3 * i + 2] =	cpu_output[i].s[2];
	}
}

void Renderer::pickPlatform(cl::Platform& platform, const std::vector<cl::Platform>& platforms) {
	platform = platforms[0];
	return;
}

void Renderer::pickDevice(cl::Device& device, const std::vector<cl::Device>& devices) {

	if (devices.size() == 1) device = devices[0];
	else {
		for (cl::Device dev : devices) {
			if (dev.getInfo< CL_DEVICE_TYPE>() == CL_DEVICE_TYPE_GPU) {
				device = dev;
				return;
			}
		}
		throw std::runtime_error("error: no suitable device found");
	}
}

void Renderer::createKernel(const char* filename, cl::Kernel& kernel, const char* entryPoint) {
	// Convert the OpenCL source code to a string
	std::string source;
	std::ifstream file(filename);
	if (!file) {
		CD_ERROR("\nNo OpenCL file found!\NExiting...");
		throw std::runtime_error("error: missing kernel file");
	}
	while (!file.eof()) {
		char line[256];
		file.getline(line, 255);
		source += line;
		source += "\n";
	}

	const char* kernel_source = source.c_str();

	// Create an OpenCL program by performing runtime source compilation for the chosen device
	program = cl::Program(context, kernel_source);
	cl_int result = program.build({ device });
	if (result) CD_WARN("Error during compilation OpenCL code!\n ({})", result);
	if (result == CL_BUILD_PROGRAM_FAILURE) printErrorLog(program, device);

	// Create a kernel (entry point in the OpenCL source program)
	kernel = cl::Kernel(program, entryPoint);
}

void Renderer::createSpheres() {

	sphere_count = 3;
	cpu_spheres = new Sphere[sphere_count];

	cpu_spheres[0].radius	= 1.0;
	cpu_spheres[0].position = {{ 10, 3, 0 }};
	cpu_spheres[0].color	= {{ 0.9, 0.5, 0.5 }};

	cpu_spheres[1].radius	= 0.5;
	cpu_spheres[1].position = {{ 4, -1, 1 }};
	cpu_spheres[1].color	= {{ 1.0, 1.0, 0.5 }};

	cpu_spheres[2].radius	= 0.2;
	cpu_spheres[2].position = {{ 5, -2, -1 }};
	cpu_spheres[2].color	= {{ 0.5, 0.5, 0.9 }};
}

void Renderer::createLights() {

	light_count = 2;
	cpu_lights = new Sphere[light_count];

	cpu_lights[0].radius = 0.1;
	cpu_lights[0].position = { { 5, 1, 2 } };
	cpu_lights[0].color = { { 1.0, 1.0, 0.8 } };

	cpu_lights[1].radius = 0.1;
	cpu_lights[1].position = { { 4, -2, -2 } };
	cpu_lights[1].color = { { 1.0, 1.0, 0.8 } };
}

void Renderer::printErrorLog(const cl::Program& program, const cl::Device& device) {

	// Get the error log and print to console
	std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
	CD_ERROR("Build log:\n" + buildlog);

	throw std::runtime_error("opencl compilation error");
}

void Renderer::cleanUp() {
	delete cpu_output;
	delete cpu_spheres;
	delete cpu_lights;
}