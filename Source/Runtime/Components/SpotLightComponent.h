#pragma once

#include "PointLightComponent.h"
#include "SpotLightComponent.generated.h"

class FLineBatcher;

UCLASS()
class USpotLightComponent : public UPointLightComponent
{
	GENERATED_BODY()


public:
	USpotLightComponent() = default;
	virtual ~USpotLightComponent() override = default;

	virtual FLightSceneProxy* CreateSceneProxy() const override;

	void DrawDebug(FLineBatcher* LineBatcher) const override;
	ELightType GetLightType() const override { return ELightType::Spot; }

	float GetInnerConeAngle() const { return InnerConeAngle; }
	void SetInnerConeAngle(float InAngle) { InnerConeAngle = InAngle; }

	float GetOuterConeAngle() const { return OuterConeAngle; }
	void SetOuterConeAngle(float InAngle) { OuterConeAngle = InAngle; }

	

private:
	// 원뿔 중심축 기준 반각(도). 
	UPROPERTY(EditAnywhere)
	float InnerConeAngle = 15.0f;
	UPROPERTY(EditAnywhere)
	float OuterConeAngle = 30.0f;
};
