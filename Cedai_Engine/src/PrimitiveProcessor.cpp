#include "PrimitiveProcessor.hpp"
#include "Interface.hpp"
#include "tools/Log.hpp"

#include <array>

#define VERT_PATH "src/shaders/primitive.vert"
#define FRAG_PATH "src/shaders/primitive.frag"

#define MAX_BONES 50

// PUBLIC FUNCTIONS

void PrimitiveProcessor::init(Interface *interface, std::vector<glm::vec4> &positions, std::vector<glm::mat4> &bones) {
	CD_INFO("Initialising primitive processing program...");
	vertexCount = positions.size();
	int boneCount = bones.size();

	// create program
	cd::createProgramGL(program, VERT_PATH, FRAG_PATH);
	setProgramIO(positions);

	// make dummy render target
	createRasteriseTarget();

	// init vertex buffer data
	vertexProcess(bones);
	vertexBarrier();
}

void PrimitiveProcessor::vertexProcess(std::vector<glm::mat4> &bones) {

	// setup the program

	glUseProgram(program);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glBindVertexArray(vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBufferIn);
	setVertexAttributes();

	updateUniforms(bones);

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

void PrimitiveProcessor::setProgramIO(std::vector<glm::vec4> &positions) {

	// vertex input

	gl_vertices.resize(vertexCount);
	for (int v = 0; v < vertexCount; v++) {
		gl_vertices[v].position = positions[v];
		gl_vertices[v].boneIndices = glm::ivec4(-1, -1, -1, -1);
		gl_vertices[v].boneWeights = glm::vec4(0, 0, 0, 0);
	}

	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);

	glGenBuffers(1, &vertexBufferIn);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBufferIn);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cd::Vertex) * vertexCount, gl_vertices.data(), GL_STATIC_DRAW);

	setVertexAttributes();

	// vertex output

	glGenBuffers(1, &vertexBufferOut);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBufferOut);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec4) * vertexCount, NULL, GL_DYNAMIC_COPY);

	GLuint vertexOutIndex = 0;
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, vertexOutIndex, vertexBufferOut);

	cd::checkErrorsGL("primitive pipeline set io");
}

void PrimitiveProcessor::setVertexAttributes() {
	std::array<int, 3> offsets = cd::Vertex::getOffsets();
	size_t stride = sizeof(glm::vec4) + sizeof(glm::ivec4) + sizeof(glm::vec4);

	// position
	int positionLocation = 0;
	glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsets[0]);
	glEnableVertexAttribArray(positionLocation);

	// bone indices
	int indicesLocation = 1;
	glVertexAttribPointer(indicesLocation, 4, GL_INT, GL_FALSE, stride, (void*)offsets[1]);
	glEnableVertexAttribArray(indicesLocation);

	// bone weights
	int weightsLocation = 2;
	glVertexAttribPointer(weightsLocation, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsets[2]);
	glEnableVertexAttribArray(weightsLocation);
}

void PrimitiveProcessor::updateUniforms(std::vector<glm::mat4> &bones) {
	GLint location = glGetUniformLocation(program, "bones");
	int boneCount = bones.size();

	if (location != -1 && boneCount <= MAX_BONES)
		glUniformMatrix4fv(location, boneCount, GL_FALSE, (const GLfloat*)bones.data());
	else
		CD_WARN("PrimitiveProcessor::updateUniforms invalid bone transform write");
}

/*
render texture target: https://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/  https://github.com/opengl-tutorials/ogl/blob/master/tutorial14_render_to_texture/tutorial14.cpp
opengl context: https://www.khronos.org/opengl/wiki/Creating_an_OpenGL_Context_(WGL)
wgl context: https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglcreatecontext
*/