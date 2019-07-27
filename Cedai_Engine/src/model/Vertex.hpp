#pragma once

#include <glm/glm.hpp>
#include <array>

#define MAX_BONES 50

namespace cd {
	struct Vertex {
		glm::vec4 position;
		glm::ivec4 boneIndices;
		glm::vec4 boneWeights;

		static std::array<int, 3> getOffsets() {
			return std::array<int, 3> {
				offsetof(Vertex, position),
					offsetof(Vertex, boneIndices),
					offsetof(Vertex, boneWeights)
			};
		}
	};
}