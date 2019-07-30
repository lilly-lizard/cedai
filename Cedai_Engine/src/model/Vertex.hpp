#pragma once

#include <glm/glm.hpp>
#include <array>

namespace cd {
	struct Vertex {
		glm::vec4 position = { 0, 0, 0, 1 };
		glm::ivec4 boneIndices = { -1, -1, -1, -1 };
		glm::vec4 boneWeights = { 0, 0, 0, 0 };

		static std::array<size_t, 3> getOffsets() {
			return std::array<size_t, 3> {
					offsetof(Vertex, position),
					offsetof(Vertex, boneIndices),
					offsetof(Vertex, boneWeights)
			};
		}
	};
}
