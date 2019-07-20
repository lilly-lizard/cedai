#include "Cedai.hpp"
#include "Interface.hpp"
#include "tools/Model_Loader.hpp"
#include "tools/Inputs.hpp"
#include "tools/Log.hpp"

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <iomanip>

#define MAIZE_FILE "../assets/maize.bin"

using namespace std::chrono;

const int screen_width = 960;
const int screen_height = 640;
// 640 x 480
// 960 x 640

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
	
	createPrimitives();
	vertexProcessor.init(&interface, vertices);
	CD_INFO("Pimitive processing program initialised.");

	renderer.init(screen_width, screen_height, &interface, &vertexProcessor,
		spheres, lights, cl_vertices, cl_polygonColors);
	CD_INFO("Renderer initialised.");

	view[0][0] = 1; view[1][1] = 1; view[2][2] = 1;
	CD_INFO("Engine initialised.");
}

void Cedai::loop() {
	bool quit = false;
	time_point<high_resolution_clock> timeStart = high_resolution_clock::now();

	while (!quit && !interface.WindowCloseCheck()) {
		// queue a render operation
		float time = duration<double, seconds::period>(high_resolution_clock::now() - timeStart).count();
		renderer.renderQueue(view, time);

		// input handling
		interface.PollEvents();
		inputs = interface.GetKeyInputs();
		quit = inputs & CD_INPUTS::ESC;
		processInputs();
		printFPS();

		// draw to the window
		renderer.renderBarrier();
		vertexProcessor.vertexProcess(time);
		interface.drawRun();
		interface.drawBarrier();
		vertexProcessor.vertexBarrier();
	}
}

void Cedai::cleanUp() {
	CD_INFO("Cleaning up...");
	CD_INFO("Average fps = {}", fpsSum / fpsCount);

	renderer.cleanUp();
	vertexProcessor.cleanUp();
	interface.cleanUp();

	CD_INFO("Finished cleaning.");
}

void Cedai::createPrimitives() {

	spheres.push_back(cd::Sphere{ 1.0, 0, 0, 0,
		cl_float3{{ 10, -3, 0 }},
		cl_uchar4{{ 200, 128, 254, 0 }} });

	spheres.push_back(cd::Sphere{ 0.5, 0, 0, 0,
		cl_float3{{ 4, 2, 2.5 }},
		cl_uchar4{{ 128, 255, 180, 0 }} });

	spheres.push_back(cd::Sphere{ 0.2, 0, 0, 0,
		cl_float3{{ 6, 0, 3 }},
		cl_uchar4{{ 255, 200, 128, 0 }} });

	lights.push_back( cd::Sphere{ 0.1, 0, 0, 0,
		cl_float3{{ 2, 3, 4 }},
		cl_uchar4{{ 255, 255, 205, 0 }} });

	lights.push_back(cd::Sphere{ 0.1, 0, 0, 0,
		cl_float3{{  -1, -6, -4 }},
		cl_uchar4{{ 255, 255, 205, 0 }} });

	std::vector<glm::vec4> verticesLoad;
	std::vector<glm::uvec4> polygonsLoad;
	
	CD_INFO("loading model(s)...");
	cd::LoadModel(MAIZE_FILE, verticesLoad, polygonsLoad);

	for (int p = 0; p < polygonsLoad.size(); p++) {
		glm::vec4 vert = verticesLoad[polygonsLoad[p].x];
		vertices.push_back(glm::vec4( vert.x, vert.y, vert.z, 0));
		cl_vertices.push_back(cl_float3{{ vert.x, vert.y, vert.z }});

		vert = verticesLoad[polygonsLoad[p].y];
		vertices.push_back(glm::vec4(vert.x, vert.y, vert.z, 0));
		cl_vertices.push_back(cl_float3{{ vert.x, vert.y, vert.z }});

		vert = verticesLoad[polygonsLoad[p].z];
		vertices.push_back(glm::vec4(vert.x, vert.y, vert.z, 0));
		cl_vertices.push_back(cl_float3{{ vert.x, vert.y, vert.z }});

		cl_polygonColors.push_back( cl_uchar4{{ 200, 200, 200, 0 }} );
	}

	CD_INFO("model(s) loaded.");
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
#ifdef DEBUG
		interface.showFPS(fps);
#endif
		fpsSum += fps;
		fpsCount++;
		prevTime = system_clock::now();
		fps = 0;
	}
}
