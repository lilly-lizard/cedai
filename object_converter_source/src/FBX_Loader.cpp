#include "FBX_Loader.h"
#include "Tools.h"
#include "AnimationLoader.h"

#include <fbxsdk.h>
#include <fbxsdk/scene/geometry/fbxmesh.h>
#include <fbxsdk/core/math/fbxvector4.h>
#include <glm/glm.hpp>

#include <iostream>
#include <fstream>
#include <vector>

#define VERSION_NUMBER 0
#define FBX_PATH "maize.fbx"
#define OUT_PATH "maize.bin"
#define IN_PATH "maize.bin"

std::vector<glm::vec4> vertices;
std::vector<glm::uvec4> indices;

// LOAD FBX

void loadFBX() {
	std::cout << "starting fbx read..." << std::endl;
	FbxManager* sdkManager = FbxManager::Create();
	FbxIOSettings* ios = FbxIOSettings::Create(sdkManager, IOSROOT);
	sdkManager->SetIOSettings(ios);

	FbxImporter* importer = FbxImporter::Create(sdkManager, "");
	if (!importer->Initialize(FBX_PATH, -1, sdkManager->GetIOSettings())) {
		printf("Call to FbxImporter::Initialize() failed.\n");
		printf("Error returned: %s\n\n", importer->GetStatus().GetErrorString());
		sdkManager->Destroy();
		exit(-1);
	}

	FbxScene* scene = FbxScene::Create(sdkManager, "myScene");
	importer->Import(scene);
	importer->Destroy();

	// node hierarchy
	std::cout << "\n ~ node hierarchy:" << std::endl;
	FbxNode* rootNode = scene->GetRootNode();
	FbxMesh* mesh = nullptr;
	if (!rootNode) {
		printf("Error: no root node found in scene");
		sdkManager->Destroy();
		exit(-1);
	}

	// print details of all nodes
	cd::PrintNode(rootNode);
	printf("\n");


	// animation processing
	if (cd::loadAnimatedModel(scene) != 0) {
		std::cout << "animation loading error" << std::endl;
		sdkManager->Destroy();
		exit(-1);
	} else {
		std::cout << " -- animation loading success! --" << std::endl;
	}


	// find the mesh
	cd::getMesh(rootNode, &mesh);
	std::cout << " ~" << std::endl;

	if (mesh == nullptr) {
		std::cout << "error: no mesh found" << std::endl;
		sdkManager->Destroy();
		exit(-1);
	}

	std::cout << "mesh found!" << std::endl;
	std::cout << "reading..." << std::endl;

	// mesh transform TODO: should be local transform?? (same thing in this case)
	FbxAMatrix meshTransformFbx = mesh->GetNode()->EvaluateGlobalTransform();
	glm::mat4 meshTransform = cd::convertMatrix(meshTransformFbx);

	// get vertices
	FbxVector4* controlPoints = mesh->GetControlPoints();
	std::cout << "vertex count = " << mesh->GetControlPointsCount() << std::endl;
	for (int i = 0; i < mesh->GetControlPointsCount(); i++) {
		glm::vec4 vertex = glm::vec4(controlPoints[i][0], controlPoints[i][1], controlPoints[i][2], 0);
		vertex = meshTransform * vertex;
		vertices.push_back(vertex);
	}

	// get indices
	std::cout << "polygon count = " << mesh->GetPolygonCount() << std::endl;
	for (int i = 0; i < mesh->GetPolygonCount(); i++) {
		if (mesh->GetPolygonSize(i) != 3) {
			std::cout << "polygon " << i << " has " << mesh->GetPolygonSize(i) << " indices!" << std::endl;
			sdkManager->Destroy();
			return;
		}
		indices.push_back(glm::uvec4(mesh->GetPolygonVertex(i, 0), mesh->GetPolygonVertex(i, 1), mesh->GetPolygonVertex(i, 2), 0));
	}

	std::cout << "fbx load success!" << std::endl;
	sdkManager->Destroy();
}

// WRITE CEDAI BINARY

void writeBinary() {
	std::cout << "starting model binary write..." << std::endl;
	std::ofstream output(OUT_PATH, std::ios::binary);

	uint16_t version_number = VERSION_NUMBER;
	output.write((char*) &version_number, sizeof(uint16_t));

	uint32_t num_vertices = vertices.size();
	uint32_t num_polygons = indices.size();
	output.write((char*) &num_vertices, sizeof(uint32_t));
	output.write((char*) &num_polygons, sizeof(uint32_t));

	output.write((char*) vertices.data(), sizeof(glm::vec4) * num_vertices);
	output.write((char*) indices.data(), sizeof(glm::uvec4) * num_polygons);

	output.close();
	std::cout << "model write success!" << std::endl;
}

// READ CEDAI BINARY

bool fileExists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

void readBinary() {
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

	uint32_t num_vertices, num_polygons;
	input.read((char*) &num_vertices, sizeof(uint32_t));
	input.read((char*) &num_polygons, sizeof(uint32_t));

	//vertices.clear();
	//polygons.clear();
	std::vector<glm::vec4> vertices_temp(num_vertices);
	std::vector<glm::uvec4> polygons_temp(num_polygons);

	input.read((char*) vertices_temp.data(), sizeof(glm::vec4) * num_vertices);
	input.read((char*) polygons_temp.data(), sizeof(glm::vec4) * num_polygons);

	for (int v = 0; v < num_vertices; v++) {
		if (vertices_temp[21] != vertices[21])
			std::cout << "v not equal: " << v << std::endl;
	}

	for (int p = 0; p < num_polygons; p++) {
		if (polygons_temp[21] != indices[21])
			std::cout << "p not equal: " << p << std::endl;
	}
}

// MAIN

void main() {
	loadFBX();
	writeBinary();
	//readBinary();
	std::cout << "exiting..." << std::endl;
}

/*
TODO: casting?? (remember pixels bug...)
documentation: http://help.autodesk.com/view/FBX/2019/ENU/
cpp reference: https://help.autodesk.com/cloudhelp/2018/ENU/FBX-Developer-Help/cpp_ref/index.html

animation: https://stackoverflow.com/questions/45690006/fbx-sdk-skeletal-animations

matrix conversion: https://stackoverflow.com/questions/35245433/fbx-node-transform-calculation
blender fbx exporting problems: https://blog.mattnewport.com/fixing-scale-problems-exporting-fbx-files-from-blender-to-unity-5/
*/