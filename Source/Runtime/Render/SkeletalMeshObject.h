#pragma once

#include "Containers/Array.h"
#include "Math/Box.h"
#include "Math/Matrix.h"
#include "Render/Buffer.h"
#include "Render/Vertex.h"

class USkeletalMesh;

// 컴포넌트 하나의 CPU 스키닝 결과. 정점을 CPU에서 변형해 이 컴포넌트 전용 Dynamic 정점 버퍼에 올린다.
// 에셋(USkeletalMesh)은 읽기만 한다. (언리얼 FSkeletalMeshObjectCPUSkin 대응)
class FSkeletalMeshObjectCPUSkin
{
public:
	// 바인드 자세 정점으로 채운 Dynamic 정점 버퍼를 만든다
	explicit FSkeletalMeshObjectCPUSkin(const USkeletalMesh* InSkeletalMesh);

	bool IsValid() const { return VertexBuffer != nullptr; }

	// SkinMatrices[본] = InvBindPose × ComponentSpace. 정점을 변형해 GPU에 올리고 바운드를 갱신한다
	void Update(const TArray<FMatrix>& SkinMatrices);

	FVertexBuffer* GetVertexBuffer() const { return VertexBuffer.get(); }
	// 스키닝 결과 정점의 로컬 AABB (컬링·그림자용)
	const FBox& GetLocalBounds() const { return LocalBounds; }

private:
	void Upload();

	const USkeletalMesh* SkeletalMesh = nullptr;
	TUniquePtr<FVertexBuffer> VertexBuffer;
	TArray<FVertex> SkinnedVertices;
	FBox LocalBounds;
};
