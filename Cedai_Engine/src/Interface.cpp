// using opengl code from http://wili.cc/blog/opengl-cs.html

#include "Interface.h"
#include "tools/Log.h"

#include <iostream>
#include <fstream>

#define FRAG_PATH "src/shaders/shader.frag"
#define VERT_PATH "src/shaders/shader.vert"

// PUBLIC FUNCTIONS

void Interface::init(int screen_width, int screen_height) {
	CD_INFO("Initialising interface...");
	this->screen_width = screen_width;
	this->screen_height = screen_height;

	glfwInit(); // initalizes the glfw library

	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	window = glfwCreateWindow(screen_width, screen_height, "Cedai", nullptr, nullptr); // make a window
	if (!window) {
		CD_ERROR("glfw window creation failed!");
		throw std::runtime_error("glfw window creation failed");
	}
	glfwSetWindowPos(window, 300, 100);

	// init opengl
	glfwMakeContextCurrent(window);
	if (gl3wInit()) {
		CD_ERROR("failed to initialize OpenGL");
		throw std::runtime_error("opengl init failed");
	}

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // disable mouse cursor
	glfwGetCursorPos(window, &mousePosPrev[0], &mousePosPrev[1]); // get mouse position

	// set up opengl program
	createTexture();
	createRenderProgram();

	CD_INFO("OpenGL version: {}", glGetString(GL_RENDERER));

	// set key bindings
	mapKeys();
}

void Interface::draw() {
	glUseProgram(renderHandle);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glfwSwapBuffers(window);
	checkErrors("GL draw");
	glFinish();
}

void Interface::MinimizeCheck() {
	int width = 0, height = 0;
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}
}

uint32_t Interface::GetKeyInputs() {
	uint32_t inputs = 0;

	for (auto&[key, input] : keyBindings) {
		int state = glfwGetKey(window, key);
		if (state == GLFW_PRESS)
			inputs |= input;
	}

	return inputs;
}

void Interface::GetMouseChange(double& mouseX, double& mouseY) {
	double mousePosCurrent[2];
	glfwGetCursorPos(window, &mousePosCurrent[0], &mousePosCurrent[1]); // get current mouse position
	mouseX = mousePosCurrent[0] - mousePosPrev[0];
	mouseY = mousePosCurrent[1] - mousePosPrev[1];
	glfwGetCursorPos(window, &mousePosPrev[0], &mousePosPrev[1]); // set previous position
}

void Interface::cleanUp() {
	glDeleteTextures(1, &texHandle);
	glfwDestroyWindow(window);
	glfwTerminate();
}

// HELPER FUNCTIONS

void Interface::mapKeys() {
	keyBindings[GLFW_KEY_ESCAPE] = CD_INPUTS::ESC;

	keyBindings[GLFW_KEY_W] = CD_INPUTS::UP;
	keyBindings[GLFW_KEY_A] = CD_INPUTS::LEFT;
	keyBindings[GLFW_KEY_S] = CD_INPUTS::DOWN;
	keyBindings[GLFW_KEY_D] = CD_INPUTS::RIGHT;
	keyBindings[GLFW_KEY_Q] = CD_INPUTS::ROTATEL;
	keyBindings[GLFW_KEY_E] = CD_INPUTS::ROTATER;

	//keyBindings[GLFW_KEY_UP] = CD_INPUTS::UP;
	//keyBindings[GLFW_KEY_DOWN] = CD_INPUTS::DOWN;
	//keyBindings[GLFW_KEY_LEFT] = CD_INPUTS::LEFT;
	//keyBindings[GLFW_KEY_RIGHT] = CD_INPUTS::RIGHT;
	//keyBindings[GLFW_KEY_PAGE_UP] = CD_INPUTS::ROTATEL;
	//keyBindings[GLFW_KEY_PAGE_DOWN] = CD_INPUTS::ROTATER;

	keyBindings[GLFW_KEY_SPACE] = CD_INPUTS::FORWARD;
	keyBindings[GLFW_KEY_LEFT_SHIFT] = CD_INPUTS::BACKWARD;

	keyBindings[GLFW_KEY_Z] = CD_INPUTS::INTERACTL;
	keyBindings[GLFW_KEY_X] = CD_INPUTS::INTERACTR;
}

