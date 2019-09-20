#include "Interface.hpp"

#include "Cedai.hpp"
#include "tools/Log.hpp"

#include <iostream>
#include <fstream>

#define VERT_PATH "shaders/draw.vert"
#define FRAG_PATH "shaders/draw.frag"

// PUBLIC FUNCTIONS

void Interface::init(Cedai *application, int window_width, int window_height) {
	CD_INFO("Initialising interface...");
	windowWidth = window_width;
	windowHeight = window_height;

	// initalizes glfw
	glfwInit();

	// create window and gl context
#ifdef RESIZABLE
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
#else
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
#endif
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwSwapInterval(0);
	window = glfwCreateWindow(windowWidth, windowHeight, WINDOW_TITLE, nullptr, nullptr);
	if (!window) {
		const char *message;
		int error = glfwGetError(&message);
		CD_ERROR("glfw window creation failed! error: {} {}", error, message);
		throw std::runtime_error("glfw window creation failed");
	}

	// init opengl
	glfwMakeContextCurrent(window);
	if (gl3wInit()) {
		CD_ERROR("failed to initialize OpenGL");
		throw std::runtime_error("opengl init failed");
	}

	// gl version check
	CD_INFO("OpenGL version: {}", glGetString(GL_VERSION));
	CD_INFO("OpenGL rendering device: {}", glGetString(GL_RENDERER));

	glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
	glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
	if (majorVersion < 4 && minorVersion < 3) {
		CD_ERROR("openGL version number less than 4.3");
		CD_ERROR("4.3 functionality is required for this program to run"); // i.e. shader storage buffer object (used for ogl <--> ocl data transfer)
		throw std::runtime_error("openGL version");
	}

	// TODO print relevent system parameters
	//GLint value;
	//glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS , &value);
	//glGetIntegerv(GL_UNIFORM_BUFFER_SIZE, &value);

	// resize callback
	glfwSetWindowUserPointer(window, application);
	glfwSetFramebufferSizeCallback(window, application->windowResizeCallback);

	// window init
	glfwSetWindowPos(window, 300, 100);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // disable mouse cursor
	glfwGetCursorPos(window, &mousePosPrev[0], &mousePosPrev[1]); // get mouse position

	// set up opengl draw program
	cd::createProgramGL(drawPipeline.programHandle, VERT_PATH, FRAG_PATH, false);
	createDrawTexture();
	setProgramIO();
	cd::checkErrorsGL("draw program create");

	// set key bindings
	mapKeys();
}

GLuint Interface::getTexHandle() { return drawPipeline.texHandle; }
GLenum Interface::getTexTarget() { return drawPipeline.texTarget; }
GLFWwindow* Interface::getWindow() { return window; }

void Interface::drawRun() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glVertexAttribPointer(vertexLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(vertexLocation);

	glUseProgram(drawPipeline.programHandle);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);

	cd::checkErrorsGL("GL draw");
}

void Interface::drawBarrier() {
	glFinish();
	glfwSwapBuffers(window);
}

void Interface::MinimizeCheck() {
	int width = 0, height = 0;
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}
}

