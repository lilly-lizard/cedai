#include "AnimatedModel.hpp"
#include "tools/Log.hpp"

#include <iostream>
#include <glm/gtc/type_ptr.hpp>

using namespace cd;

void AnimatedModel::loadFBX(std::string filePath) {
	std::cout << "starting fbx read..." << std::endl;
	FbxManager *sdkManager = FbxManager::Create();
	FbxIOSettings *ios = FbxIOSettings::Create(sdkManager, IOSROOT);
	sdkManager->SetIOSettings(ios);

	FbxImporter *importer = FbxImporter::Create(sdkManager, "");
	if (!importer->Initialize(filePath.c_str(), -1, sdkManager->GetIOSettings())) {
		printf("Call to FbxImporter::Initialize() failed.\n");
		printf("Error returned: %s\n\n", importer->GetStatus().GetErrorString());
		sdkManager->Destroy();
		exit(-1);
	}

	FbxScene *scene = FbxScene::Create(sdkManager, "myScene");
	importer->Import(scene);
	importer->Destroy();

	// node hierarchy
	std::cout << "\n ~ node hierarchy:" << std::endl;
	FbxNode *rootNode = scene->GetRootNode();
	FbxMesh *mesh = nullptr;
	if (!rootNode) {
		printf("Error: no root node found in scene");
		sdkManager->Destroy();
		exit(-1);
	}

	// model processing
	int result = loadAnimatedModel(scene);
	if (result != 0) {
		std::cout << " -- animation loading error! --" << std::endl;
		sdkManager->Destroy();
		throw std::runtime_error("animated fbx model loading");
	} else {
		std::cout << " -- animation loading success! --" << std::endl;
	}

	sdkManager->Destroy();
}

