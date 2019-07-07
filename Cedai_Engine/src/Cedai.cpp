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

#undef main // sdl2 defines main for some reason
int main() {
	Cedai App;
	try {
		App.Run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
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
	renderer.init(screen_width, screen_height);
	CD_INFO("Renderer initialised.");
	interface.init(screen_width, screen_height);
	CD_INFO("Interface initialised.");

	pixels = new float[screen_width * screen_height * 3];
	view[0][0] = 1; view[1][1] = 1; view[2][2] = 1;
	CD_INFO("Engine initialised.");
}

void Cedai::loop() {
	bool quit = false;
	time_point<system_clock> prevTime;
	duration<double> elapsedTime;

	while (!quit) {
		prevTime = system_clock::now();

		// input handling
		interface.processEvents();
		processInputs();
		quit = (inputs & CD_INPUTS::QUIT) || (inputs & CD_INPUTS::ESC);

		// render and draw
		renderer.render(pixels, view);
		interface.draw(pixels);

		elapsedTime = system_clock::now() - prevTime;
		//CD_TRACE("draw took: {}s", elapsedTime.count());
	}
}

void Cedai::cleanUp() {
	renderer.cleanUp();
	interface.cleanUp();
	delete pixels;
}

void Cedai::processInputs() {
	// get inputs from window interface
	inputs = interface.getKeyInputs();
	long mouseMovement[2];
	interface.getMouseChange(mouseMovement[0], mouseMovement[1]);

	float viewAngleHoriz = (double)mouseMovement[0] * radiansPerMousePosHoriz;
	viewerForward = glm::rotate(viewerForward, viewAngleHoriz, viewerUp);
	viewerCross = glm::cross(viewerUp, viewerForward);

	float viewAngleVert = (double)mouseMovement[1] * radiansPerMousePosVert;
	viewerForward = glm::rotate(viewerForward, viewAngleVert, viewerCross);
	viewerUp = glm::cross(viewerForward, viewerCross);

	printViewData();
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
	//static const int width = 7;
	//static const int prec = 2;
	//std::cout << std::fixed << std::setw(width) << std::showpoint << std::setprecision(prec) << viewerForward.x << std::setw(width) << std::setprecision(prec) << viewerForward.y << std::setw(width) << std::setprecision(prec) << viewerForward.z << " :: "
	//	<< std::setw(width) << std::setprecision(prec) << viewerCross.x << std::setw(width) << std::setprecision(prec) << viewerCross.y << std::setw(width) << std::setprecision(prec) << viewerCross.z << " :: "
	//	<< std::setw(width) << std::setprecision(prec) << viewerUp.x << std::setw(width) << std::setprecision(prec) << viewerUp.y << std::setw(width) << std::setprecision(prec) << viewerUp.z << std::endl;

	CD_TRACE("forward:	{:+>9.6f} {:+>9.6f} {:+>9.6f}", viewerForward[0], viewerForward[1], viewerForward[2]);
	CD_TRACE("cross:	{:+>9.6f} {:+>9.6f} {:+>9.6f}", viewerCross[0], viewerCross[1], viewerCross[2]);
	CD_TRACE("up:		{:+>9.6f} {:+>9.6f} {:+>9.6f}", viewerUp[0], viewerUp[1], viewerUp[2]);
}