void Interface::showFPS(int fps) {
	std::string title = WINDOW_TITLE;
	title += " fps: " + std::to_string(fps);
	glfwSetWindowTitle(window, title.c_str());
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

void Interface::resize(int &window_width, int &window_height) {
	glfwGetWindowSize(window, &windowWidth, &windowHeight);

#	ifdef HALF_RESOLUTION
	windowWidth = windowWidth - windowWidth % 32;
	windowHeight = windowHeight - windowHeight % 32;
#	else
	windowWidth = windowWidth - windowWidth % 16;
	windowHeight = windowHeight - windowHeight % 16;
#	endif
	glfwSetWindowSize(window, windowWidth, windowHeight);

	window_width = windowWidth;
	window_height = windowHeight;

	//glBindTexture(drawPipeline.texTarget, drawPipeline.texHandle);
	glDeleteTextures(1, &drawPipeline.texHandle);
	drawPipeline.texHandle = -1;
	createDrawTexture();
}

void Interface::cleanUp() {
	glDeleteTextures(1, &drawPipeline.texHandle);
	glfwDestroyWindow(window);
	glfwTerminate();
}

// OPENGL HELPER FUNCTIONS

void cd::createProgramGL(GLuint &program, std::string vertPath, std::string fragPath, bool defineBones) {
	program = glCreateProgram();

	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

	std::string vertSrc = cd::readFile(vertPath.c_str());
	std::string fragSrc = cd::readFile(fragPath.c_str());

	// define bone array size
	if (0) {
		// find the #version line
		char c = '\0';
		while (c != '\0') {

		}
	}

	GLchar const* vertString[] = { vertSrc.c_str() };
	GLchar const* fragString[] = { fragSrc.c_str() };

	glShaderSource(vert, 1, vertString, NULL);
	glShaderSource(frag, 1, fragString, NULL);

	glCompileShader(vert);
	int rvalue;
	glGetShaderiv(vert, GL_COMPILE_STATUS, &rvalue);
	if (!rvalue) {
		CD_ERROR("Error in compiling vert shader: {}", vertPath);
		int message_length = 0;
		char message[1024];
		glGetShaderInfoLog(vert, 1024, &message_length, message);
		std::string str_message(message, message_length);
		CD_WARN("compile message: {}", str_message);
		throw std::runtime_error("opengl setup error");
	}
	glAttachShader(program, vert);

	glCompileShader(frag);
	glGetShaderiv(frag, GL_COMPILE_STATUS, &rvalue);
	if (!rvalue) {
		CD_ERROR("Error in compiling frag shader: {}", fragPath);
		int message_length = 0;
		char message[128];
		glGetShaderInfoLog(vert, 128, &message_length, message);
		std::string str_message(message, message_length);
		CD_WARN("compile message: {}", str_message);
		throw std::runtime_error("opengl setup error");
	}
	glAttachShader(program, frag);

	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &rvalue);
	if (!rvalue) {
		CD_ERROR("Error in linking shader program error: {}", rvalue);
		throw std::runtime_error("opengl setup error");
	}
	glUseProgram(program);

	glDeleteShader(vert);
	glDeleteShader(frag);
}

void cd::checkErrorsGL(std::string desc) {
	bool error_found = false;
	int e = glGetError();
	while (e != GL_NO_ERROR) {
		error_found = true;
		CD_ERROR("OpenGL error with '{}': {:#x}", desc, e);
		//e = GL_NO_ERROR;
		e = glGetError();
	}
	if (error_found)
		throw std::runtime_error("opengl error");
}

std::string cd::readFile(const std::string& filename) {
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

// PRIVATE FUNCTIONS

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

void Interface::createDrawTexture() {
	// create output texture for opencl to write to
	glGenTextures(1, &drawPipeline.texHandle);
	drawPipeline.texTarget = GL_TEXTURE_2D;
	glBindTexture(drawPipeline.texTarget, drawPipeline.texHandle);

	glTexParameteri(drawPipeline.texTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(drawPipeline.texTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(drawPipeline.texTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(drawPipeline.texTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(drawPipeline.texTarget, 0, GL_RGBA8UI, windowWidth, windowHeight, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL);
	glUniform1i(glGetUniformLocation(drawPipeline.programHandle, "srcTex"), 0);
	cd::checkErrorsGL("texture gen");
}

void Interface::setProgramIO() {

	GLuint vertexArray;
	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);

	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

	float data[] = {
		0.0f, 0.0f,
		2.0f, 0.0f,
		0.0f, 2.0f,
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6, data, GL_STREAM_DRAW);

	vertexLocation = glGetAttribLocation(drawPipeline.programHandle, "pos");
	glVertexAttribPointer(vertexLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(vertexLocation);

	glBindFragDataLocation(drawPipeline.programHandle, 0, "color");
}

/*
notes:
opencl with opengl: https://software.intel.com/en-us/articles/opencl-and-opengl-interoperability-tutorial

which core profile loader? https://www.reddit.com/r/opengl/comments/3m28x1/glew_vs_glad/
gl3w setup: https://stackoverflow.com/questions/22436937/opengl-with-gl3w
gl3w reddit: https://www.reddit.com/r/opengl/comments/3m28x1/glew_vs_glad/

glTexImage2D integer format: https://stackoverflow.com/questions/11278928/opengl-2-texture-internal-formats-gl-rgb8i-gl-rgb32ui-etc
*/
