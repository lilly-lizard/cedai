#include "Cedai.hpp"

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <iomanip>
#include <math.h>

#include "model/Model_Loader.hpp"
#include "tools/Inputs.hpp"
#include "tools/Log.hpp"

//#define PRINT_FPS

using namespace std::chrono;

#define MAIZE_FILE "../assets/maize_v1.bin"

// MAIN FUNCTIONS

int main() {
	Cedai App;
	try {
		App.Run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		//system("PAUSE");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

void Cedai::Run() {
	init();

	loop();

	//cleanUp();
}

Cedai::~Cedai() {
	CD_INFO("Application cleaning up...");
	cleanUp();
}

void Cedai::init() {

	Log::Init();
	CD_INFO("Logger initialised");

#	ifndef CD_PLATFORM_WINDOWS
#	ifndef CD_PLATFORM_LINUX
	CD_ERROR("Unsupported platform: only windows and linux are supported at this time.");
	throw std::runtime_error("platform error");
#	endif // CD_PLATFORM_LINUX
#	endif // CD_PLATFORM_WINDOWS

	windowWidth = INIT_SCREEN_WIDTH;
	windowHeight = INIT_SCREEN_HEIGHT;

	interface.init(this, windowWidth, windowHeight);
	CD_INFO("Interface initialised.");

	createPrimitives();
	vertexProcessor.init(&interface, maize.vertices, maize.GetBoneTransforms());
	CD_INFO("Pimitive processing program initialised.");

	renderer.init(windowWidth, windowHeight, &interface, &vertexProcessor,
		spheres, lights, cl_polygonColors);
	CD_INFO("Renderer initialised.");

	view[0][0] = 1; view[1][1] = 1; view[2][2] = 1;
	CD_INFO("Engine initialised.");
	quit = false;
}

void Cedai::loop() {
	time_point<high_resolution_clock> timeStart = high_resolution_clock::now();

	while (!quit && !interface.WindowCloseCheck()) {
		// window resize check
#		ifdef RESIZABLE
		resizeCheck();
#		endif
		
		// 1) transform vertices
		vertexProcessor.vertexProcess(maize.GetBoneTransforms());
		vertexProcessor.vertexBarrier();
		
		// 2) queue a render operation
		double time = duration<double, seconds::period>(high_resolution_clock::now() - timeStart).count();
		renderer.renderQueue(view, (float)time);

		// input handling
		interface.PollEvents();
		inputs = interface.GetKeyInputs();
		quit |= (bool)(inputs & CD_INPUTS::ESC);

		// game logic
		processInputs();
		//updateAnimation(time);
		fpsHandle();

		renderer.renderBarrier();
		// 3) draw to window
		interface.drawRun();
		interface.drawBarrier();
	}
}

void Cedai::cleanUp() {
	CD_INFO("Average fps = {}", fpsSum / fpsCount);

	renderer.cleanUp();
	vertexProcessor.cleanUp();
	interface.cleanUp();

	CD_INFO("Finished cleaning.");
}

// INITIALIZATION

void Cedai::createPrimitives() {

	// spheres

	spheres.push_back(cd::Sphere(1.0,
		cl_float3{ { 3, 2, -4 } },
		cl_uint4{ { 200, 128, 254, 255 } }));

	spheres.push_back(cd::Sphere(0.5,
		cl_float3{ { -2, 2, 2.5 } },
		cl_uint4{ { 128, 255, 180, 255 } }));

	spheres.push_back(cd::Sphere(0.2,
		cl_float3{ { 6, 3, 3 } },
		cl_uint4{ { 255, 230, 80, 255 } }));

	// lights

	lights.push_back(cd::Sphere(0.1,
		cl_float3{ { 2, 3, 4 } },
		cl_uint4{ { 255, 255, 205, 255 } }));

	lights.push_back(cd::Sphere(0.1,
		cl_float3{ { -1, -6, -4 } },
		cl_uint4{ { 255, 255, 205, 255 } }));

	// animated model

	CD_INFO("loading model(s)...");

	cd::LoadModelv1(MAIZE_FILE, maize);
	for (int p = 0; p < maize.vertices.size() / 3; p++)
		cl_polygonColors.push_back(cl_uchar4{ { 200, 200, 200, 255 } });

	if (maize.vertices.size() != cl_polygonColors.size() * 3)
		CD_WARN("Cedai model init warning: unequal number of vertices for the number of polygons");

	CD_INFO("model(s) loaded.");

	CD_INFO("number of vertices = {}", maize.vertices.size());
	CD_INFO("number of polygons = {}", cl_polygonColors.size());
	CD_INFO("animation period = {}", maize.animation.duration);
}

// GAME LOGIC

void Cedai::resizeCheck() {
	if (windowResized) {
		CD_WARN("window resizing...");
		windowResized = false;

		interface.resize(windowWidth, windowHeight);
		renderer.resize(windowWidth, windowHeight, &interface);
	}

}

void Cedai::processInputs() {
	// get time difference
	static time_point<high_resolution_clock> timePrev = high_resolution_clock::now();
	double timeDif = duration<double, seconds::period>(high_resolution_clock::now() - timePrev).count();
	timePrev = high_resolution_clock::now();

	// get mouse inputs from window interface
	double mouseMovement[2];
	interface.GetMouseChange(mouseMovement[0], mouseMovement[1]);

	// LOOK

	float viewAngleHoriz = (double)mouseMovement[0] * radiansPerMousePosYaw;
	viewerForward = glm::rotate(viewerForward, viewAngleHoriz, viewerUp);
	viewerCross = glm::cross(viewerUp, viewerForward);

	float viewAngleVert = (double)mouseMovement[1] * radiansPerMousePosPitch;
	viewerForward = glm::rotate(viewerForward, viewAngleVert, viewerCross);
	viewerUp = glm::cross(viewerForward, viewerCross);

	if ((bool)(inputs & CD_INPUTS::ROTATEL) != (bool)(inputs & CD_INPUTS::ROTATER)) {
		float viewAngleFront = radiansPerSecondRoll * timeDif * (inputs & CD_INPUTS::ROTATEL ? 1 : -1);
		viewerUp = glm::rotate(viewerUp, viewAngleFront, viewerForward);
		viewerCross = glm::cross(viewerUp, viewerForward);
	}

	static uint32_t frameCount = 0;
	frameCount++;
	if (frameCount % 30 == 0) {
		viewerForward = glm::normalize(viewerForward);
		viewerCross = glm::normalize(viewerCross);
		viewerUp = glm::normalize(viewerUp);
		frameCount = 0;
	}

	// POSITION

	if ((bool)(inputs & CD_INPUTS::FORWARD) != (bool)(inputs & CD_INPUTS::BACKWARD))
		viewerPosition += viewerForward * glm::vec3(inputs & CD_INPUTS::FORWARD ? forwardSpeed * timeDif : -backSpeed * timeDif);

	if ((bool)(inputs & CD_INPUTS::LEFT) != (bool)(inputs & CD_INPUTS::RIGHT))
		viewerPosition += viewerCross * glm::vec3(inputs & CD_INPUTS::RIGHT ? strafeSpeed * timeDif : -strafeSpeed * timeDif);

	if ((bool)(inputs & CD_INPUTS::UP) != (bool)(inputs & CD_INPUTS::DOWN))
		viewerPosition += viewerUp * glm::vec3(inputs & CD_INPUTS::UP ? strafeSpeed * timeDif : -strafeSpeed * timeDif);

	updateView();
}

void Cedai::updateAnimation(double time) {
	double relativeTime = fmod(time, maize.animation.duration);

	// figure out which frame we're on
	for (int f = 0; f < maize.animation.keyframes.size() - 1; f++) {
		if (maize.animation.keyframes[f].time <= relativeTime && relativeTime < maize.animation.keyframes[f + 1].time) {
			maize.keyFrameIndex = f;
			break;
		}
	}
}

// HELPER

void Cedai::windowResizeCallback(GLFWwindow *window, int width, int height) {
	Cedai *application = reinterpret_cast<Cedai *>(glfwGetWindowUserPointer(window));
	application->windowResized = true;
}

void Cedai::updateView() {
	view[0][0] = viewerForward[0];
	view[0][1] = viewerForward[1];
	view[0][2] = viewerForward[2];

	view[1][0] = viewerCross[0];
	view[1][1] = viewerCross[1];
	view[1][2] = viewerCross[2];

	view[2][0] = viewerUp[0];
	view[2][1] = viewerUp[1];
	view[2][2] = viewerUp[2];

	view[3][0] = viewerPosition[0];
	view[3][1] = viewerPosition[1];
	view[3][2] = viewerPosition[2];
}

void Cedai::fpsHandle() {
	static int fps = 0;
	static time_point<system_clock> prevTime = system_clock::now();

	fps++;
	duration<double> elapsedTime;
	elapsedTime = system_clock::now() - prevTime;
	if (elapsedTime.count() > 1) {

#		ifdef PRINT_FPS
		CD_TRACE("fps = {}", fps);
#		endif
#		ifdef DEBUG
		interface.showFPS(fps);
#		endif

		fpsSum += fps;
		fpsCount++;
		prevTime = system_clock::now();
		fps = 0;
	}
}

void Cedai::printViewData() {
	CD_TRACE("forward:	{:+>9.6f} {:+>9.6f} {:+>9.6f}", viewerForward[0], viewerForward[1], viewerForward[2]);
	CD_TRACE("cross:	{:+>9.6f} {:+>9.6f} {:+>9.6f}", viewerCross[0], viewerCross[1], viewerCross[2]);
	CD_TRACE("up:		{:+>9.6f} {:+>9.6f} {:+>9.6f}", viewerUp[0], viewerUp[1], viewerUp[2]);
}
