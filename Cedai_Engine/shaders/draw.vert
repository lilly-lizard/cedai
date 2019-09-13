#version 430

layout(location = 0) in vec2 pos;

layout(location = 0) out vec2 texCoord;

void main() {
	texCoord = pos;
	gl_Position = vec4(pos.x * 2 - 1, pos.y * -2 + 1, 0.0, 1.0);
}
