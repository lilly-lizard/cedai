#pragma once

#include <glm/glm.hpp>
#include <array>

#define MAX_BONES 50

namespace cd {
	struct VertexGl {
		glm::vec4 position = { 0, 0, 0, 1 };
		glm::ivec4 boneIndices = { -1, -1, -1, -1 };
		glm::vec4 boneWeights = { 0, 0, 0, 0 };

		static std::array<int, 3> getOffsets() {
			return std::array<int, 3> {
					offsetof(VertexGl, position),
					offsetof(VertexGl, boneIndices),
					offsetof(VertexGl, boneWeights)
			};
		}
	};
}