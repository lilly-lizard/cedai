#pragma once

/*
Engine notes...
Coordinate system: x - forwards, y - right, z - up
*/

#include "Interface.hpp"
#include "Renderer.hpp"
#include "tools/Sphere.hpp"
#include "tools/Polygon.hpp"

#include <glm/glm.hpp> // vector/matrix linear algebra
#include <chrono>
#include <vector>
#define CD_PI 3.14159

class Cedai {
public:
	void Run();

private:
	void init();
	void loop();
	void cleanUp();

	Interface interface;
	Renderer renderer;

	std::vector<cd::Sphere> spheres;
	std::vector<cd::Sphere> lights;
	std::vector<cl_uchar4> polygon_colors;
	std::vector<cl_float3> vertices;

	float view[4][4] = { 0 };
	uint32_t inputs;

	float strafeSpeed  = 4;
	float forwardSpeed = 4;
	float backSpeed	   = 4;
	float radiansPerMousePosHoriz = 0.002f;
	float radiansPerMousePosVert =	0.002f;
	float radiansPerSecondFront = CD_PI / 4;

	glm::vec3 viewerPosition =	glm::vec3(0, 0, 0);				 // your position in the world
	glm::vec3 viewerForward =	glm::vec3(1, 0, 0);				 // direction you are facing
	glm::vec3 viewerUp =		glm::vec3(0, 0, 1);				 // viewer up direction
	glm::vec3 viewerCross = glm::cross(viewerUp, viewerForward); // cross product up x forward

	double fpsSum = 0;
	int fpsCount = 0;

	void createPrimitives();

	void processInputs();
	void updateView();
	void printViewData();
	void printFPS();
};