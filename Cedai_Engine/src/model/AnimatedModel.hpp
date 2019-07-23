#pragma once

#include "VertexGl.hpp"

#include <fbxsdk.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace cd
{
	struct Bone {
		FbxNode *node = nullptr;
		int parentIndex = -1;
		std::string name;
		FbxAMatrix globalBindposeInverse;

		Bone() { globalBindposeInverse.SetIdentity(); }
	};

	struct BoneWeight
	{
		int boneIndex = -1;
		float weight = 0;
	};

	struct VertexBones
	{
		std::vector<BoneWeight> boneWeights;
	};

	struct Keyframe
	{
		double time = 0;
		std::vector<glm::mat4> boneTransforms;
	};

	struct AnimClip
	{
		double duration = 0;
		std::vector<Keyframe> frames;
	};
}

class AnimatedModel {
public:
	std::vector<cd::VertexGl> vertices;
	cd::AnimClip animation;

	void loadFBX(std::string filePath);

private:
	std::vector<cd::Bone> bones;

	int loadAnimatedModel(FbxScene *scene);

	void getMesh(FbxNode *pNode, FbxMesh **mesh);
	glm::mat4 convertMatrix(FbxAMatrix fbxMatrix);

	bool findBones(FbxNode *node);
	void loadBones(FbxNode *node, int parentIndex);
	int getBoneIndex(std::string boneName);
};