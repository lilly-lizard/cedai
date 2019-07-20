#version 430

layout(location = 1) in vec4 vertex;

layout(std140, binding = 0) buffer VERTICES_OUT
{
	vec4 vertices_out[];
};

uniform UBO
{
	float time;
} ubo;

void main() {
	vertices_out[gl_VertexID] = vertex + float(ubo.time);
}