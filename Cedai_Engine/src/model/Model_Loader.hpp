#pragma once

#include "AnimatedModel.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace cd {
	void LoadModelv0(const std::string &filePath, std::vector<glm::vec4> &vertices, std::vector<glm::uvec4> &polygons);
	void LoadModelv1(const std::string &filePath, AnimatedModel &model);
}