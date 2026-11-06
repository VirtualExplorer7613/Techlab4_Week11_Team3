#pragma once

#include "LightComponent.h"
#include "LocalLightComponent.generated.h"

UCLASS()
class ULocalLightComponent :public ULightComponent
{
	GENERATED_BODY()
public:
	ULocalLightComponent();
	virtual ~ULocalLightComponent() override;

	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InAttenuationRadius) { AttenuationRadius = std::max(InAttenuationRadius, 0.01f); }

private:
	UPROPERTY(EditAnywhere)
	float AttenuationRadius = 5.0f;
};