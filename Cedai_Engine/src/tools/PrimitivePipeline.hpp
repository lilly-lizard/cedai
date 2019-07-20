#pragma once

#include <GL/gl3w.h>
#include <glm/glm.hpp>

#include <vector>

class Interface;

class PrimitivePipeline {
public:
	void init(Interface* interface, std::vector<glm::vec4> &vertices);

	inline GLuint getVertexBuffer() { return vertexBufferOut; }

	void vertexProcess(float time);
	void vertexBarrier();

	void cleanUp();

private:

	GLuint program;
	GLuint vertexBufferIn;
	GLuint framebuffer;
	GLuint renderbuffer;

	GLint vertexLocation;
	GLuint vertexBufferOut;
	GLuint ubo;
	GLfloat ubo_time = 0;

	uint32_t vertexCount = 0;

	void createRasteriseTarget();
	void setProgramIO(std::vector<glm::vec4>& vertices);
};
