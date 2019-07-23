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
		int boneIndex;
		float weight;
	};

	struct VertexBones
	{
		std::vector<BoneWeight> boneWeights;
	};

	struct Keyframe
	{
		double time;
		std::vector<FbxAMatrix> boneTransforms;
	};

	struct AnimClip
	{
		double duration;
		std::vector<Bone> bones;
		std::vector<Keyframe> frames;
	};
}

class AnimatedModel {
public:
	void loadFBX(std::string filePath);

	std::vector<cd::VertexGl> vertices;
	cd::AnimClip animation;

private:

	int loadAnimatedModel(FbxScene *scene);

	void getMesh(FbxNode *pNode, FbxMesh **mesh);
	glm::mat4 convertMatrix(FbxAMatrix fbxMatrix);

	bool findBones(std::vector<cd::Bone> &bones, FbxNode *node);
	void loadBones(std::vector<cd::Bone> &bones, FbxNode *node, int parentIndex);
	int getBoneIndex(std::vector<cd::Bone> &bones, std::string boneName);
};