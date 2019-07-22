#pragma once

#include "tools/Vertex.hpp"

#include <GL/gl3w.h>
#include <glm/glm.hpp>

#include <vector>

class Interface;

class PrimitiveProcessor {
public:
	void init(Interface* interface, std::vector<glm::vec4> & positions);

	inline GLuint getVertexBuffer() { return vertexBufferOut; }

	void vertexProcess(float time);
	void vertexBarrier();

	void cleanUp();

private:

	GLuint program;
	GLuint framebuffer, renderbuffer;

	GLuint vertexBufferIn, vertexBufferOut, ubo;
	GLuint vertexArray;

	std::vector<cd::Vertex> gl_vertices;
	uint32_t vertexCount = 0;
	GLfloat ubo_time = 0;

	void createRasteriseTarget();
	void setProgramIO(std::vector<glm::vec4>& positions);
	void setVertexAttributes();
};
