#pragma once

#include "Components/RectLightComponent.h"
#include "GameFramework/Actor/LightActorBase.h"
#include "Components/StaticMeshComponent.h"
#include "RectLightActor.generated.h"

UCLASS()
class ARectLightActor : public ALightActorBase
{
	GENERATED_BODY()


public:
	ARectLightActor();
	virtual ~ARectLightActor() override = default;

    virtual ULightComponent* GetLightComponent() const { return RectLightComponent; }
    URectLightComponent* GetRectLightComponent() const { return RectLightComponent; }

private:
    URectLightComponent* RectLightComponent = nullptr;
	UStaticMeshComponent* ArrowComponent = nullptr;

};
