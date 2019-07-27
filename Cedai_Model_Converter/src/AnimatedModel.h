#pragma once

#include <fbxsdk.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <string>

#define MAX_BONES 50

namespace cd
{
	struct Vertex {
		glm::vec4 position = { 0, 0, 0, 1 };
		glm::ivec4 boneIndices = { -1, -1, -1, -1 };
		glm::vec4 boneWeights = { 0, 0, 0, 0 };
	};

	struct Keyframe {
		double time = 0;
		std::array<glm::mat4, MAX_BONES> boneTransforms;
		Keyframe() { boneTransforms.fill(glm::mat4(1.0)); }
	};

	struct AnimationClip {
		double duration = 0;
		std::vector<Keyframe> keyframes;
	};

	struct Bone {
		FbxNode *node = nullptr;
		int parentIndex = -1;
		std::string name;
		glm::mat4 bindPose = glm::mat4(1.0);
	};

	struct BoneWeight {
		int boneIndex = -1;
		float weight = 0;
	};

	struct VertexBones {
		std::vector<BoneWeight> boneWeights;
	};
}

class AnimatedModel {
public:
	std::vector<cd::Vertex> vertices;
	cd::AnimationClip animation;

	void loadFBX(std::string filePath);

private:
	std::vector<cd::Bone> bones;

	int loadAnimatedModel(FbxScene *scene);

	void getMesh(FbxNode *pNode, FbxMesh **mesh);
	glm::mat4 convertMatrix(FbxMatrix fbxMatrix);
	glm::mat4 convertMatrix(FbxAMatrix fbxMatrix);

	bool findBones(FbxNode *node);
	void loadBones(FbxNode *node, int parentIndex);
	int getBoneIndex(std::string boneName);
};