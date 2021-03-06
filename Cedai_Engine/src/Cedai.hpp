#pragma once

/*
Engine notes...
Coordinate system: x - forwards, y - right, z - up
the number of polygons rendered depends on the number of polygon colors passed to the renderer
*/

#include "Interface.hpp"
#include "tools/Config.hpp"

#include "Renderer.hpp"
#include "PrimitiveProcessor.hpp"
#include "model/AnimatedModel.hpp"
#include "model/Sphere.hpp"

#include <glm/glm.hpp>
#include <CL/cl.h>
#include <chrono>
#include <vector>

class Cedai {
public:
	~Cedai();
	void Run();

	static void windowResizeCallback(GLFWwindow *window, int width, int height);

private:
	void init();
	void loop();
	void cleanUp();

	Interface interface;
	Renderer renderer;
	PrimitiveProcessor vertexProcessor;

	bool quit = false;
	bool windowResized = false;
	int windowWidth = 0, windowHeight = 0;

	uint32_t inputs;
	float view[4][4] = { 0 };

	AnimatedModel maize;

	std::vector<cd::Sphere> spheres;
	std::vector<cd::Sphere> lights;
	std::vector<cl_uchar4> cl_polygonColors;

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

	void resizeCheck();
	void processInputs();
	void updateAnimation(double time);

	void updateView();
	void printViewData();
	void fpsHandle();
};