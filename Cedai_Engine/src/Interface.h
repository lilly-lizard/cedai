#pragma once

#include "tools/Inputs.h"

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include <map>
#include <string>

class Interface {
public:
	void init(int screen_width, int screen_height);

	inline GLuint getTexHandle() { return texHandle; }
	inline GLenum getTexTarget() { return texTarget; }
	inline GLFWwindow* getWindow() { return window; }

	void draw();

	void MinimizeCheck();
	inline void PollEvents() { glfwPollEvents(); };
	inline int WindowCloseCheck() { return glfwWindowShouldClose(window); };

	unsigned int GetKeyInputs();
	void GetMouseChange(double& mouseX, double& mouseY);

	void cleanUp();

private:

	GLFWwindow* window;
	int screen_width;
	int screen_height;

	GLuint texHandle;
	GLenum texTarget;
	GLuint renderHandle;

	std::map<int, CD_INPUTS> keyBindings;
	double mousePosPrev[2];

	void mapKeys();

	void createTexture();
	void createRenderProgram();

	void checkErrors(std::string desc);
	std::string readFile(const std::string& filename);
};