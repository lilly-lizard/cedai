#pragma once

/*
Engine notes...
Coordinate system: x - forwards, y - right, z - up
*/

#include "Interface.h"
#include "Renderer.h"

#include <glm/glm.hpp> // vector/matrix linear algebra
#include <chrono>

class Cedai {
public:
	void Run();

private:
	void init();
	void loop();
	void cleanUp();

	void processInputs();
	void updateView();
	void printViewData();

	Interface interface;
	Renderer renderer;

	float *pixels;
	float view[4][4] = { 0 };
	uint32_t inputs;

	float strafeSpeed = 1.5;
	float forwardSpeed = 2;
	float backSpeed = 1;
	float radiansPerMousePosHoriz = 0.01f;
	float radiansPerMousePosVert =	0.01f;
	float radiansPerSecondFront = (float)M_PI / 4;

	glm::vec3 viewerPosition =	glm::vec3(0, 0, 0);				 // your position in the world
	glm::vec3 viewerForward =	glm::vec3(1, 0, 0);				 // direction you are facing
	glm::vec3 viewerUp =		glm::vec3(0, 0, 1);				 // viewer up direction
	glm::vec3 viewerCross = glm::cross(viewerUp, viewerForward); // cross product up x forward
	std::chrono::time_point<std::chrono::high_resolution_clock> timePrev; // time value from the previous render
};