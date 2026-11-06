#pragma once

#include "Components/SceneComponent.h"
#include "SpringArmComponent.generated.h"

class FLineBatcher;
class UWorld;
class AActor;

void DrawSpringArms(UWorld* World, FLineBatcher* LineBatcher, const AActor* Selected);

UCLASS()
class USpringArmComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USpringArmComponent();

	void TickComponent(float DeltaTime) override;

	void DrawDebug(FLineBatcher* LineBatcher) const;

	void UpdateInEditor();
	void ResetCameraLag();

	UPROPERTY(EditAnywhere)
	float TargetArmLength = 4.0f;			// 팔 길이 (월드 단위)

	UPROPERTY(EditAnywhere)
	FVector SocketOffset;					// 팔 "끝"에서 카메라 공간 오프셋 (어깨 너머: Y+)

	UPROPERTY(EditAnywhere)
	FVector TargetOffset;					// 팔 "시작점" 월드 오프셋 (머리 높이: Z+)

	UPROPERTY(EditAnywhere)
	float ProbeSize = 0.12f;				// 충돌 구 반지름

	UPROPERTY(EditAnywhere)
	bool bDoCollisionTest = true;

	UPROPERTY(EditAnywhere)
	bool bUsePawnControlRotation = false;

	UPROPERTY(EditAnywhere)
	bool bInheritPitch = true;

	UPROPERTY(EditAnywhere)
	bool bInheritYaw = true;

	UPROPERTY(EditAnywhere)
	bool bInheritRoll = true;


	UPROPERTY(EditAnywhere)
	bool bEnableCameraLag = false;

	UPROPERTY(EditAnywhere)
	bool bEnableCameraRotationLag = false;

	UPROPERTY(EditAnywhere)
	float CameraLagSpeed = 10.0f;

	UPROPERTY(EditAnywhere)
	float CameraRotationLagSpeed = 10.0f;

	UPROPERTY(EditAnywhere)
	float CameraLagMaxDistance = 0.0f;		// 0 = 제한 없음

	UPROPERTY(EditAnywhere)
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere)
	bool bDrawOnlyIfSelected = false;

protected:
	void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime);
	FRotator GetTargetRotation() const;
	FVector PreviousDesiredLoc;
	FRotator PreviousDesiredRot;
	void OnRegister() override; // Lag 없이 한 번 계산해서 Previous* 초기화
private:
	FVector DebugArmOrigin = FVector();
	FVector DebugDesiredLoc = FVector();
	FVector DebugResultLoc = FVector();

	bool bDebugHasData = false;
	bool bDebugCollisionHit = false;
};