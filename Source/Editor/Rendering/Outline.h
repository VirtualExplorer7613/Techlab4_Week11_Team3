#pragma once

#include "Components/PrimitiveComponent.h"
#include "Engine/PrimitiveSceneProxy.h"

class FOutline
{
public:
	void SetTarget(UPrimitiveComponent* InTarget) { Target = InTarget; }
	UPrimitiveComponent* GetTarget() const { return Target; }
	// 그릴 버퍼는 프록시에서 받는다. 스태틱은 에셋 LOD0, 스켈레탈은 스키닝 결과라 휜 모양 그대로 외곽선이 생긴다
	FVertexBuffer* GetVertexBuffer() const { return GetProxy() ? GetProxy()->GetVertexBuffer(0) : nullptr; }
	FIndexBuffer* GetIndexBuffer() const { return GetProxy() ? GetProxy()->GetIndexBuffer(0) : nullptr; }
	FMatrix GetWorldMatrix() const { return Target->GetWorldMatrix(); }
	const FVector& GetTargetScale() const { return Target->GetRelativeScale3D(); }
private:
	const FPrimitiveSceneProxy* GetProxy() const { return Target ? Target->GetSceneProxy() : nullptr; }

	UPrimitiveComponent* Target = nullptr;

};