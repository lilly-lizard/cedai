#pragma once

#include "tools/Vertex.hpp"

#include <GL/gl3w.h>
#include <glm/glm.hpp>

#include <vector>

class Interface;

class PrimitiveProcessor {
public:
	void init(Interface *interface, std::vector<glm::vec4> &positions, std::vector<glm::mat4> &bones);

	inline GLuint getVertexBuffer() { return vertexBufferOut; }

	void vertexProcess(std::vector<glm::mat4> &bones);
	void vertexBarrier();

	void cleanUp();

private:

	GLuint program;
	GLuint framebuffer, renderbuffer;

	GLuint vertexBufferIn, vertexBufferOut;
	GLuint vertexArray;

	std::vector<cd::Vertex> gl_vertices;
	uint32_t boneCount = 0, vertexCount = 0;

	void createRasteriseTarget();
	void setProgramIO(std::vector<glm::vec4> &positions);

	void setVertexAttributes();
	void updateUniforms(std::vector<glm::mat4> &bones);
};
