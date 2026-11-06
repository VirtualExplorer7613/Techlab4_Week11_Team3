#pragma once

#include "GameFramework/Actor/LightActorBase.h"
#include "Components/SkyLightComponent.h"
#include "SkyLightActor.generated.h"

UCLASS()
class ASkyLightActor : public ALightActorBase
{
	GENERATED_BODY()


public:
	ASkyLightActor();
	virtual ~ASkyLightActor() override = default;

    virtual ULightComponent* GetLightComponent() const { return SkyLightComponent; }
    USkyLightComponent* GetSkyLightComponent() const { return SkyLightComponent; }

private:
    USkyLightComponent* SkyLightComponent = nullptr;

};
