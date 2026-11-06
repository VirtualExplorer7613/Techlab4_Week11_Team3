#pragma once

#include "GameFramework/Actor.h"
#include "Components/BillboardComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FakeSpotLightActor.generated.h"

UCLASS()
class AFakeSpotLightActor : public AActor
{
	GENERATED_BODY()

public:
	AFakeSpotLightActor();
	virtual ~AFakeSpotLightActor() override = default;

	UDecalComponent* GetDecalComponent() const { return DecalComponent; }

private:
	UDecalComponent* DecalComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	UStaticMeshComponent* ArrowComponent = nullptr;
};