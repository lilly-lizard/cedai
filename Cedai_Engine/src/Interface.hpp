#pragma once

#include "tools/Config.hpp"
#include "tools/Inputs.hpp"

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <map>
#include <string>

 // opengl functions

class Cedai;

namespace cd {
	void createProgramGL(GLuint &program, std::string vertPath, std::string fragPath);
	void checkErrorsGL(std::string desc);
	std::string readFile(const std::string& filename);
}

class Interface {
public:
	void init(Cedai *application, int window_width, int window_height);

	GLuint getTexHandle();
	GLenum getTexTarget();
	GLFWwindow* getWindow();

	void drawRun();
	void drawBarrier();

	void MinimizeCheck();
	inline void PollEvents() { glfwPollEvents(); };
	inline int WindowCloseCheck() { return glfwWindowShouldClose(window); };
	void showFPS(int fps);

	unsigned int GetKeyInputs();
	void GetMouseChange(double& mouseX, double& mouseY);

	void resize(int &window_width, int &window_height);

	void cleanUp();

private:

	GLFWwindow* window;
	GLint majorVersion = 0, minorVersion = 0;
	int windowWidth = 0, windowHeight = 0;

	struct drawPipeline {
		GLuint texHandle;
		GLenum texTarget;
		GLuint programHandle;
	} drawPipeline;

	GLuint vertexBuffer;
	GLint vertexLocation;

	std::map<int, CD_INPUTS> keyBindings;
	double mousePosPrev[2];

	void mapKeys();

	void createDrawTexture();
	void setProgramIO();
};