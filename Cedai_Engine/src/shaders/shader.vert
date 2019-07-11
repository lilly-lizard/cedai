#version 430

layout(location = 0) in vec2 pos;
layout(location = 0) out vec2 texCoord;

void main() {
	texCoord = pos * vec2(0.5f, -0.5f) + 0.5f;
	gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}