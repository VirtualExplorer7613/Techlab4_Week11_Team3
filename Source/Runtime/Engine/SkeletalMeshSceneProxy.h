#pragma once

#include "Engine/PrimitiveSceneProxy.h"

class USkinnedMeshComponent;

// 스켈레탈 메시 전용 프록시. UStaticMesh를 쓰지 않고, 섹션은 에셋에서, 정점 버퍼는 컴포넌트의 스키닝 결과에서 받는다.
// GetMesh()가 nullptr이라 정적 묶음에 들어가지 않고, 컴포넌트가 매 프레임 패킷을 제출한다.
// (언리얼 FSkeletalMeshSceneProxy 대응)
class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FSkeletalMeshSceneProxy(USkinnedMeshComponent* InComponent);

	virtual void UpdateRenderState() override;
	virtual FVertexBuffer* GetVertexBuffer(uint32 LODIndex) const override;
	virtual FIndexBuffer* GetIndexBuffer(uint32 LODIndex) const override;

private:
	USkinnedMeshComponent* SkinnedComponent = nullptr;
};
