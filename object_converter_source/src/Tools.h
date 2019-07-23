#pragma once

#include <fbxsdk.h>
#include <glm/glm.hpp>

namespace cd {
	void getMesh(FbxNode* pNode, FbxMesh** mesh);
	void PrintNode(FbxNode* pNode);
	glm::mat4 convertMatrix(FbxAMatrix fbxMatrix);
}