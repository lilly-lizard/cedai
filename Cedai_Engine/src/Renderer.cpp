// OpenCL ray tracing tutorial by Sam Lapere, 2016
// http://raytracey.blogspot.com

#include <iostream>
#include <fstream>

#include "Renderer.h"

using namespace std;
using namespace cl;

#define KERNEL_PATH "src/kernels/opencl_kernel.cl"

void Renderer::init(int image_width, int image_height) {
	this->image_width = image_width;
	this->image_height = image_height;

	// Get all available OpenCL platforms (e.g. AMD OpenCL, Nvidia CUDA, Intel OpenCL)
	vector<Platform> platforms;
	Platform::get(&platforms);
	cout << "Available OpenCL platforms : " << endl << endl;
	for (int i = 0; i < platforms.size(); i++) {
		cout << "\t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << endl;
	}

	// Pick one platform
	Platform platform;
	pickPlatform(platform, platforms);
	cout << "\nUsing OpenCL platform: \t" << platform.getInfo<CL_PLATFORM_NAME>() << endl;

	// Get available OpenCL devices on platform
	vector<Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

	cout << "Available OpenCL devices on this platform: " << endl << endl;
	for (int i = 0; i < devices.size(); i++) {
		cout << "\t" << i + 1 << ": " << devices[i].getInfo<CL_DEVICE_NAME>() << endl;
		cout << "\t\tMax compute units: " << devices[i].getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << endl;
		cout << "\t\tMax work group size: " << devices[i].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl << endl;
	}

	// Pick one device
	Device device;
	pickDevice(device, devices);
	cout << "\nUsing OpenCL device: \t" << device.getInfo<CL_DEVICE_NAME>() << endl;
	cout << "\t\t\tMax compute units: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << endl;
	cout << "\t\t\tMax work group size: " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl;

	// Create an OpenCL context and command queue on that device.
	context = Context(device);
	queue = CommandQueue(context, device);

	// Convert the OpenCL source code to a string
	string source;
	ifstream file(KERNEL_PATH);
	if (!file) {
		cout << "\nNo OpenCL file found!" << endl << "Exiting..." << endl;
		system("PAUSE");
		exit(1);
	}
	while (!file.eof()) {
		char line[256];
		file.getline(line, 255);
		source += line;
	}

	const char* kernel_source = source.c_str();

	// Create an OpenCL program by performing runtime source compilation for the chosen device
	program = Program(context, kernel_source);
	cl_int result = program.build({ device });
	if (result) cout << "Error during compilation OpenCL code!!!\n (" << result << ")" << endl;
	if (result == CL_BUILD_PROGRAM_FAILURE) printErrorLog(program, device);

	// Create a kernel (entry point in the OpenCL source program)
	kernel = Kernel(program, "render_kernel");

	// allocate memory on CPU to hold image
	cpu_output = new cl_float3[image_width * image_height];

	// Create image buffer on the OpenCL device
	cl_output = Buffer(context, CL_MEM_WRITE_ONLY, image_width * image_height * sizeof(cl_float3));

	// specify OpenCL kernel arguments
	kernel.setArg(0, cl_output);
	kernel.setArg(1, image_width);
	kernel.setArg(2, image_height);

	// every pixel in the image has its own thread or "work item",
	// so the total amount of work items equals the number of pixels
	global_work_size = image_width * image_height;
	local_work_size = 64;
}

void Renderer::draw(float *pixels) {

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

void Renderer::pickPlatform(Platform& platform, const vector<Platform>& platforms) {
	platform = platforms[0];
	return;
}

void Renderer::pickDevice(Device& device, const vector<Device>& devices) {

	if (devices.size() == 1) device = devices[0];
	else {
		for (Device dev : devices) {
			if (dev.getInfo< CL_DEVICE_TYPE>() == CL_DEVICE_TYPE_GPU) {
				device = dev;
				return;
			}
		}
		throw std::runtime_error("error: no suitable device found");
	}
}

void Renderer::printErrorLog(const Program& program, const Device& device) {

	// Get the error log and print to console
	string buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
	cerr << "Build log:" << std::endl << buildlog << std::endl;

	// Print the error log to a file
	FILE *log;
	errno_t err = fopen_s(&log, "errorlog.txt", "w");
	if (err == 0) {
		fprintf(log, "%s\n", buildlog);
		cout << "Error log saved in 'errorlog.txt'" << endl;
	} else {
		cout << "Error: could not save errorlog to 'errorlog.txt'" << endl;
	}
	system("PAUSE");
	exit(1);
}
