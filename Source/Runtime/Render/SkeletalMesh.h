#pragma once

#include "Asset/RenderAsset.h"
#include "Render/Buffer.h"
#include "Render/SkeletalMeshData.h"
#include "SkeletalMesh.generated.h"

class UMaterial;

UCLASS()
class USkeletalMesh : public URenderAsset
{
	GENERATED_BODY()
public:
	// 1. 원본: 뼈, 바인드 자세 메시, 가중치. Data.Mesh가 섹션·머티리얼 슬롯·바운드·피킹 BVH를 가진다
	FSkeletalMeshData Data;
	// 2. 공유 인덱스 버퍼. 정점 버퍼는 컴포넌트마다 스키닝 결과가 달라 FSkeletalMeshObjectCPUSkin이 가진다
	TUniquePtr<FIndexBuffer> IndexBuffer;
	// 3. 슬롯별 기본 머티리얼
	TArray<UMaterial*> Materials;

	// 슬롯 머티리얼. 없으면 DefaultMaterial
	UMaterial* GetMaterial(uint32 SlotIndex) const;
};
