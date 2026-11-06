#pragma once

#include "GameFramework/Actor.h"
#include "Pawn.generated.h"

class APlayerController;

UCLASS()
class APawn : public AActor
{
	GENERATED_BODY()
public:
	APawn();
	virtual void AddMovementInput(const FVector& WorldDirection, float ScaleValue) {}
	virtual void AddControllerYawInput(float Value) {}
	virtual void AddControllerPitchInput(float Value) {}
	void PossessedBy(APlayerController* NewController) { Controller = NewController; }
	void UnPossessed() { Controller = nullptr; }
	APlayerController* GetController() const { return Controller; }
private:
	APlayerController* Controller = nullptr;
};
