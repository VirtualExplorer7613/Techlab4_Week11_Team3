#pragma once

#include "GameFramework/Actor/LightActorBase.h"
#include "Components/PointLightComponent.h"
#include "PointLightActor.generated.h"

UCLASS()
class APointLightActor : public ALightActorBase
{
	GENERATED_BODY()


public:
	APointLightActor();
	virtual ~APointLightActor() override = default;

    virtual ULightComponent* GetLightComponent() const { return PointLightComponent; }
    UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }

private:
    UPointLightComponent* PointLightComponent = nullptr;

};
