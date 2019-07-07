
#include "Renderer.h"
#include "tools/Log.h"

#include <iostream>
#include <fstream>

#define KERNEL_PATH "src/kernels/opencl_kernel.cl"

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
	cl::Device device;
	pickDevice(device, devices);
	std::cout << "~ \n~ Using OpenCL device: \t" << device.getInfo<CL_DEVICE_NAME>() << std::endl;
	std::cout << "~ \t\t\tMax compute units: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << std::endl;
	std::cout << "~ \t\t\tMax work group size: " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << std::endl;

	// Create an OpenCL context and command queue on that device.
	context = cl::Context(device);
	queue = cl::CommandQueue(context, device);

	// Convert the OpenCL source code to a string
	std::string source;
	std::ifstream file(KERNEL_PATH);
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
	kernel = cl::Kernel(program, "render_kernel");

	// allocate memory on CPU to hold image
	cpu_output = new cl_float3[image_width * image_height];
	// Create image buffer on the OpenCL device
	cl_output = cl::Buffer(context, CL_MEM_WRITE_ONLY, image_width * image_height * sizeof(cl_float3));

	// allocate memory on CPU to write spheres from
	createSpheres();
	// create sphere buffer on the OpenCL device
	cl_spheres = cl::Buffer(context, CL_MEM_READ_ONLY, sphere_count * sizeof(Sphere));
	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);

	// specify OpenCL kernel arguments
	kernel.setArg(0, cl_spheres);
	kernel.setArg(1, sphere_count);
	//arg 2 = view matrix
	kernel.setArg(3, image_width);
	kernel.setArg(4, image_height);
	kernel.setArg(5, cl_output);

	// every pixel in the image has its own thread or "work item",
	// so the total amount of work items equals the number of pixels
	global_work_size = image_width * image_height;
	local_work_size = 64;
}

void Renderer::draw(float *pixels, const float view[4][4]) {
	cl_float16 cl_view = {{	view[0][0], view[0][1], view[0][2], view[0][3],
							view[1][0], view[1][1], view[1][2], view[1][3],
							view[2][0], view[2][1], view[2][2], view[2][3],
							view[3][0], view[3][1], view[3][2], view[3][3] }};
	kernel.setArg(2, cl_view);

	// launch the kernel
	queue.enqueueNDRangeKernel(kernel, NULL, global_work_size, local_work_size);
	queue.finish();

	// read and copy OpenCL output to CPU
	queue.enqueueReadBuffer(cl_output, CL_TRUE, 0, image_width * image_height * sizeof(cl_float3), cpu_output);

	for (int i = 0; i < image_width * image_height; i++) {
		pixels[3 * i]	  = cpu_output[i].s[0];
		pixels[3 * i + 1] =	cpu_output[i].s[1];
		pixels[3 * i + 2] =	cpu_output[i].s[2];
	}
}

void Renderer::cleanUp() {
	delete cpu_output;
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

void Renderer::createSpheres() {

	sphere_count = 2;
	cpu_spheres = new Sphere[sphere_count];

	cpu_spheres[0].radius	= 0.4f;
	cpu_spheres[0].position = {{0.2f, 0.1f, -10.0f}};
	cpu_spheres[0].color	= {{0.9f, 0.3f, 0.0f}};

	cpu_spheres[1].radius	= 0.2f;
	cpu_spheres[1].position = {{-0.2f, 0.2f, -9.0f}};
	cpu_spheres[1].color	= {{0.2f, 0.8f, 0.9f}};
}

void Renderer::printErrorLog(const cl::Program& program, const cl::Device& device) {

	// Get the error log and print to console
	std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
	CD_ERROR("Build log:\n" + buildlog);

	throw std::runtime_error("opencl compilation error");
}
