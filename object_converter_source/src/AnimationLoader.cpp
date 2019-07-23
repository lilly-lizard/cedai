#include "AnimationLoader.h"
#include "Tools.h"

#include <iostream>

int cd::loadAnimatedModel(FbxScene* scene) {

	// just the first animation stack
	FbxAnimStack* animStack = scene->GetCurrentAnimationStack();
	FbxString animStackName = animStack->GetName();

	// time and frames
	FbxTime animationTime = animStack->GetLocalTimeSpan().GetDuration();
	FbxLongLong frameCount = animationTime.GetFrameCount(FbxTime::EMode::eFrames24) + 1;

	FbxNode* rootNode = scene->GetRootNode();
	if (!rootNode) return -1; // todo error handling?

	// our model data
	std::vector<Vertex> vertices;
	std::vector<glm::uvec4> indices;
	AnimClip animation;

	// get mesh
	FbxMesh* mesh = nullptr;
	getMesh(rootNode, &mesh);
	if (mesh == nullptr) return -1;

	// global mesh transform
	FbxAMatrix meshTransformFbx = mesh->GetNode()->EvaluateGlobalTransform();
	glm::mat4 meshTransform = cd::convertMatrix(meshTransformFbx);

	// get vertices
	FbxVector4* controlPoints = mesh->GetControlPoints();
	std::cout << "vertex count = " << mesh->GetControlPointsCount() << std::endl;
	for (int i = 0; i < mesh->GetControlPointsCount(); i++) {
		Vertex vertex;

		// vertex position
		vertex.pos = glm::vec4(controlPoints[i][0], controlPoints[i][1], controlPoints[i][2], 0);
		vertex.pos = meshTransform * vertex.pos;
		vertices.push_back(vertex);
	}

	// get indices
	std::cout << "polygon count = " << mesh->GetPolygonCount() << std::endl;
	for (int i = 0; i < mesh->GetPolygonCount(); i++) {
		// ensure the whole mesh is made of triangles
		if (mesh->GetPolygonSize(i) != 3) return -1;
		indices.push_back(glm::uvec4(mesh->GetPolygonVertex(i, 0), mesh->GetPolygonVertex(i, 1), mesh->GetPolygonVertex(i, 2), 0));
	}

	// get skeleton bones
	bool skeletonFound = cd::findBones(animation.bones, rootNode);
	if (!skeletonFound) return -1;

	// load keyframes
	for (long f = 0; f < frameCount; f++) { // bind pose at frame 0
		Keyframe keyframe;

		// get keyframe time
		animationTime.SetFrame(f, FbxTime::EMode::eFrames24);
		keyframe.time = animationTime.GetSecondDouble();

		// get global transforms for each bone for this keyframe
		for (int j = 0; j < animation.bones.size(); j++) {
			FbxAMatrix globalTransform = animation.bones[j].node->EvaluateGlobalTransform(animationTime);
			keyframe.boneTransforms.push_back(globalTransform);
		}

		animation.frames.push_back(keyframe);
	}

	// find for a skin mesh deformer (todo vertex cache?)
	FbxSkin* skin = nullptr;
	for (int d = 0; d < mesh->GetDeformerCount(); d++) {
		FbxDeformer* deformer = mesh->GetDeformer(d);
		if (deformer->GetDeformerType() == FbxDeformer::EDeformerType::eSkin) {
			skin = FbxCast<FbxSkin>(deformer);
			break;
		}
	}
	if (skin == nullptr) return -1;

	// todo: same as meshTransformation??
	const FbxVector4 lT = mesh->GetNode()->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 lR = mesh->GetNode()->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 lS = mesh->GetNode()->GetGeometricScaling(FbxNode::eSourcePivot);
	FbxAMatrix geometryTransform = FbxAMatrix(lT, lR, lS);

	// process clusters ('clusters' of vertices for each bone)
	for (int c = 0; c < skin->GetClusterCount(); c++) {
		FbxCluster* cluster = skin->GetCluster(c);

		// find associated bone/node
		int boneIndex = getBoneIndex(animation.bones, cluster->GetLink()->GetName());
		if (boneIndex == -1) return -1;

		// todo what is this?
		FbxAMatrix transformMatrix;
		FbxAMatrix transformLinkMatrix;
		FbxAMatrix globalBindposeInverseMatrix;
		cluster->GetTransformMatrix(transformMatrix); // node containing link
		cluster->GetTransformLinkMatrix(transformLinkMatrix); // link node
		animation.bones[boneIndex].globalBindposeInverse = transformLinkMatrix.Inverse() * transformMatrix * geometryTransform;

		// loop through the vertices affected by this cluster
		int* indices = cluster->GetControlPointIndices();
		double* weights = cluster->GetControlPointWeights();
		for (int i = 0; i < cluster->GetControlPointIndicesCount(); i++) {

			// set bone reference and weighting for affected vertex
			vertices[indices[i]].boneWeights.push_back(
				BoneWeight{
					boneIndex,
					weights[i]
				});
		}
	}

	// TODO quaternian and offset for each bone
	// TODO shave down BoneWeights of the vertices to 4 bones each

	return 0;
}

// traverses node tree to find skeleton nodes
bool cd::findBones(std::vector<cd::Bone> &bones, FbxNode* node) {
	// check if this node is a bone
	bool isBone = false;
	for (int a = 0; a < node->GetNodeAttributeCount(); a++) {
		if (node->GetNodeAttributeByIndex(a)->GetAttributeType() == FbxNodeAttribute::eSkeleton)
			isBone = true;
	}

	if (isBone) {
		// load bones
		loadBones(bones, node, -1);
		return true;
	} else {
		// loop through node children to keep looking for a bone tree
		for (int n = 0; n < node->GetChildCount(); n++) {
			if (findBones(bones, node->GetChild(n)))
				return true;
		}
	}
	return false;
}

// makes bones from a skeleton node and it's children
void cd::loadBones(std::vector<cd::Bone> &bones, FbxNode *node, int parentIndex) {
	Bone bone;
	bone.node = node;
	bone.parentIndex = parentIndex;
	bone.name = node->GetName();
	bones.push_back(bone);

	int myindex = bones.size() - 1;
	for (int n = 0; n < node->GetChildCount(); n++)
		loadBones(bones, node->GetChild(n), myindex);
}

int cd::getBoneIndex(std::vector<cd::Bone>& bones, std::string boneName) {
	for (int j = 0; j < bones.size(); j++) {
		if (bones[j].name == boneName)
			return j;
	}
	return -1;
}

/*
stack overflow example: https://stackoverflow.com/questions/45690006/fbx-sdk-skeletal-animations
opengl skeletal animation overview: https://www.khronos.org/opengl/wiki/Skeletal_Animation

3d model optimisation: http://manual.notch.one/0.9.22/en/topic/optimising-3d-scenes-for-notch
opengl skeleton tutorial: http://ogldev.atspace.co.uk/www/tutorial38/tutorial38.html
more links: https://www.reddit.com/r/opengl/comments/4du56u/trying_to_understand_skeletal_animation/
*/