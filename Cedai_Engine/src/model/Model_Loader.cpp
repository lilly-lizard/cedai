#include "Model_Loader.hpp"

#include "tools/Log.hpp"

#include <iostream>
#include <fstream>

// READ CEDAI BINARY

bool fileExists(const std::string& filePath) {
	struct stat buffer;
	return (stat(filePath.c_str(), &buffer) == 0);
}

void cd::LoadModelv0(const std::string& filePath, std::vector<glm::vec4>& vertices, std::vector<glm::uvec4>& polygons) {
	if (!fileExists(filePath)) {
		CD_ERROR("file {} not found", filePath);
		throw std::runtime_error("model reader error");
	}

	std::ifstream input(filePath, std::ios::binary);
	const int version_number = 0;

	uint16_t version_in;
	input.read((char*)& version_in, sizeof(uint16_t));
	if (version_in != version_number) {
		CD_ERROR("file version number {} incomatible with reader version {}", version_in, version_number);
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

void cd::LoadModelv1(const std::string &filePath, AnimatedModel &model) {
	if (!fileExists(filePath)) {
		CD_ERROR("model reader file {} not found", filePath);
		throw std::runtime_error("model reader");
	}

	std::ifstream input(filePath, std::ios::binary);
	const int version_number = 1;

	uint16_t version_in;
	input.read((char *)& version_in, sizeof(uint16_t));
	if (version_in != version_number) {
		CD_ERROR("file version number {} incomatible with reader version {}", version_in, version_number);
		throw std::runtime_error("model reader error");
	}

	uint32_t num_vertices, num_keyframes, num_bones;
	input.read((char *)& num_vertices, sizeof(uint32_t));
	input.read((char *)& num_keyframes, sizeof(uint32_t));
	input.read((char *)& num_bones, sizeof(uint32_t));
	if (num_bones != MAX_BONES) {
		CD_ERROR("model reader incompatible bone count (file has MAX_BONES = {} we use MAX_BONES = {}", num_bones, MAX_BONES);
		throw std::runtime_error("model reader");
	}

	model.vertices.clear();
	model.vertices.resize(num_vertices);
	model.animation.keyframes.clear();
	model.animation.keyframes.resize(num_keyframes);

	input.read((char *) model.vertices.data(), sizeof(cd::Vertex) * num_vertices);
	input.read((char *) &model.animation.duration, sizeof(double));
	input.read((char *) model.animation.keyframes.data(), sizeof(cd::Keyframe) * num_keyframes);
}