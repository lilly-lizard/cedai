#pragma once

#include "Vertex.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <array>

namespace cd
{
	struct Keyframe {
		double time = 0;
		std::array<glm::mat4, MAX_BONES> boneTransforms;
		Keyframe() { boneTransforms.fill(glm::mat4(1.0)); }
	};

	struct AnimationClip {
		double duration = 0;
		std::vector<Keyframe> keyframes;
	};
}

struct AnimatedModel {
	std::vector<cd::Vertex> vertices;
	cd::AnimationClip animation;
};