#pragma once

#include "MeshComponent.h"
#include "Render/SkeletalMesh.h"
#include "Render/SkeletalMeshObject.h"
#include "SkinnedMeshComponent.generated.h"

UCLASS()
class USkinnedMeshComponent : public UMeshComponent
{
	GENERATED_BODY()
public:
	// 1. 에셋 지정
	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
	USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

	// 2. 피킹·바운드: 피킹은 바인드 자세 메시, 바운드는 스키닝 결과
	virtual const FStaticMeshData* GetMeshData() const override;
	virtual FBox CalcLocalBounds() const override;

	// 3. 머티리얼 슬롯
	virtual int32 GetNumMaterials() const override;
	virtual FString GetMaterialSlotName(int32 SlotIndex) const override;
	virtual UMaterial* GetDefaultMaterial(int32 SlotIndex) const override;

	// 4. 렌더링: 전용 프록시를 만들고, 매 프레임 섹션마다 스키닝 결과 버퍼로 패킷을 제출한다
	virtual FPrimitiveSceneProxy* CreatePrimitiveSceneProxy() override;
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext) override;
	FSkeletalMeshObjectCPUSkin* GetMeshObject() const { return MeshObject.get(); }

	// 5. 스키닝: Local 자세 → Component Space → Skinning Matrix → 정점 변형(MeshObject)
	void RefreshSkinning(const TArray<FMatrix>& LocalPose);
	const TArray<FMatrix>& GetComponentSpaceTransforms() const { return ComponentSpace; }

	// 6. 씬 저장/로드: 불러온 에셋이 기본값과 다르면 OnSkeletalMeshChanged
	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

protected:
	virtual void CreateRenderState() override;

	// 에셋이 바뀐 뒤 스키닝 결과·렌더 상태를 다시 만든다. 파생 클래스는 자세를 새 뼈 기준으로 맞춘다
	virtual void OnSkeletalMeshChanged();

	UPROPERTY(EditAnywhere)
	USkeletalMesh* SkeletalMesh = nullptr;

	// 뼈별 Component Space 행렬
	TArray<FMatrix> ComponentSpace;

private:
	// 에셋이 바뀌면 스키닝 결과(정점 버퍼)를 새로 만든다
	void RecreateMeshObject();

	// 이 컴포넌트 전용 스키닝 결과 (Dynamic Vertex Buffer)
	TUniquePtr<FSkeletalMeshObjectCPUSkin> MeshObject;
};
