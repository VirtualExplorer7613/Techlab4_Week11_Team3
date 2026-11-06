#pragma once

#include "GameFramework/Pawn.h"
#include "DefaultPawn.generated.h"

class UCameraComponent;
class UStaticMeshComponent;

UCLASS()
class ADefaultPawn : public APawn
{
	GENERATED_BODY()
public:
	ADefaultPawn();
	void AddMovementInput(const FVector& WorldDirection, float ScaleValue) override;
	void AddControllerYawInput(float Value) override;
	void AddControllerPitchInput(float Value) override;
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }
	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
private:
	UCameraComponent* CameraComponent = nullptr;
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	float MoveSpeed = 20.0f;
};
