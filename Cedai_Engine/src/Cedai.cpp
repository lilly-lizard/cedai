#include "Cedai.h"
#include "Interface.h"
#include "tools/Model_Loader.h"
#include "tools/Inputs.h"
#include "tools/Log.h"

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <iomanip>

#define MAIZE_FILE "../assets/maize.bin"

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
	
	CD_WARN("p size = {}", sizeof(cd::Polygon));
	createEntities();
	renderer.init(screen_width, screen_height, &interface, spheres, lights, vertices, polygons);
	CD_INFO("Renderer initialised.");

	view[0][0] = 1; view[1][1] = 1; view[2][2] = 1;
	timePrev = high_resolution_clock::now();
	CD_INFO("Engine initialised.");
}

void Cedai::loop() {
	bool quit = false;

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

	spheres.resize(3);

	spheres[0].radius = 1.0;
	spheres[0].position = { { 10, -3, 0 } };
	spheres[0].color = { { 230, 128, 128 } };

	spheres[1].radius = 0.5;
	spheres[1].position = { { 4, 1, 1 } };
	spheres[1].color = { { 255, 255, 128 } };

	spheres[2].radius = 0.2;
	spheres[2].position = { { 5, 2, -1 } };
	spheres[2].color = { { 128, 128, 230 } };

	lights.resize(2);

	lights[0].radius = 0.1;
	lights[0].position = { { 5, 1, 2 } };
	lights[0].color = { { 255, 255, 205 } };

	lights[1].radius = 0.1;
	lights[1].position = { { 4, -2, -2 } };
	lights[1].color = { { 255, 255, 205 } };

	std::vector<glm::vec4> verticesLoad;
	std::vector<glm::uvec4> polygonsLoad;
	
	CD_INFO("loading models...");
	cd::LoadModel(MAIZE_FILE, verticesLoad, polygonsLoad);
	
	vertices.clear();
	for (int v = 0; v < verticesLoad.size(); v++)
		vertices.push_back(cl_float3{{ verticesLoad[v].x, verticesLoad[v].y, verticesLoad[v].z }});
	
	polygons.clear();
	for (int p = 0; p < polygonsLoad.size(); p++)
		polygons.push_back(cd::Polygon{
			cl_uint3{{ polygonsLoad[p].x, polygonsLoad[p].y, polygonsLoad[p].z }},
			cl_uchar3{{ 128, 128, 128 }}
			});
	CD_INFO("models loaded.");
	
	uint32_t n = vertices.size();
	vertices.push_back(cl_float3{ { 5, 0, 2 } });
	vertices.push_back(cl_float3{ { 5.7, 0.1, 3 } });
	vertices.push_back(cl_float3{ { 6.1, 0.6, 2.05 } });
	vertices.push_back(cl_float3{ { 6.2, -0.5, 2 } });
	vertices.push_back(cl_float3{ { 6.2, -0.5, 2 } });

	polygons.push_back(cd::Polygon{
		cl_uint3{{ 0 + n, 1 + n, 2 + n }},
		cl_uchar3{{ 128, 255, 180 }}
		});
	polygons.push_back(cd::Polygon{
		cl_uint3{{ 0 + n, 2 + n, 3 + n }},
		cl_uchar3{{ 180, 128, 255 }}
		});
	polygons.push_back(cd::Polygon{
		cl_uint3{{ 0 + n, 3 + n, 1 + n }},
		cl_uchar3{{ 255, 180, 128 }}
		});
	polygons.push_back(cd::Polygon{
		cl_uint3{{ 1 + n, 3 + n, 2 + n }},
		cl_uchar3{{ 180, 255, 128 }}
		});

	//vertices.resize(4);
	//
	//vertices[0] = { { 5, 0, 0 } };
	//vertices[1] = { { 5.7, 0.1, 1 } };
	//vertices[2] = { { 6.1, 0.6, 0.05 } };
	//vertices[3] = { { 6.2, -0.5, 0 } };
	//
	//polygons.resize(4);
	//
	//polygons[0].indices = { { 0, 1, 2 } };
	//polygons[1].indices = { { 0, 2, 3 } };
	//polygons[2].indices = { { 0, 3, 1 } };
	//polygons[3].indices = { { 1, 3, 2 } };
	//
	//polygons[0].color = { { 128, 255, 180 } };
	//polygons[1].color = { { 180, 128, 255 } };
	//polygons[2].color = { { 255, 180, 128 } };
	//polygons[3].color = { { 180, 255, 128 } };

	CD_WARN("num vertices = {}", vertices.size());
	CD_WARN("num polygons = {}", polygons.size());
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
