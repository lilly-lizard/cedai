
#include "Renderer.h"
#include "tools/Log.h"

#include <iostream>
#include <fstream>

#define RAY_GEN_PATH "src/kernels/ray_gen.cl"
#define SPHERE_PATH "src/kernels/sphere_intersect.cl"
#define DRAW_PATH "src/kernels/draw.cl"

void Renderer::init(int image_width, int image_height, uint8_t *pixels) {
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

	createKernel(RAY_GEN_PATH, rayGenKernel, "entry");
	createKernel(SPHERE_PATH, sphereKernel, "entry");
	createKernel(DRAW_PATH, drawKernel, "entry");

	// spheres and lights
	createSpheres();
	createLights();
	cl_spheres = cl::Buffer(context, CL_MEM_READ_ONLY, (sphere_count + light_count) * sizeof(Sphere));
	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, 0, sphere_count * sizeof(Sphere), cpu_spheres);
	queue.enqueueWriteBuffer(cl_spheres, CL_TRUE, sphere_count * sizeof(Sphere), light_count * sizeof(Sphere), cpu_lights);

	// inter-kernel buffers
	cl_rays = cl::Buffer(context, CL_MEM_READ_WRITE, image_width * image_height * sizeof(cl_float3));
	cl_sphere_t = cl::Buffer(context, CL_MEM_READ_WRITE, image_width * image_height * (sphere_count + light_count) * sizeof(cl_float));

	// output image
	cl_output = cl::Buffer(context, CL_MEM_WRITE_ONLY, image_width * image_height * sizeof(cl_uchar4));
	static cl_uchar4* const cpu_output_temp = new cl_uchar4[image_width * image_height]; // to ensure the address of cpu_output doesn't get changed by the opencl library
	cpu_output = cpu_output_temp;

	void* ptr1 = (void*)pixels;
	void* ptr2 = (void*)cpu_output;
	if (ptr1 == ptr2) {
		CD_ERROR("ALLOCATION ERROR - Cedai.pixels and Renderer.cpu_output are assigned to same address");
		throw("init allocation error");
	}

	/* rayGenKernel arg 0 = view matrix */
	rayGenKernel.setArg(1, cl_rays);

	/* sphereKernel arg 0 = view position */
	sphereKernel.setArg(1, cl_spheres);
	sphereKernel.setArg(2, cl_rays);
	sphereKernel.setArg(3, cl_sphere_t);

	/* drawKernel arg 0 = view position */
	drawKernel.setArg(1, cl_spheres);
	drawKernel.setArg(2, sphere_count);
	drawKernel.setArg(3, light_count);
	drawKernel.setArg(4, cl_rays);
	drawKernel.setArg(5, cl_sphere_t);
	drawKernel.setArg(6, cl_output);

	// TODO: query CL_DEVICE_MAX_WORK_GROUP_SIZE
	global_work_pixels	= cl::NDRange(image_width, image_height);
	local_work_pixels	= cl::NDRange(image_width  % 16 == 0 ? 16 : 1,
									  image_height % 16 == 0 ? 16 : 1);

	global_work_spheres = cl::NDRange(image_width, image_height, sphere_count + light_count);
	local_work_spheres  = cl::NDRange(image_width  % 16 == 0 ? 16 : 1,
									  image_height % 16 == 0 ? 16 : 1,
									  1);
}

void Renderer::render(uint8_t *pixels, const float view[4][4]) {
	cl_view = {{ view[0][0], view[0][1], view[0][2], 0,
				 view[1][0], view[1][1], view[1][2], 0,
				 view[2][0], view[2][1], view[2][2], 0,
				 0, 0, 0, 0 }};
	cl_pos = {{ view[3][0], view[3][1], view[3][2] }};

	rayGenKernel.setArg(0, cl_view);
	sphereKernel.setArg(0, cl_pos);
	drawKernel.setArg(0, cl_pos);

	// launch the kernels
	queue.enqueueNDRangeKernel(rayGenKernel, NULL, global_work_pixels, local_work_pixels);
	queue.finish();
	queue.enqueueNDRangeKernel(sphereKernel, NULL, global_work_spheres, local_work_spheres);
	queue.finish();
	queue.enqueueNDRangeKernel(drawKernel, NULL, global_work_pixels, local_work_pixels);
	queue.finish();

	// read and copy OpenCL output to CPU
	queue.enqueueReadBuffer(cl_output, CL_TRUE, 0, image_width * image_height * sizeof(cl_uchar4), cpu_output);

	for (int p = 0; p < image_width * image_height; p++) {
		pixels[p * 4] = 0; // A
		pixels[p * 4 + 1] = cpu_output[p].s[2]; // B
		pixels[p * 4 + 2] = cpu_output[p].s[1]; // G
		pixels[p * 4 + 3] = cpu_output[p].s[0]; // R
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

	// Create an OpenCL program by performing runtime source compilation for the chosen device
	program = cl::Program(context, kernel_source);
	cl_int result = program.build({ device });
	if (result) CD_ERROR("Error during openCL compilation {} error: ({})", filename, result);
	if (result == CL_BUILD_PROGRAM_FAILURE) printErrorLog(program, device);

	// Create a kernel (entry point in the OpenCL source program)
	kernel = cl::Kernel(program, entryPoint);
}

void Renderer::createSpheres() {

	sphere_count = 3;
	cpu_spheres = new Sphere[sphere_count];

	cpu_spheres[0].radius	= 1.0;
	cpu_spheres[0].position = {{ 10, 3, 0 }};
	cpu_spheres[0].color	= {{ 230, 128, 128 }};

	cpu_spheres[1].radius	= 0.5;
	cpu_spheres[1].position = {{ 4, -1, 1 }};
	cpu_spheres[1].color	= {{ 255, 255, 128 }};

	cpu_spheres[2].radius	= 0.2;
	cpu_spheres[2].position = {{ 5, -2, -1 }};
	cpu_spheres[2].color	= {{ 128, 128, 230 }};
}

void Renderer::createLights() {

	light_count = 2;
	cpu_lights = new Sphere[light_count];

	cpu_lights[0].radius = 0.1;
	cpu_lights[0].position = { { 5, 1, 2 } };
	cpu_lights[0].color = { { 255, 255, 205 } };

	cpu_lights[1].radius = 0.1;
	cpu_lights[1].position = { { 4, -2, -2 } };
	cpu_lights[1].color = { { 255, 255, 205 } };
}

void Renderer::printErrorLog(const cl::Program& program, const cl::Device& device) {

	// Get the error log and print to console
	std::string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
	CD_ERROR("Build log:\n" + buildlog);

	throw std::runtime_error("opencl compilation error");
}

void Renderer::cleanUp() {
	delete[] cpu_output;
	delete[] cpu_spheres;
	delete[] cpu_lights;
}

/*
notes:
cpp reference: https://github.khronos.org/OpenCL-CLHPP/
work group info: https://stackoverflow.com/questions/3957125/questions-about-global-and-local-work-size

CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS = 3
CL_DEVICE_MAX_WORK_ITEM_SIZES = {256, 256, 256}
CL_DEVICE_MAX_WORK_GROUP_SIZE = 256
*/