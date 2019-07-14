#include "FBX_Loader.h"

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
std::vector<glm::uvec4> polygons;

// LOAD FBX

void getMesh(FbxNode* pNode, FbxMesh** mesh) {
	const char* nodeName = pNode->GetName();

	for (int i = 0; i < pNode->GetNodeAttributeCount(); i++) {
		FbxNodeAttribute* pAttribute = pNode->GetNodeAttributeByIndex(i);
		if (pAttribute->GetAttributeType() == FbxNodeAttribute::eMesh) {
			std::cout << "mesh name = " << pAttribute->GetName() << std::endl;
			*mesh = static_cast<FbxMesh*>(pAttribute);
		}
	}

	for (int j = 0; j < pNode->GetChildCount(); j++)
		getMesh(pNode->GetChild(j), mesh);
}

void loadFBX() {
	std::cout << "starting fbx read..." << std::endl;
	FbxManager* sdkManager = FbxManager::Create();
	FbxIOSettings* ios = FbxIOSettings::Create(sdkManager, IOSROOT);
	sdkManager->SetIOSettings(ios);

	FbxImporter* importer = FbxImporter::Create(sdkManager, "");
	if (!importer->Initialize(FBX_PATH, -1, sdkManager->GetIOSettings())) {
		printf("Call to FbxImporter::Initialize() failed.\n");
		printf("Error returned: %s\n\n", importer->GetStatus().GetErrorString());
		exit(-1);
	}

	FbxScene* scene = FbxScene::Create(sdkManager, "myScene");
	importer->Import(scene);
	importer->Destroy();

	FbxNode* rootNode = scene->GetRootNode();
	FbxMesh* mesh = nullptr;
	if (rootNode) {
		for (int i = 0; i < rootNode->GetChildCount(); i++)
			getMesh(rootNode->GetChild(i), &mesh);
	}

	if (mesh != nullptr) {
		std::cout << "mesh found!" << std::endl;
		std::cout << "reading..." << std::endl;
	}
	else {
		std::cout << "no mesh found." << std::endl;
		sdkManager->Destroy();
		return;
	}

	FbxVector4* controlPoints = mesh->GetControlPoints();
	std::cout << "vertex count = " << mesh->GetControlPointsCount() << std::endl;
	for (int i = 0; i < mesh->GetControlPointsCount(); i++) {
		FbxVector4 controlPoint = controlPoints[i];
		vertices.push_back(glm::vec4(-controlPoint[2], controlPoint[0], controlPoint[1], 0));
	}

	std::cout << "polygon count = " << mesh->GetPolygonCount() << std::endl;
	for (int i = 0; i < mesh->GetPolygonCount(); i++) {
		if (mesh->GetPolygonSize(i) != 3) {
			std::cout << "polygon " << i << " has " << mesh->GetPolygonSize(i) << " indices!" << std::endl;
			sdkManager->Destroy();
			return;
		}
		polygons.push_back(glm::uvec4(mesh->GetPolygonVertex(i, 0), mesh->GetPolygonVertex(i, 1), mesh->GetPolygonVertex(i, 2), 0));
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
	uint32_t num_polygons = polygons.size();
	output.write((char*) &num_vertices, sizeof(uint32_t));
	output.write((char*) &num_polygons, sizeof(uint32_t));

	output.write((char*) vertices.data(), sizeof(glm::vec4) * num_vertices);
	output.write((char*) polygons.data(), sizeof(glm::uvec4) * num_polygons);

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
		if (polygons_temp[21] != polygons[21])
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
*/