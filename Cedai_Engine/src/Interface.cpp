// using opengl code from http://wili.cc/blog/opengl-cs.html

#include "Interface.h"
#include "tools/Log.h"

#include <iostream>

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

	CD_WARN("GL renderer: {}", glGetString(GL_RENDERER));

	// set key bindings
	mapKeys();
}

void Interface::draw() {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texHandle);
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
	delete[] pixels;
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
	//glActiveTexture(GL_TEXTURE0);
	texTarget = GL_TEXTURE_2D;
	glBindTexture(texTarget, texHandle);
	//glTexParameteri(texTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glTexParameteri(texTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	pixels = new uint8_t[screen_width * screen_height * 4];
	for (int x = 0; x < screen_width; x++) {
		for (int y = 0; y < screen_height; y++) {
			pixels[x * 4 + y * 4 * 640]		= 255 * x / screen_width;
			pixels[x * 4 + y * 4 * 640 + 1] = 255 * y / screen_height;
			pixels[x * 4 + y * 4 * 640 + 2] = 255;
			pixels[x * 4 + y * 4 * 640 + 3] = 0;
		}
	}
	glTexImage2D(texTarget, 0, GL_RGBA8, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	// TODO: is this needed?
	//glBindImageTexture(0, texHandle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
	checkErrors("texture gen");
}

void Interface::createRenderProgram() {
	renderHandle = glCreateProgram();
	GLuint vp = glCreateShader(GL_VERTEX_SHADER);
	GLuint fp = glCreateShader(GL_FRAGMENT_SHADER);

	const char* vpSrc[] = {
		"#version 430\n",
		"in vec2 pos;\
		 out vec2 texCoord;\
		 void main() {\
			 texCoord = pos*0.5f + 0.5f;\
			 gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);\
		 }"
	};

	const char* fpSrc[] = {
		"#version 430\n",
		"uniform usampler2D srcTex;\
		 in vec2 texCoord;\
		 out uvec4 color;\
		 void main() {\
			 color = texture(srcTex, texCoord);\
		 }"
	};

	glShaderSource(vp, 2, vpSrc, NULL);
	glShaderSource(fp, 2, fpSrc, NULL);

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
		throw std::runtime_error("opengl setup error");
}

/*
notes:
opencl with opengl: https://software.intel.com/en-us/articles/opencl-and-opengl-interoperability-tutorial

which core profile loader? https://www.reddit.com/r/opengl/comments/3m28x1/glew_vs_glad/
gl3w setup: https://stackoverflow.com/questions/22436937/opengl-with-gl3w
gl3w reddit: https://www.reddit.com/r/opengl/comments/3m28x1/glew_vs_glad/
*/