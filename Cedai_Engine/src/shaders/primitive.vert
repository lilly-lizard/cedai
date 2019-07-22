#version 430

layout(std140, location = 0) in vec4 position_in;
layout(std140, location = 1) in ivec4 bone_indices;
layout(std140, location = 2) in vec4 bone_weights;

layout(std140, binding = 0) buffer POSITION_OUT
{
	vec4 position_out[];
};

uniform mat4 bones[10];

void main()
{
	vec4 position = position_in;
	for (int b = 0; b < 4; b++) {
		if (bone_indices[b] != -1) {
			position += bones[bone_indices[b]] * position;
	}	}

	position_out[gl_VertexID] = position;
}