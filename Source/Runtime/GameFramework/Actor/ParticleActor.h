#pragma once

#include "GameFramework/Actor.h"
#include "Components/ParticleSubUVComponent.h"
#include "ParticleActor.generated.h"

// Todo: subuv
UCLASS()
class AParticleActor : public AActor
{
	GENERATED_BODY()


public:
	AParticleActor();
	//UParticleSubUVComponent* GetParticleComponent() const;

private:
	UParticleSubUVComponent* ParticleComponent;
};
