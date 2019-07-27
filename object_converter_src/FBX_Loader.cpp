#include "FBX_Loader.h"
#include "AnimatedModel.h"
#include "Tools.h"

#include <fbxsdk.h>
#include <fbxsdk/scene/geometry/fbxmesh.h>
#include <fbxsdk/core/math/fbxvector4.h>
#include <glm/glm.hpp>

#include <iostream>
#include <fstream>
#include <vector>

#define FBX_PATH "maize.fbx"
#define OUT_PATH "maize.bin"
#define IN_PATH "maize.bin"

// MAIN

void writeBinary(const AnimatedModel &model);
void readBinary(AnimatedModel &model);

int main() {
	AnimatedModel model;
	try {
		model.loadFBX(FBX_PATH);
		writeBinary(model);
		AnimatedModel model_in;
		readBinary(model_in);
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	std::cout << "exiting..." << std::endl;
	return 0;
}

// WRITE CEDAI BINARY

void writeBinary(const AnimatedModel &model) {
	std::cout << "starting model binary write..." << std::endl;
	std::ofstream output(OUT_PATH, std::ios::binary);
	
	uint16_t version_number = VERSION_NUMBER;
	output.write((char*) &version_number, sizeof(uint16_t));
	
	uint32_t num_vertices = model.vertices.size();
	uint32_t num_keyframes = model.animation.keyframes.size();
	uint32_t num_bones = MAX_BONES;
	output.write((char*) &num_vertices, sizeof(uint32_t));
	output.write((char*) & num_keyframes, sizeof(uint32_t));
	output.write((char*) & num_bones, sizeof(uint32_t));
	
	output.write((char*) model.vertices.data(), sizeof(cd::Vertex) * num_vertices);
	output.write((char*) &model.animation.duration, sizeof(double));
	output.write((char*) model.animation.keyframes.data(), sizeof(cd::Keyframe) * num_keyframes);
	
	output.close();
	std::cout << "model write success!" << std::endl;
}

// READ CEDAI BINARY

bool fileExists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

void readBinary(AnimatedModel &model) {
	if (!fileExists(IN_PATH)) {
		std::cout << "model reader file not found" << std::endl;
		return;
	}

	std::ifstream input(IN_PATH, std::ios::binary);

	uint16_t version_number;
	input.read((char*) &version_number, sizeof(uint16_t));
	if (version_number != VERSION_NUMBER) {
		std::cout << "model reader incompatible version number" << std::endl;
		return;
	}

	uint32_t num_vertices, num_keyframes, num_bones;
	input.read((char*) &num_vertices, sizeof(uint32_t));
	input.read((char*) &num_keyframes, sizeof(uint32_t));
	input.read((char*) &num_bones, sizeof(uint32_t));
	if (num_bones != MAX_BONES) throw std::runtime_error("file read: incompatible bone count");

	model.vertices.clear();
	model.vertices.resize(num_vertices);
	model.animation.keyframes.clear();
	model.animation.keyframes.resize(num_keyframes);

	input.read((char*) model.vertices.data(), sizeof(cd::Vertex) * num_vertices);
	input.read((char*) &model.animation.duration, sizeof(double));
	input.read((char*) model.animation.keyframes.data(), sizeof(cd::Keyframe) * num_keyframes);

	std::cout << "model read success!" << std::endl;
}

/*
TODO: casting?? (remember pixels bug...)
documentation: http://help.autodesk.com/view/FBX/2019/ENU/
cpp reference: https://help.autodesk.com/cloudhelp/2018/ENU/FBX-Developer-Help/cpp_ref/index.html

animation: https://stackoverflow.com/questions/45690006/fbx-sdk-skeletal-animations

matrix conversion: https://stackoverflow.com/questions/35245433/fbx-node-transform-calculation
blender fbx exporting problems: https://blog.mattnewport.com/fixing-scale-problems-exporting-fbx-files-from-blender-to-unity-5/
*/