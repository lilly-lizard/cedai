#pragma once

#include "model/Vertex.hpp"

#include <GL/gl3w.h>
#include <glm/glm.hpp>

#include <vector>

class Interface;

class PrimitiveProcessor {
public:
	void init(Interface *interface, std::vector<cd::Vertex> &vertices, std::array<glm::mat4, MAX_BONES> &bones);

	inline GLuint getVertexBuffer() { return vertexBufferOut; }

	void vertexProcess(std::array<glm::mat4, MAX_BONES> &bones);
	void vertexBarrier();

	void cleanUp();

private:

	GLuint program;
	GLuint framebuffer, renderbuffer;

	GLuint vertexBufferIn, vertexBufferOut;
	GLuint vertexArray;
	uint32_t vertexCount = 0;

	void createRasteriseTarget();
	void setProgramIO(std::vector<cd::Vertex> &vertices);

	void setVertexAttributes();
	void updateUniforms(std::array<glm::mat4, MAX_BONES> &bones);
};
