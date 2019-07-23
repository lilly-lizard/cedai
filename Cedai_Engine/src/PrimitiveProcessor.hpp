#pragma once

#include "model/VertexGl.hpp"

#include <GL/gl3w.h>
#include <glm/glm.hpp>

#include <vector>

class Interface;

class PrimitiveProcessor {
public:
	void init(Interface *interface, std::vector<cd::VertexGl> &vertices, std::vector<glm::mat4> &bones);

	inline GLuint getVertexBuffer() { return vertexBufferOut; }

	void vertexProcess(std::vector<glm::mat4> &bones);
	void vertexBarrier();

	void cleanUp();

private:

	GLuint program;
	GLuint framebuffer, renderbuffer;

	GLuint vertexBufferIn, vertexBufferOut;
	GLuint vertexArray;

	uint32_t boneCount = 0, vertexCount = 0;

	void createRasteriseTarget();
	void setProgramIO(std::vector<cd::VertexGl> &vertices);

	void setVertexAttributes();
	void updateUniforms(std::vector<glm::mat4> &bones);
};
