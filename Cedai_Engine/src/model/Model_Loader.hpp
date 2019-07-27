#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace cd {

	void LoadModel(const std::string& filePath, std::vector<glm::vec4>& vertices, std::vector<glm::uvec4>& polygons);

}