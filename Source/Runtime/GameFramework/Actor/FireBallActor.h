#pragma once
#include "GameFramework/Actor.h"
#include "Components/FireBallComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/RotatingMovementComponent.h"
#include "Components/ProjectileMovementComponent.h"
#include "FireBallActor.generated.h"

UCLASS()
class AFireBallActor : public AActor
{
	GENERATED_BODY()

public:
	AFireBallActor();
	UFireBallComponent* GetFireBallComponent() const { return FireballComponent; }

private:
	UFireBallComponent* FireballComponent;
	UStaticMeshComponent* StaticMeshComponent;

	URotatingMovementComponent* RotatingMovementComponent;
	UProjectileMovementComponent* ProjectileMovementComponent;
};