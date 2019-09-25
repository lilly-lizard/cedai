#include "PrimitiveProcessor.hpp"
#include "Interface.hpp"
#include "tools/Log.hpp"

#define VERT_PATH "shaders/primitive.vert"
#define FRAG_PATH "shaders/primitive.frag"

// PUBLIC FUNCTIONS

void PrimitiveProcessor::init(Interface *interface, std::vector<cd::Vertex> &vertices, std::array<glm::mat4, MAX_BONES> bones) {
	CD_INFO("Initialising primitive processing program...");
	vertexCount = vertices.size();

	// create program
	cd::createProgramGL(program, VERT_PATH, FRAG_PATH, true);
	setProgramIO(vertices);

	// make dummy render target
	createRasteriseTarget();

	// init vertex buffer data
	vertexProcess(bones);
	vertexBarrier();
}

void PrimitiveProcessor::vertexProcess(std::array<glm::mat4, MAX_BONES> bones) {

	// setup the program

	glUseProgram(program);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glBindVertexArray(vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBufferIn);
	setVertexAttributes();

	updateUniforms(bones);
	glFinish();

	// run

	glDrawArrays(GL_TRIANGLE_STRIP, 0, vertexCount);
	cd::checkErrorsGL("GL process primitives");
}

void PrimitiveProcessor::vertexBarrier() {
	glFinish();
}

void PrimitiveProcessor::cleanUp() {
	glDeleteRenderbuffers(1, &renderbuffer);
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteBuffers(1, &vertexBufferOut);
}

// PRIVATE FUNCTIONS

void PrimitiveProcessor::createRasteriseTarget() {
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

void PrimitiveProcessor::setProgramIO(std::vector<cd::Vertex> &vertices) {

	// vertex input

	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);

	glGenBuffers(1, &vertexBufferIn);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBufferIn);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cd::Vertex) * vertexCount, vertices.data(), GL_STATIC_DRAW);

	setVertexAttributes();

	// vertex output TODO use transform feedback

	glGenBuffers(1, &vertexBufferOut);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBufferOut);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec4) * vertexCount, NULL, GL_DYNAMIC_COPY);

	GLuint vertexOutIndex = 0;
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vertexOutIndex, vertexBufferOut);

	cd::checkErrorsGL("primitive pipeline set io");
}

void PrimitiveProcessor::setVertexAttributes() {
	std::array<size_t, 3> offsets = cd::Vertex::getOffsets();
	size_t stride = sizeof(glm::vec4) + sizeof(glm::ivec4) + sizeof(glm::vec4);

	// position
	const GLuint positionLocation = 0;
	glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsets[0]);
	glEnableVertexAttribArray(positionLocation);

	// bone indices
	const GLuint indicesLocation = 1;
	glVertexAttribIPointer(indicesLocation, 4, GL_INT, stride, (void*)offsets[1]);
	glEnableVertexAttribArray(indicesLocation);

	// bone weights
	const GLuint weightsLocation = 2;
	glVertexAttribPointer(weightsLocation, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsets[2]); // TODO normalize
	glEnableVertexAttribArray(weightsLocation);
}

void PrimitiveProcessor::updateUniforms(std::array<glm::mat4, MAX_BONES> bones) {
	GLint location = glGetUniformLocation(program, "bones");
	if (location != -1)
		glUniformMatrix4fv(location, BONES_GL, GL_FALSE, (const GLfloat*)bones.data()); // TODO use uniform buffer object (50 * vec4...) large number of uniforms changing in bulk
	else
		CD_WARN("PrimitiveProcessor::updateUniforms invalid bone transform write");
}

/*
render texture target: https://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/  https://github.com/opengl-tutorials/ogl/blob/master/tutorial14_render_to_texture/tutorial14.cpp
opengl context: https://www.khronos.org/opengl/wiki/Creating_an_OpenGL_Context_(WGL)
wgl context: https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglcreatecontext

integer artribute: https://stackoverflow.com/questions/15844939/sending-array-to-shader-with-gluniformmatrix
*/
