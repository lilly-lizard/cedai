#version 430

layout(std140, location = 0) in vec4 position_in;
layout(std140, location = 1) in ivec4 bone_indices;
layout(std140, location = 2) in vec4 bone_weights;

layout(std140, binding = 0) buffer POSITION_OUT
{
	vec4 position_out[];
};

uniform UBO
{
	float time;
} ubo;

void main()
{
	position_out[gl_VertexID] = position_in + float(ubo.time);
}