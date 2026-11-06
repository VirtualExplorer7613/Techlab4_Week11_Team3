#pragma once
#include "PointLightComponent.h"
#include "FireBallComponent.generated.h"

UCLASS()
class UFireBallComponent : public UPointLightComponent
{
	GENERATED_BODY()


public:
	UFireBallComponent();
	virtual ~UFireBallComponent() override;

	ELightType GetLightType() const { return ELightType::Fireball; }

	float GetRadius() const { return GetAttenuationRadius(); }
	FVector4 GetColor() const { return GetLightColor().ToVector4(); }
	float GetRadiusFallOff() const { return GetFalloff(); }

private:
};
