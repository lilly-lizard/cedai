#version 430

#define MAX_BONES 50
// also defined in VertexGl.hpp

layout(location = 0) in vec4 position_in;
layout(location = 1) in ivec4 bone_indices;
layout(location = 2) in vec4 bone_weights;

layout(std140, binding = 0) buffer POSITION_OUT
{
	vec4 position_out[];
};

uniform mat4 bones[MAX_BONES];

void main()
{
	vec4 position = position_in;
	for (int b = 0; b < 4; b++) {
		int bone_index = bone_indices[b];
		if (0 <= bone_index && bone_index < MAX_BONES)
			position = bones[bone_index] * position_in * bone_weights[b] + position;
	}

	position_out[gl_VertexID] = position;
}