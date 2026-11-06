#pragma once
#include "MovementComponent.h"
#include "RotatingMovementComponent.generated.h"

UCLASS()
class URotatingMovementComponent : public UMovementComponent
{
	GENERATED_BODY()


public:	
	UPROPERTY(EditAnywhere)
	FRotator RotationRate = FRotator(0,0,0);
	UPROPERTY(EditAnywhere)
	FVector PivotTranslation = FVector::ZeroVector; // Rotate Pivot or Orbit
	UPROPERTY(EditAnywhere)
	bool bRotaionInLocalSpace = true;

	virtual void TickComponent(float DeltaTime) override;
};