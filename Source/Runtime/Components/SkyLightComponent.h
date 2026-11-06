#pragma once

#include "LightComponent.h"
#include "SkyLightComponent.generated.h"

UCLASS()
class USkyLightComponent : public ULightComponent
{
	GENERATED_BODY()
public:
	USkyLightComponent();
	virtual ~USkyLightComponent() override;

	ELightType GetLightType() const override { return ELightType::Sky; }
	
protected:
private:
};