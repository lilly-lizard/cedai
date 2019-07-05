// OpenCL ray tracing tutorial by Sam Lapere, 2016
// http://raytracey.blogspot.com

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL\cl.hpp>

#include "opencl.h"
#include "c_sdl.h"

using namespace std;
using namespace cl;
using namespace std::chrono;

const int image_width = 1280;
const int image_height = 720;

#define KERNEL_PATH "src/kernels/opencl_kernel.cl"

cl_float4* cpu_output;
CommandQueue queue;
Kernel kernel;
Context context;
Program program;
Buffer cl_output;

void pickPlatform(Platform& platform, const vector<Platform>& platforms) {

	if (platforms.size() == 1) platform = platforms[0];
	else {
		int input = 0;
		cout << "\nChoose an OpenCL platform: ";
		cin >> input;

		// handle incorrect user input
		while (input < 1 || input > platforms.size()) {
			cin.clear(); //clear errors/bad flags on cin
			cin.ignore(cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
			cout << "No such option. Choose an OpenCL platform: ";
			cin >> input;
		}
		platform = platforms[input - 1];
	}
}

void pickDevice(Device& device, const vector<Device>& devices) {

	if (devices.size() == 1) device = devices[0];
	else {
		int input = 0;
		cout << "\nChoose an OpenCL device: ";
		cin >> input;

		// handle incorrect user input
		while (input < 1 || input > devices.size()) {
			cin.clear(); //clear errors/bad flags on cin
			cin.ignore(cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
			cout << "No such option. Choose an OpenCL device: ";
			cin >> input;
		}
		device = devices[input - 1];
	}
}

void printErrorLog(const Program& program, const Device& device) {

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

void selectRenderMode(unsigned int& rendermode) {
	cout << endl << "Rendermodes: " << endl << endl;
	cout << "\t(1) Simple gradient" << endl;
	cout << "\t(2) Sphere with plain colour" << endl;
	cout << "\t(3) Sphere with cosine weighted colour" << endl;
	cout << "\t(4) Stripey sphere" << endl;
	cout << "\t(5) Sphere with screen door effect" << endl;
	cout << "\t(6) Sphere with normals" << endl;

	unsigned int input;
	cout << endl << "Select rendermode (1-6): ";
	cin >> input;

	// handle incorrect user input
	while (input < 1 || input > 6) {
		cin.clear(); //clear errors/bad flags on cin
		cin.ignore(cin.rdbuf()->in_avail(), '\n'); // ignores exact number of chars in cin buffer
		cout << "No such option. Select rendermode: ";
		cin >> input;
	}
	rendermode = input;
}

void initOpenCL()
{
	// Get all available OpenCL platforms (e.g. AMD OpenCL, Nvidia CUDA, Intel OpenCL)
	vector<Platform> platforms;
	Platform::get(&platforms);
	cout << "Available OpenCL platforms : " << endl << endl;
	for (int i = 0; i < platforms.size(); i++)
		cout << "\t" << i + 1 << ": " << platforms[i].getInfo<CL_PLATFORM_NAME>() << endl;

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
}

inline float clamp(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

// convert RGB float in range [0,1] to int in range [0, 255]
inline int toInt(float x) { return int(clamp(x) * 255 + .5); }

void saveImage() {

	// write image to PPM file, a very simple image file format
	// PPM files can be opened with IrfanView (download at www.irfanview.com) or GIMP
	cout << "Saving image..." << endl;

	FILE *f;
	errno_t err = fopen_s(&f, "opencl_raytracer.ppm", "w");
	if (err == 0) {
		fprintf(f, "P3\n%d %d\n%d\n", image_width, image_height, 255);

		// loop over pixels, write RGB values
		for (int i = 0; i < image_width * image_height; i++)
			fprintf(f, "%d %d %d ",
				toInt(cpu_output[i].s[0]),
				toInt(cpu_output[i].s[1]),
				toInt(cpu_output[i].s[2]));

		cout << "Saved image to 'opencl_raytracer.ppm'" << endl;
	} else {
		cout << "Error: could not write to 'opencl_raytracer.ppm'" << endl;
	}
}

void cleanUp() {
	delete cpu_output;
}

void opencl() {

	// allocate memory on CPU to hold image
	cpu_output = new cl_float3[image_width * image_height];

	// initialise OpenCL
	initOpenCL();

	// Create image buffer on the OpenCL device
	cl_output = Buffer(context, CL_MEM_WRITE_ONLY, image_width * image_height * sizeof(cl_float3));

	// pick a rendermode
	unsigned int rendermode;
	selectRenderMode(rendermode);

	// specify OpenCL kernel arguments
	kernel.setArg(0, cl_output);
	kernel.setArg(1, image_width);
	kernel.setArg(2, image_height);
	kernel.setArg(3, rendermode);

	// every pixel in the image has its own thread or "work item",
	// so the total amount of work items equals the number of pixels
	std::size_t global_work_size = image_width * image_height;
	std::size_t local_work_size = 64;

	string input = "0";
	while (input == "0") {

		// start timer
		time_point<system_clock> startTime = system_clock::now();

		// launch the kernel
		queue.enqueueNDRangeKernel(kernel, NULL, global_work_size, local_work_size);
		queue.finish();

		// stop timer
		duration<double> elapsedTime = system_clock::now() - startTime;

		// read and copy OpenCL output to CPU
		queue.enqueueReadBuffer(cl_output, CL_TRUE, 0, image_width * image_height * sizeof(cl_float3), cpu_output);
		cout << "Rendering done! It took: " << elapsedTime.count() << "s" << endl;

		// save image to PPM format
		saveImage();

		cout << "\nInput 0 to repeat: ";
		cin >> input;
		cout << "\n";
	}

	// release memory
	cleanUp();
}

void main() {
	run_sdl();
}