void Interface::createTexture() {
	// create output texture for opencl to write to
	glGenTextures(1, &texHandle);
	texTarget = GL_TEXTURE_2D;
	glBindTexture(texTarget, texHandle);

	glTexParameteri(texTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(texTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	
	glTexImage2D(texTarget, 0, GL_RGBA8UI, screen_width, screen_height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL);
	checkErrors("texture gen");
}

void Interface::createRenderProgram() {
	renderHandle = glCreateProgram();
	GLuint vp = glCreateShader(GL_VERTEX_SHADER);
	GLuint fp = glCreateShader(GL_FRAGMENT_SHADER);

	std::string vpSrc = readFile(VERT_PATH);
	std::string fpSrc = readFile(FRAG_PATH);

	GLchar const* vpString[] = { vpSrc.c_str() };
	GLchar const* fpString[] = { fpSrc.c_str() };

	glShaderSource(vp, 1, vpString, NULL);
	glShaderSource(fp, 1, fpString, NULL);

	glCompileShader(vp);
	int rvalue;
	glGetShaderiv(vp, GL_COMPILE_STATUS, &rvalue);
	if (!rvalue) {
		CD_ERROR("Error in compiling vp");
		throw std::runtime_error("opengl setup error");
	}
	glAttachShader(renderHandle, vp);

	glCompileShader(fp);
	glGetShaderiv(fp, GL_COMPILE_STATUS, &rvalue);
	if (!rvalue) {
		CD_ERROR("Error in compiling fp");
		throw std::runtime_error("opengl setup error");
	}
	glAttachShader(renderHandle, fp);

	glBindFragDataLocation(renderHandle, 0, "color");
	glLinkProgram(renderHandle);

	glGetProgramiv(renderHandle, GL_LINK_STATUS, &rvalue);
	if (!rvalue) {
		CD_ERROR("Error in linking sp");
		throw std::runtime_error("opengl setup error");
	}

	glUseProgram(renderHandle);
	glUniform1i(glGetUniformLocation(renderHandle, "srcTex"), 0);

	GLuint vertArray;
	glGenVertexArrays(1, &vertArray);
	glBindVertexArray(vertArray);

	GLuint posBuf;
	glGenBuffers(1, &posBuf);
	glBindBuffer(GL_ARRAY_BUFFER, posBuf);
	float data[] = {
		-1.0f, -1.0f,
		-1.0f,  1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, data, GL_STREAM_DRAW);
	GLint posPtr = glGetAttribLocation(renderHandle, "pos");
	glVertexAttribPointer(posPtr, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(posPtr);

	checkErrors("Render shaders");
}

void Interface::checkErrors(std::string desc) {
	bool error_found = false;
	GLenum e = glGetError();
	while (e != GL_NO_ERROR) {
		error_found = true;
		CD_ERROR("OpenGL error in {}: {}", desc, e);
		e = glGetError();
	}
	if (error_found)
		throw std::runtime_error("opengl error");
}

std::string Interface::readFile(const std::string& filename) {
	// open file
	std::ifstream file(filename, std::ios::ate | std::ios::binary); // ate: read from the end of the file; binary: file type
	if (!file.is_open()) {
		CD_ERROR("Unable to open file: " + filename);
		throw std::runtime_error("~ failed to open file!");
	}

	// allocate a buffer
	size_t fileSize = (size_t)file.tellg(); // read position = file size
	std::string buffer(fileSize, ' ');

	// go back to beginning and read all bytes at once
	file.seekg(0);
	file.read(buffer.data(), fileSize);

	// close and return bytes
	file.close();
	return buffer;
}

/*
notes:
opencl with opengl: https://software.intel.com/en-us/articles/opencl-and-opengl-interoperability-tutorial

which core profile loader? https://www.reddit.com/r/opengl/comments/3m28x1/glew_vs_glad/
gl3w setup: https://stackoverflow.com/questions/22436937/opengl-with-gl3w
gl3w reddit: https://www.reddit.com/r/opengl/comments/3m28x1/glew_vs_glad/

glTexImage2D integer format: https://stackoverflow.com/questions/11278928/opengl-2-texture-internal-formats-gl-rgb8i-gl-rgb32ui-etc
*/