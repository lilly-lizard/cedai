#include "Cedai.h"
#include "Interface.h"
#include "tools/Log.h"
#include "tools/Inputs.h"

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <iomanip>

using namespace std::chrono;

const int screen_width = 640;
const int screen_height = 480;

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

	cleanUp();
}

void Cedai::init() {

	Log::Init();
	CD_INFO("Logger initialised");

	interface.init(screen_width, screen_height);
	CD_INFO("Interface initialised.");

	createEntities();
	CD_WARN("sphere bytes: {}", spheres.size() * sizeof(Sphere));
	renderer.init(screen_width, screen_height, &interface, spheres, lights);
	CD_INFO("Renderer initialised.");

	view[0][0] = 1; view[1][1] = 1; view[2][2] = 1;
	CD_INFO("Engine initialised.");
}

void Cedai::loop() {
	bool quit = false;
	timePrev = high_resolution_clock::now();

	while (!quit && !interface.WindowCloseCheck()) {
		// queue a render operation
		renderer.queueRender(view);

		// input handling
		interface.PollEvents();
		inputs = interface.GetKeyInputs();
		quit = inputs & CD_INPUTS::ESC;
		processInputs();
		printFPS();

		// draw to the window
		renderer.queueFinish();
		interface.draw();
	}
}

void Cedai::cleanUp() {
	CD_INFO("Cleaning up...");
	CD_INFO("Average fps = {}", fpsSum / fpsCount);
	renderer.cleanUp();
	interface.cleanUp();
	CD_INFO("Finished cleaning.");
}

void Cedai::createEntities() {

	spheres.push_back(Sphere{ 1.0, 0, 0, 0,
		cl_float3{{ 10, -3, 0 }},
		cl_uchar3{{ 230, 128, 128 }} });

	spheres.push_back(Sphere{ 0.5, 0, 0, 0,
		cl_float3{{ 4, 3, 1 }},
		cl_uchar3{{ 255, 255, 128 }} });

	spheres.push_back(Sphere{ 0.2, 0, 0, 0,
		cl_float3{{ 5, 2, -1 }},
		cl_uchar3{{ 128, 128, 230 }} });

	for (int i = 0; i < 10; i++) {
		for (int j = 0; j < 10; j++) {
			spheres.push_back(Sphere{ 0.5, 0, 0, 0,
				cl_float3{{ 10, (float)i * 1.5f - 5, (float)j * 1.5f - 5 }},
				cl_uchar3{{ 128, 255, 255 }} });
		}
	}

	lights.push_back(Sphere{ 0.1, 0, 0, 0,
		cl_float3{{ 5, 4, 4 }},
		cl_uchar3{{ 255, 255, 205 }} });

	lights.push_back(Sphere{ 0.1, 0, 0, 0,
		cl_float3{{  -1, -6, -4 }},
		cl_uchar3{{ 255, 255, 205 }} });
}

void Cedai::processInputs() {
	// get time difference
	double timeDif = duration<double, seconds::period>(high_resolution_clock::now() - timePrev).count();
	timePrev = high_resolution_clock::now();

	// get mouse inputs from window interface
	double mouseMovement[2];
	interface.GetMouseChange(mouseMovement[0], mouseMovement[1]);

	// LOOK

	float viewAngleHoriz = (double)mouseMovement[0] * radiansPerMousePosHoriz;
	viewerForward = glm::rotate(viewerForward, viewAngleHoriz, viewerUp);
	viewerCross = glm::cross(viewerUp, viewerForward);

	float viewAngleVert = (double)mouseMovement[1] * radiansPerMousePosVert;
	viewerForward = glm::rotate(viewerForward, viewAngleVert, viewerCross);
	viewerUp = glm::cross(viewerForward, viewerCross);

	if ((bool)(inputs & CD_INPUTS::ROTATEL) != (bool)(inputs & CD_INPUTS::ROTATER)) {
		float viewAngleFront = radiansPerSecondFront * timeDif * (inputs & CD_INPUTS::ROTATEL ? 1 : -1);
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

void Cedai::printViewData() {
	CD_TRACE("forward:	{:+>9.6f} {:+>9.6f} {:+>9.6f}", viewerForward[0], viewerForward[1], viewerForward[2]);
	CD_TRACE("cross:	{:+>9.6f} {:+>9.6f} {:+>9.6f}", viewerCross[0], viewerCross[1], viewerCross[2]);
	CD_TRACE("up:		{:+>9.6f} {:+>9.6f} {:+>9.6f}", viewerUp[0], viewerUp[1], viewerUp[2]);
}

void Cedai::printFPS() {
	static int fps = 0;
	static time_point<system_clock> prevTime = system_clock::now();

	fps++;
	duration<double> elapsedTime;
	elapsedTime = system_clock::now() - prevTime;
	if (elapsedTime.count() > 1) {
		CD_TRACE("fps = {}", fps);
		fpsSum += fps;
		fpsCount++;
		prevTime = system_clock::now();
		fps = 0;
	}
}
