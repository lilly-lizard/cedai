#pragma once

/*
Engine notes...
Coordinate system: x - forwards, y - right, z - up
*/

#include "Interface.hpp"
#include "Renderer.hpp"
#include "PrimitiveProcessor.hpp"
#include "tools/Sphere.hpp"
#include "tools/Polygon.hpp"

#include <glm/glm.hpp>
#include <CL/cl.h>
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
	PrimitiveProcessor vertexProcessor;

	std::vector<cd::Sphere> spheres;
	std::vector<cd::Sphere> lights;
	std::vector<glm::vec4> vertices;
	std::vector<glm::mat4> bones;

	// TODO remove these
	std::vector<cl_float3> cl_vertices;
	std::vector<cl_uchar4> cl_polygonColors;

	float view[4][4] = { 0 };
	uint32_t inputs;

	float strafeSpeed  = 4;
	float forwardSpeed = 4;
	float backSpeed	   = 4;
	float radiansPerMousePosYaw	  = 0.002f;
	float radiansPerMousePosPitch =	0.002f;
	float radiansPerSecondRoll = CD_PI / 4;

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