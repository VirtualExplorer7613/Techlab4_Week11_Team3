#pragma once

#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Math/Matrix.h"
#include "Render/StaticMeshData.h"

struct FBoneInfo
{
	FString Name;
	int32 ParentIndex = -1; // Root = -1. 항상 자기 인덱스보다 작다
	FMatrix RefLocal = FMatrix::Identity; // 부모에서 본 공간 기준
	FMatrix InvBindPose = FMatrix::Identity; // 메시 공간 → 바인드 순간 뼈 공간
};

// 정점 하나가 따라가는 뼈 최대 4개
struct FSkinWeight
{
	int32 BoneIndices[4] = { 0, 0, 0, 0 };
	float Weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct FSkeletalMeshData
{
	TArray<FBoneInfo> Bones;
	FStaticMeshData Mesh;
	TArray<FSkinWeight> SkinWeights; // Mesh.Vertices와 같은 순서
};
