
#include "Tools.h"

#include <iostream>
#include <glm/gtc/type_ptr.hpp>

glm::mat4 cd::convertMatrix(FbxAMatrix fbxMatrix) {
	glm::dvec4 c0 = glm::make_vec4((double*)fbxMatrix.GetColumn(0).Buffer());
	glm::dvec4 c1 = glm::make_vec4((double*)fbxMatrix.GetColumn(1).Buffer());
	glm::dvec4 c2 = glm::make_vec4((double*)fbxMatrix.GetColumn(2).Buffer());
	glm::dvec4 c3 = glm::make_vec4((double*)fbxMatrix.GetColumn(3).Buffer());
	return glm::transpose(glm::mat4(c0, c1, c2, c3));
}

////////////////// GET MESH //////////////////

void cd::getMesh(FbxNode* pNode, FbxMesh** mesh) {
	const char* nodeName = pNode->GetName();

	for (int i = 0; i < pNode->GetNodeAttributeCount(); i++) {
		FbxNodeAttribute* pAttribute = pNode->GetNodeAttributeByIndex(i);
		if (pAttribute->GetAttributeType() == FbxNodeAttribute::eMesh) {
			std::cout << "mesh name = " << pAttribute->GetName() << std::endl;
			*mesh = FbxCast<FbxMesh>(pAttribute);
		}
	}

	for (int j = 0; j < pNode->GetChildCount(); j++)
		getMesh(pNode->GetChild(j), mesh);
}

////////////////// PRINT NODES //////////////////

/* Tab character ("\t") counter */
int numTabs = 0;

/**
 * Print the required number of tabs.
 */
void PrintTabs() {
	for (int i = 0; i < numTabs; i++)
		printf("\t");
}

/**
 * Return a string-based representation based on the attribute type.
 */
FbxString GetAttributeTypeName(FbxNodeAttribute::EType type) {
	switch (type) {
	case FbxNodeAttribute::eUnknown: return "unidentified";
	case FbxNodeAttribute::eNull: return "null";
	case FbxNodeAttribute::eMarker: return "marker";
	case FbxNodeAttribute::eSkeleton: return "skeleton";
	case FbxNodeAttribute::eMesh: return "mesh";
	case FbxNodeAttribute::eNurbs: return "nurbs";
	case FbxNodeAttribute::ePatch: return "patch";
	case FbxNodeAttribute::eCamera: return "camera";
	case FbxNodeAttribute::eCameraStereo: return "stereo";
	case FbxNodeAttribute::eCameraSwitcher: return "camera switcher";
	case FbxNodeAttribute::eLight: return "light";
	case FbxNodeAttribute::eOpticalReference: return "optical reference";
	case FbxNodeAttribute::eOpticalMarker: return "marker";
	case FbxNodeAttribute::eNurbsCurve: return "nurbs curve";
	case FbxNodeAttribute::eTrimNurbsSurface: return "trim nurbs surface";
	case FbxNodeAttribute::eBoundary: return "boundary";
	case FbxNodeAttribute::eNurbsSurface: return "nurbs surface";
	case FbxNodeAttribute::eShape: return "shape";
	case FbxNodeAttribute::eLODGroup: return "lodgroup";
	case FbxNodeAttribute::eSubDiv: return "subdiv";
	default: return "unknown";
	}
}

/**
 * Print an attribute.
 */
void PrintAttribute(FbxNodeAttribute* pAttribute) {
	if (!pAttribute) return;

	FbxString typeName = GetAttributeTypeName(pAttribute->GetAttributeType());
	FbxString attrName = pAttribute->GetName();
	PrintTabs();
	// Note: to retrieve the character array of a FbxString, use its Buffer() method.
	printf("attribute type: %s -- name: %s\n", typeName.Buffer(), attrName.Buffer());
}

/**
 * Print a node, its attributes, and all its children recursively.
 */
void cd::PrintNode(FbxNode* pNode) {
	PrintTabs();
	const char* nodeName = pNode->GetName();
	FbxDouble3 translation = pNode->LclTranslation.Get();
	FbxDouble3 rotation = pNode->LclRotation.Get();
	FbxDouble3 scaling = pNode->LclScaling.Get();

	// Print the contents of the node.
	printf("node: %s t: %f %f %f r: %f %f %f\n",
		nodeName,
		translation[0], translation[1], translation[2],
		rotation[0], rotation[1], rotation[2]
	);

	// Print the node's attributes.
	for (int i = 0; i < pNode->GetNodeAttributeCount(); i++)
		PrintAttribute(pNode->GetNodeAttributeByIndex(i));

	// Recursively print the children.
	numTabs++;
	for (int j = 0; j < pNode->GetChildCount(); j++) {
		printf("\n");
		PrintNode(pNode->GetChild(j));
	}

	numTabs--;
	//PrintTabs();
}
