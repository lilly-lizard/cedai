#include "Model_Loader.h"

#include "tools/Log.h"

#include <iostream>
#include <fstream>

#define VERSION_NUMBER 0

// READ CEDAI BINARY

bool fileExists(const std::string& filePath) {
	struct stat buffer;
	return (stat(filePath.c_str(), &buffer) == 0);
}

void cd::LoadModel(const std::string& filePath, std::vector<glm::vec4>& vertices, std::vector<glm::uvec4>& polygons) {
	if (!fileExists(filePath)) {
		CD_ERROR("file {} not found", filePath);
		throw std::runtime_error("model reader error");
	}

	std::ifstream input(filePath, std::ios::binary);

	uint16_t version_number;
	input.read((char*)&version_number, sizeof(uint16_t));
	if (version_number != VERSION_NUMBER) {
		CD_ERROR("file version number {} incomatible with reader version {}", version_number, VERSION_NUMBER);
		throw std::runtime_error("model reader error");
	}

	uint32_t num_vertices, num_polygons;
	input.read((char*)&num_vertices, sizeof(uint32_t));
	input.read((char*)&num_polygons, sizeof(uint32_t));

	vertices.clear();
	vertices.resize(num_vertices);
	polygons.clear();
	polygons.resize(num_polygons);

	input.read((char*)vertices.data(), sizeof(glm::vec4) * num_vertices);
	input.read((char*)polygons.data(), sizeof(glm::uvec4) * num_polygons);
}