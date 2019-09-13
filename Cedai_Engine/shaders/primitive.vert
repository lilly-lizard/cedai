#version 430

#define GL_BONES 16
// also defined in Config.hpp and AnimatedModel.h in the model converter

layout(location = 0) in vec4 position_in;
layout(location = 1) in ivec4 bone_indices;
layout(location = 2) in vec4 bone_weights;

layout(std140, binding = 0) buffer POSITION_OUT
{
	vec4 position_out[];
};

uniform mat4 bones[GL_BONES];

void main()
{
	mat4 animation;
	float weight_remaining = 1;
	for (int b = 0; b < 4; b++) {
		int bone_index = bone_indices[b];
		if (0 <= bone_index && bone_index < GL_BONES) {
			animation += bones[bone_index] * bone_weights[b];
			weight_remaining -= bone_weights[b];
	}	}
	animation += mat4(1) * clamp(weight_remaining, 0.0f, 1.0f);
	
	position_out[gl_VertexID] = animation * position_in + animation[3];
	//position_out[gl_VertexID] = position_in;
}