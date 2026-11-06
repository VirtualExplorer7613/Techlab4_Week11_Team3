#pragma once

#include "ShapeComponent.h"
#include "CapsuleComponent.generated.h"

// 로컬 Z축이 캡슐 축이다. HalfHeight는 반구 끝까지 포함한 길이 (UE와 같음).
UCLASS()
class UCapsuleComponent : public UShapeComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	float CapsuleHalfHeight = 1.0f;
	UPROPERTY(EditAnywhere)
	float CapsuleRadius = 0.5f;

	float GetScaledCapsuleHalfHeight() const;
	float GetScaledCapsuleRadius() const;
	virtual FBox CalcLocalBounds() const override;
	virtual void DrawDebug(FLineBatcher*, const FVector4&) const override;
};
