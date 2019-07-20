#version 430

uniform usampler2D srcTex;
layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 color;

void main() {
	uvec4 tex = texture(srcTex, texCoord);
	color = vec4(float(tex.x) / 255, float(tex.y) / 255, float(tex.z) / 255, 1);
}