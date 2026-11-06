#pragma once

#include "LocalLightComponent.h"
#include "PointLightComponent.generated.h"

UCLASS()
class UPointLightComponent : public ULocalLightComponent
{
	GENERATED_BODY()
public:
	UPointLightComponent();
	virtual ~UPointLightComponent() override;

	virtual FLightSceneProxy* CreateSceneProxy() const override;

	void DrawDebug(FLineBatcher* LineBatcher) const override;
	ELightType GetLightType() const override { return ELightType::Point; }

	float GetFalloff() const { return LightFalloffExponent; }
	void SetFalloff(float InFalloff) { LightFalloffExponent = InFalloff; }
	float GetSourceRadius() const { return SourceRadius; }
	void SetSourceRadius(float InRadius) { SourceRadius = InRadius; }


protected:
	UPROPERTY(EditAnywhere)
	float LightFalloffExponent = 1.0f;
	UPROPERTY(EditAnywhere)
	float SourceRadius = 0.02f;
private:
	
};