int AnimatedModel::loadAnimatedModel(FbxScene *scene) {
	std::vector<glm::uvec3> indices;
	std::vector<cd::VertexGl> indexedVertices;
	std::vector<cd::VertexBones> vertexBones;

	// just the first animation stack
	FbxAnimStack *animStack = scene->GetCurrentAnimationStack();
	FbxString animStackName = animStack->GetName();

	// time and frames
	FbxTime animationTime = animStack->GetLocalTimeSpan().GetDuration();
	FbxLongLong frameCount = animationTime.GetFrameCount(FbxTime::EMode::eFrames24) + 1;
	animation.duration = animationTime.GetSecondDouble();

	FbxNode *rootNode = scene->GetRootNode();
	if (!rootNode) return -1; // todo error handling?

	// get mesh
	FbxMesh *mesh = nullptr;
	getMesh(rootNode, &mesh);
	if (mesh == nullptr) return -1;

	// global mesh transform
	FbxAMatrix meshTransformFbx = mesh->GetNode()->EvaluateGlobalTransform();
	glm::mat4 meshTransform = convertMatrix(meshTransformFbx);

	// get vertices
	FbxVector4 *controlPoints = mesh->GetControlPoints();
	std::cout << "vertex count = " << mesh->GetControlPointsCount() << std::endl;
	for (int i = 0; i < mesh->GetControlPointsCount(); i++) {
		VertexGl vertex;

		// vertex position
		vertex.position = glm::vec4(controlPoints[i][0], controlPoints[i][1], controlPoints[i][2], 0);
		vertex.position = meshTransform * vertex.position;
		indexedVertices.push_back(vertex);
	}

	// get indices
	std::cout << "polygon count = " << mesh->GetPolygonCount() << std::endl;
	for (int i = 0; i < mesh->GetPolygonCount(); i++) {
		// ensure the whole mesh is made of triangles
		if (mesh->GetPolygonSize(i) != 3) return -1;
		indices.push_back(glm::uvec3(mesh->GetPolygonVertex(i, 0), mesh->GetPolygonVertex(i, 1), mesh->GetPolygonVertex(i, 2)));
	}

	// get skeleton bones
	bool skeletonFound = findBones(rootNode);
	if (!skeletonFound) return -1;

	// load keyframes
	for (long f = 0; f < frameCount; f++) { // bind pose at frame 0
		Keyframe keyframe;

		// get keyframe time
		animationTime.SetFrame(f, FbxTime::EMode::eFrames24);
		keyframe.time = animationTime.GetSecondDouble();

		// get global transforms for each bone for this keyframe
		for (int j = 0; j < bones.size(); j++) {
			FbxAMatrix globalTransform = bones[j].node->EvaluateGlobalTransform(animationTime);
			keyframe.boneTransforms[j] = convertMatrix(globalTransform);
		}

		animation.frames.push_back(keyframe);
	}

	// find for a skin mesh deformer (todo vertex cache?)
	FbxSkin *skin = nullptr;
	for (int d = 0; d < mesh->GetDeformerCount(); d++) {
		FbxDeformer *deformer = mesh->GetDeformer(d);
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
	vertexBones.resize(indexedVertices.size());
	for (int c = 0; c < skin->GetClusterCount(); c++) {
		FbxCluster *cluster = skin->GetCluster(c);

		// find associated bone/node
		int boneIndex = getBoneIndex(cluster->GetLink()->GetName());
		if (boneIndex == -1) return -1;

		// todo what is this?
		FbxAMatrix transformMatrix;
		FbxAMatrix transformLinkMatrix;
		FbxAMatrix globalBindposeInverseMatrix;
		cluster->GetTransformMatrix(transformMatrix); // node containing link
		cluster->GetTransformLinkMatrix(transformLinkMatrix); // link node
		bones[boneIndex].globalBindposeInverse = transformLinkMatrix.Inverse() * transformMatrix * geometryTransform;

		// loop through the vertices affected by this cluster
		int *indices = cluster->GetControlPointIndices();
		double *weights = cluster->GetControlPointWeights();
		for (int i = 0; i < cluster->GetControlPointIndicesCount(); i++) {

			// set bone reference and weighting for affected vertex
			vertexBones[indices[i]].boneWeights.push_back(
				BoneWeight{
					boneIndex,
					(float)weights[i]
				});
		}
	}

	// TODO quaternian and offset for each bone
	// TODO shave down BoneWeights of the vertices to 4 bones each

	// add bone data to vertices (only the top 4)
	for (int v = 0; v < indexedVertices.size(); v++) {
		for (int w = 0; w < vertexBones[v].boneWeights.size() && w < 4; w++) {
			indexedVertices[v].boneIndices[w] = vertexBones[v].boneWeights[w].boneIndex;
			indexedVertices[v].boneWeights[w] = vertexBones[v].boneWeights[w].weight;
		}
	}

	// convert vertices array into non indexed array
	for (int p = 0; p < indices.size(); p++) {
		vertices.push_back(indexedVertices[indices[p].x]);
		vertices.push_back(indexedVertices[indices[p].y]);
		vertices.push_back(indexedVertices[indices[p].z]);
	}

	return 0;
}

void AnimatedModel::getMesh(FbxNode *pNode, FbxMesh **mesh) {
	const char *nodeName = pNode->GetName();

	for (int i = 0; i < pNode->GetNodeAttributeCount(); i++) {
		FbxNodeAttribute *pAttribute = pNode->GetNodeAttributeByIndex(i);
		if (pAttribute->GetAttributeType() == FbxNodeAttribute::eMesh) {
			std::cout << "mesh name = " << pAttribute->GetName() << std::endl;
			*mesh = FbxCast<FbxMesh>(pAttribute);
		}
	}

	for (int j = 0; j < pNode->GetChildCount(); j++)
		getMesh(pNode->GetChild(j), mesh);
}

// traverses node tree to find skeleton nodes
bool AnimatedModel::findBones(FbxNode *node) {
	// check if this node is a bone
	bool isBone = false;
	for (int a = 0; a < node->GetNodeAttributeCount(); a++) {
		if (node->GetNodeAttributeByIndex(a)->GetAttributeType() == FbxNodeAttribute::eSkeleton)
			isBone = true;
	}

	if (isBone) {
		// load bones
		loadBones(node, -1);
		return true;
	} else {
		// loop through node children to keep looking for a bone tree
		for (int n = 0; n < node->GetChildCount(); n++) {
			if (findBones(node->GetChild(n)))
				return true;
		}
	}
	return false;
}

// makes bones from a skeleton node and it's children
void AnimatedModel::loadBones(FbxNode *node, int parentIndex) {
	Bone bone;
	bone.node = node;
	bone.parentIndex = parentIndex;
	bone.name = node->GetName();
	bones.push_back(bone);

	int myindex = bones.size() - 1;
	for (int n = 0; n < node->GetChildCount(); n++)
		loadBones(node->GetChild(n), myindex);
}

int AnimatedModel::getBoneIndex(std::string boneName) {
	for (int j = 0; j < bones.size(); j++) {
		if (bones[j].name == boneName)
			return j;
	}
	return -1;
}

glm::mat4 AnimatedModel::convertMatrix(FbxAMatrix fbxMatrix) {
	glm::dvec4 c0 = glm::make_vec4((double *)fbxMatrix.GetColumn(0).Buffer());
	glm::dvec4 c1 = glm::make_vec4((double *)fbxMatrix.GetColumn(1).Buffer());
	glm::dvec4 c2 = glm::make_vec4((double *)fbxMatrix.GetColumn(2).Buffer());
	glm::dvec4 c3 = glm::make_vec4((double *)fbxMatrix.GetColumn(3).Buffer());
	return glm::transpose(glm::mat4(c0, c1, c2, c3));
}

/*
stack overflow example: https://stackoverflow.com/questions/45690006/fbx-sdk-skeletal-animations
opengl skeletal animation overview: https://www.khronos.org/opengl/wiki/Skeletal_Animation

3d model optimisation: http://manual.notch.one/0.9.22/en/topic/optimising-3d-scenes-for-notch
opengl skeleton tutorial: http://ogldev.atspace.co.uk/www/tutorial38/tutorial38.html
more links: https://www.reddit.com/r/opengl/comments/4du56u/trying_to_understand_skeletal_animation/
*/

/*
documentation: http://help.autodesk.com/view/FBX/2019/ENU/
cpp reference: https://help.autodesk.com/cloudhelp/2018/ENU/FBX-Developer-Help/cpp_ref/index.html

animation: https://stackoverflow.com/questions/45690006/fbx-sdk-skeletal-animations

matrix conversion: https://stackoverflow.com/questions/35245433/fbx-node-transform-calculation
blender fbx exporting problems: https://blog.mattnewport.com/fixing-scale-problems-exporting-fbx-files-from-blender-to-unity-5/
*/