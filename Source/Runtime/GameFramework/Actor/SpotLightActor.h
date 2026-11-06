#pragma once

#include "GameFramework/Actor/LightActorBase.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SpotLightActor.generated.h"


// 에디터에서 아이콘(빌보드)으로 보이고, 선택하면 스포트라이트 원뿔이 라인으로 그려진다.
UCLASS()
class ASpotLightActor : public ALightActorBase
{
	GENERATED_BODY()


public:
	ASpotLightActor();
	virtual ~ASpotLightActor() override = default;

    virtual ULightComponent* GetLightComponent() const { return SpotLightComponent; }
    USpotLightComponent* GetSpotLightComponent() const { return SpotLightComponent; }

private:
    USpotLightComponent* SpotLightComponent = nullptr;
	UStaticMeshComponent* ArrowComponent = nullptr;


};
