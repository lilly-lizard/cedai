#include "PrimitivePipeline.hpp"
#include "Interface.hpp"
#include "tools/Log.hpp"

#define VERT_PATH "src/shaders/primitive.vert"
#define FRAG_PATH "src/shaders/primitive.frag"

// PUBLIC FUNCTIONS

void PrimitivePipeline::init(Interface* interface, std::vector<glm::vec4>& vertices) {
	CD_INFO("Initialising primitive processing program...");

	// create program
	cd::createProgramGL(program, VERT_PATH, FRAG_PATH);
	setProgramIO(vertices);

	// make dummy render target
	createRasteriseTarget();

	// init vertex buffer data
	vertexProcess(0);
	vertexBarrier();
}

void PrimitivePipeline::vertexProcess(float time) {

	// write to the uniform buffer object

	ubo_time = time;
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	GLvoid* p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
	memcpy(p, &ubo_time, sizeof(ubo_time));
	glUnmapBuffer(GL_UNIFORM_BUFFER);

	// setup and run the program

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	
	glBindBuffer(GL_ARRAY_BUFFER, vertexBufferIn);
	glVertexAttribPointer(vertexLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(vertexLocation);

	glUseProgram(program);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, vertexCount);

	cd::checkErrorsGL("GL process primitives");
}

void PrimitivePipeline::vertexBarrier() {
	glFinish();
}

void PrimitivePipeline::cleanUp() {
	glDeleteRenderbuffers(1, &renderbuffer);
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteBuffers(1, &vertexBufferOut);
}

// PRIVATE FUNCTIONS

void PrimitivePipeline::createRasteriseTarget() {
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glGenRenderbuffers(1, &renderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, 1, 1);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
	
	GLenum drawBuffers[] = { GL_NONE };
	glDrawBuffers(1, drawBuffers);
	
	GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (result != GL_FRAMEBUFFER_COMPLETE) {
		CD_ERROR("primitive pipeline framebuffer create error: {}", result);
		throw std::runtime_error("pimitive pipeline init");
	}
	cd::checkErrorsGL("rasterize target create");

	// switch back to window frambuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PrimitivePipeline::setProgramIO(std::vector<glm::vec4>& vertices) {
	vertexCount = vertices.size();

	// vertex input

	GLuint vertArray;
	glGenVertexArrays(1, &vertArray);
	glBindVertexArray(vertArray);
	
	glGenBuffers(1, &vertexBufferIn);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBufferIn);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * vertexCount, vertices.data(), GL_STATIC_DRAW);

	vertexLocation = glGetAttribLocation(program, "vertex");
	glVertexAttribPointer(vertexLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(vertexLocation);

	// vertext output

	glGenBuffers(1, &vertexBufferOut);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBufferOut);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec4) * vertexCount, NULL, GL_DYNAMIC_COPY);
	GLuint vertexOutIndex = 0;
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vertexOutIndex, vertexBufferOut);

	// time ubo

	// todo generate buffers togehter?
	glGenBuffers(1, &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(ubo_time), &ubo_time, GL_DYNAMIC_DRAW);
	GLuint uboIndex = glGetUniformBlockIndex(program, "UBO");
	glBindBufferBase(GL_UNIFORM_BUFFER, uboIndex, ubo);

	cd::checkErrorsGL("primitive pipeline set io");
}

/*
render texture target: https://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/  https://github.com/opengl-tutorials/ogl/blob/master/tutorial14_render_to_texture/tutorial14.cpp
opengl context: https://www.khronos.org/opengl/wiki/Creating_an_OpenGL_Context_(WGL)
wgl context: https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglcreatecontext
*/