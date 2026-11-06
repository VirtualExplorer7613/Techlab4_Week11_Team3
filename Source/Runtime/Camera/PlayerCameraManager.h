#pragma once
#include "GameFramework/Actor.h"

#include "CameraModifier_CameraShake.h"
#include "CameraModifier_FOVPunch.h"

#include "PlayerCameraManager.generated.h"

struct FMinimalViewInfo
{
	FVector Location;
	FRotator Rotation;
	float FOV = 60.0f;
	float AspectRatio = 16.0f / 9.0f;
	float OrthoWidth = 16.0f;
	float NearClip = 0.1f;
	float FarClip = 10000.0f;
	bool bIsOrthogonal = false;

	void BlendViewInfo(const FMinimalViewInfo& Other, float Alpha);
	FMatrix CalculateViewMatrix() const;
	FMatrix CalculateProjectionMatrix() const;
};

struct FViewTarget
{
	AActor* Target = nullptr;
	uint32 TargetUUID = 0; // 파괴감지용
	FMinimalViewInfo POV;

	void SetTarget(AActor* InTarget)
	{
		Target = InTarget;
		TargetUUID = InTarget ? InTarget->GetUUID() : 0;
	}
};

enum class EViewTargetBlendFunction : uint8
{
	Linear,// alpha = t
	Cubic, // alpha = 3t^2 - 2t^3
	EaseIn, // alpha = t^e
	EaseOut, // 1- (1-t)^e
	EaseInOut,
	SmoothStep,
};

struct FViewTargetTransitionParams
{
	float BlendTime = 0.0f;
	float BlendExp = 2.0f;
	EViewTargetBlendFunction BlendFunction = EViewTargetBlendFunction::Cubic;
	bool bLockOutgoing = false;
	float GetBlendAlpha(float TimePct) const;

};
class UCameraModifier;
class APlayerController;

UCLASS()
class APlayerCameraManager : public AActor
{
	GENERATED_BODY()
public:
	APlayerCameraManager() {};
	~APlayerCameraManager() override;

	void AddCameraModifier(UCameraModifier* Modifier);
	void RemoveCameraModifier(UCameraModifier* Modifier);
	UCameraModifier* FindCameraModifierByClass(UClass* ModifierClass) const;

	const FViewTarget& GetViewTarget() const { return ViewTarget; }
	const FMinimalViewInfo& GetCameraCachePOV() const { return CameraCachePOV; }
	
	bool HasCameraCache() const { return bCameraCacheValid; }
	bool IsActorAlive(AActor* Actor, uint32 UUID) const;
	bool IsBlending() const { return PendingViewTarget.Target != nullptr; }
	// 이 액터의 눈으로 카메라를 보겠다.
	void SetViewTarget(AActor* NewTarget, const FViewTargetTransitionParams& Params = {});
	void UpdateViewTarget(FViewTarget& OutVT, float DeltaTime);
	void UpdateCamera(float DeltaTime);
	void InitializeFor(APlayerController* PC);
	void CheckViewTarget(FViewTarget& VT);
	void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);

	APlayerController* GetOwningPlayerController() const { return PlayerControllerOwner; }

	void StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color, bool bHoldWhenFinished);
	void UpdateFade(float DeltaTime);
	void StopCameraFade();

	UCameraShakeBase* StartCameraShake(UClass* ShakeClass, float Scale = 1.0f);
	void StopCameraShake(UCameraShakeBase* ShakeInstance, bool bImmediately = false);
	void StopAllCameraShakes(bool bImmediately = false);

	UCameraModifier_FOVPunch* PlayFOVPunch(UCurveFloat* Curve, float Duration, float Strength);
	UCameraModifier_FOVPunch* PlayFOVPunch(float Strength, float InTime, float HoldTime, float OutTime);
	void StopFOVPunch(bool bImmediately);


	void SetLetterbox(float TargetAspect, float BlendTime);
	void UpdateLetterbox(float DeltaTime);

public:
	FLinearColor FadeColor{ 0, 0, 0, 1 };   // 지금은 초기값이 없음
	float FadeAmount = 0.0f;
	bool  bEnableFading = false;            // UE APlayerCameraManager::bEnableFading
	float LetterboxAmount = 0.0f;
	float LetterboxAspect = 2.39f;
	float LetterboxTarget = 0.0f;       // 추가
	float LetterboxBlendSpeed = 0.0f;   // 추가
	float VignetteIntensity = 0.0f;

	FVector2 FadeAlpha;
	float FadeTime;
	float FadeTimeRemaining;
	bool bHoldFadeWhenFinished;
	bool bAutoAnimateFade;       // false = SetManualCameraFade로 FadeAmount 직접 지정


	FName CameraStyle;
	FViewTarget ViewTarget;

protected:
	TArray<UCameraModifier*> ModifierList;
	APlayerController* PlayerControllerOwner = nullptr;
	FMinimalViewInfo CameraCachePOV;
	bool bCameraCacheValid = false;

	FViewTarget PendingViewTarget;
	FViewTargetTransitionParams BlendParams;
	float BlendTimeToGo = 0.0f;
	bool  bFreezeOutgoing = false;

	// PCM이 직접 소유하는 시간 설정용 커브.
	UCurveFloat* FOVPunchCurve = nullptr;
};
