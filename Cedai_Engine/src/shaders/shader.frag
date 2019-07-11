#version 430

uniform usampler2D srcTex;
layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 color;

void main() {
	uvec4 tex = texture(srcTex, texCoord);
	color = vec4(float(tex.x) / 255, float(tex.y) / 255, float(tex.z) / 255, 1);
}

/*

uniform usampler2D srcTex;
in vec2 texCoord;
out vec4 color;

void main() {
	uvec4 tex = texture(srcTex, texCoord);
	color = uvec4(float(tex.x) / 255, float(tex.y) / 255, float(tex.z) / 255, float(tex.w) / 255);
}

*/

//texelFetch(srcTex, ivec2(texCoord.x * 640, texCoord.y * 480), 0);

//color = texture(srcTex, texCoord);