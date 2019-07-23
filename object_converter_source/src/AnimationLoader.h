#pragma once

#include <fbxsdk.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace cd
{
	int loadAnimatedModel(FbxScene* scene);

	struct Bone {
		FbxNode* node = nullptr;
		int parentIndex = -1;
		std::string name;
		FbxAMatrix globalBindposeInverse;

		Bone() { globalBindposeInverse.SetIdentity(); }
	};

	struct BoneWeight
	{
		int boneIndex;
		double weight;
	};

	struct Vertex
	{
		glm::vec4 pos;
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

	bool findBones(std::vector<cd::Bone>& bones, FbxNode* node);
	void loadBones(std::vector<cd::Bone>& bones, FbxNode* node, int parentIndex);
	int getBoneIndex(std::vector<cd::Bone>& bones, std::string boneName);
}