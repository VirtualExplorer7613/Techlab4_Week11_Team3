#include "EnginePCH.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraModifier.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Curves/CurveFloat.h"

// =======================FMinimalViewInfo===============
FMatrix FMinimalViewInfo::CalculateViewMatrix() const
{
	const FMatrix InverseTranslation = FMatrix::MakeTranslation(Location * -1.0f);
	const FMatrix InverseRotation = Rotation.Quaternion().ToFMatrix().GetTransposed();
	return InverseTranslation * InverseRotation;
}

FMatrix FMinimalViewInfo::CalculateProjectionMatrix() const
{
	if (bIsOrthogonal)
	{
		const float Width = OrthoWidth;
		const float Height = Width / AspectRatio;

		const float XScale = 2.0f / Width;
		const float YScale = 2.0f / Height;

		const float ZScale = 1.0f / (FarClip - NearClip);
		const float ZOffset = -NearClip / (FarClip - NearClip);

		return FMatrix(
			0.0f, 0.0f, ZScale, 0.0f,
			XScale, 0.0f, 0.0f, 0.0f,
			0.0f, YScale, 0.0f, 0.0f,
			0.0f, 0.0f, ZOffset, 1.0f
		);
	}

	const float HalfFOV = FMath::DegreesToRadians(FOV) * 0.5f;

	const float YScale = 1.0f / tan(HalfFOV);
	const float XScale = YScale / AspectRatio;

	const float ZScale = FarClip / (FarClip - NearClip);
	const float ZOffset = -NearClip * FarClip / (FarClip - NearClip);

	return FMatrix(
		0.0f, 0.0f, ZScale, 1.0f,
		XScale, 0.0f, 0.0f, 0.0f,
		0.0f, YScale, 0.0f, 0.0f,
		0.0f, 0.0f, ZOffset, 0.0f
	);
}

void FMinimalViewInfo::BlendViewInfo(const FMinimalViewInfo& Other, float Alpha)
{
	Location = FMath::Lerp(Location, Other.Location, Alpha);
	Rotation = FMath::LerpRotator(Rotation, Other.Rotation, Alpha);
	FOV = FMath::Lerp(FOV, Other.FOV, Alpha);
	OrthoWidth = FMath::Lerp(OrthoWidth, Other.OrthoWidth, Alpha);
	NearClip = FMath::Lerp(NearClip, Other.NearClip, Alpha);
	FarClip = FMath::Lerp(FarClip, Other.FarClip, Alpha);
	if (Alpha >= 0.5f) bIsOrthogonal = Other.bIsOrthogonal;
}
// =======================FViewTargetTransitionParams===============
float FViewTargetTransitionParams::GetBlendAlpha(float TimePct) const
{
	const float T = FMath::Clamp(TimePct, 0.0f, 1.0f);
	// 2. 곡선 고르기
	switch (BlendFunction)
	{
	case EViewTargetBlendFunction::Linear:    return T;
	//case EViewTargetBlendFunction::Cubic:     return FMath::CubicInterp();
	case EViewTargetBlendFunction::SmoothStep:return FMath::CubicInterp(0.0f,0.0f,1.0f,0.0f,T);
	case EViewTargetBlendFunction::EaseIn:    return pow(T,BlendExp);
	case EViewTargetBlendFunction::EaseOut:   return 1-pow(1-T,BlendExp);
	case EViewTargetBlendFunction::EaseInOut: return FMath::InterpEaseInOut(0.0f, 1.0f, T, BlendExp);
	// 입맞에 맞게 커스텀해서 만들면됨.
	// 헤더파일의 EViewTargetBlendFunction에서 enum 선언필요
	}
	return T;
}

//// =======================APlayerCameraManager===============
APlayerCameraManager::~APlayerCameraManager()
{
	// 효과를 먼저 정리하고, 효과가 참조하던 커브를 마지막에 해제한다.
	for (UCameraModifier* Modifier : ModifierList)
	{
		uint32 UUID = 0;
		if (TryGetLiveObjectUUID(Modifier, nullptr, UUID))
			delete Modifier;
	}

	uint32 UUID = 0;
	if (TryGetLiveObjectUUID(FOVPunchCurve, nullptr, UUID))
		delete FOVPunchCurve;
}

void APlayerCameraManager::InitializeFor(APlayerController* PC)
{
	PlayerControllerOwner = PC;
	SetViewTarget(PC ? static_cast<AActor*>(PC->GetPawn()) : nullptr);
}

void APlayerCameraManager::SetViewTarget(AActor* NewTarget, const FViewTargetTransitionParams& Params)
{
	if (!NewTarget) NewTarget = PlayerControllerOwner;
	if (NewTarget == PendingViewTarget.Target) return;
	if (NewTarget == ViewTarget.Target && !PendingViewTarget.Target) return;

	BlendParams = Params;
	if (Params.BlendTime > 0.0f)
	{
		// 전환중 재전환 : 지금 화면을 출발점으로 고정
		if (PendingViewTarget.Target && bCameraCacheValid)
		{
			ViewTarget.POV = CameraCachePOV;
			bFreezeOutgoing = true;
		}
		PendingViewTarget.SetTarget(NewTarget);
		BlendTimeToGo = Params.BlendTime;
	}
	else
	{
		ViewTarget.SetTarget(NewTarget);
		PendingViewTarget.SetTarget(nullptr);
		bFreezeOutgoing = false;
	}		

}


void APlayerCameraManager::UpdateCamera(float DeltaTime) 
{
	//현재 대상 시점
	const bool bOutgoingLocked = bFreezeOutgoing || (PendingViewTarget.Target && BlendParams.bLockOutgoing);
	if (!bOutgoingLocked)
	{
		CheckViewTarget(ViewTarget);
		UpdateViewTarget(ViewTarget, DeltaTime);
	}
	FMinimalViewInfo NewPOV = ViewTarget.POV;
	
	// 전환 
	if (PendingViewTarget.Target)
	{
		CheckViewTarget(PendingViewTarget);
		UpdateViewTarget(PendingViewTarget, DeltaTime);
		BlendTimeToGo -= DeltaTime;
		if (BlendTimeToGo > 0.0f)
		{
			const float TimePct = 1.0f - BlendTimeToGo / BlendParams.BlendTime;
			NewPOV.BlendViewInfo(PendingViewTarget.POV, BlendParams.GetBlendAlpha(TimePct));
		}
		else
		{
			//  전환 완료: Pending → ViewTarget
			ViewTarget = PendingViewTarget;
			PendingViewTarget.SetTarget(nullptr);
			bFreezeOutgoing = false;
			BlendTimeToGo = 0.0f;
			NewPOV = ViewTarget.POV;
		}
	}
	//효과
	ApplyCameraModifiers(DeltaTime, NewPOV);
	
	//Fade
	if (bEnableFading && bAutoAnimateFade) UpdateFade(DeltaTime);
	UpdateLetterbox(DeltaTime);
	
	//결과저장
	CameraCachePOV = NewPOV;
	bCameraCacheValid = true;
}

void APlayerCameraManager::AddCameraModifier(UCameraModifier* Modifier)
{
	if (!Modifier || ModifierList.Find(Modifier) >= 0 ||
		(Modifier->GetCameraOwner() && Modifier->GetCameraOwner() != this)) return;
	Modifier->AddedToCamera(this);
	ModifierList.Add(Modifier);
}

void APlayerCameraManager::RemoveCameraModifier(UCameraModifier* Modifier)
{
	const int32 Index = ModifierList.Find(Modifier);
	if (Index < 0) return;
	ModifierList.RemoveAt(Index);
	delete Modifier;
}

UCameraModifier* APlayerCameraManager::FindCameraModifierByClass(UClass* ModifierClass) const
{
	if (!ModifierClass) return nullptr;
	for (UCameraModifier* Modifier : ModifierList)
		if (Modifier && Modifier->IsA(ModifierClass)) return Modifier;
	return nullptr;
}

UCameraShakeBase* APlayerCameraManager::StartCameraShake(UClass* ShakeClass, float Scale)
{
	auto* Modifier = static_cast<UCameraModifier_CameraShake*>(
		FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()));
	if (!Modifier)
	{
		Modifier = NewObject<UCameraModifier_CameraShake>(this);
		AddCameraModifier(Modifier);
	}
	return Modifier ? Modifier->AddCameraShake(ShakeClass, Scale) : nullptr;
}

void APlayerCameraManager::StopCameraShake(UCameraShakeBase* ShakeInstance, bool bImmediately)
{
	auto* Modifier = static_cast<UCameraModifier_CameraShake*>(
		FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()));
	if (Modifier) Modifier->RemoveCameraShake(ShakeInstance, bImmediately);
}

void APlayerCameraManager::StopAllCameraShakes(bool bImmediately)
{
	auto* Modifier = static_cast<UCameraModifier_CameraShake*>(
		FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()));
	if (Modifier) Modifier->RemoveAllCameraShakes(bImmediately);
}

// Curve를 이용한 FOV Punch 효과를 재생합니다. 
UCameraModifier_FOVPunch* APlayerCameraManager::PlayFOVPunch(UCurveFloat* Curve, float Duration, float Strength)
{
	auto* Modifier = static_cast<UCameraModifier_FOVPunch*>(
		FindCameraModifierByClass(UCameraModifier_FOVPunch::StaticClass()));
	if (!Modifier)
	{
		Modifier = NewObject<UCameraModifier_FOVPunch>(this);
		AddCameraModifier(Modifier);
	}
	if (!Modifier) return nullptr;
	Modifier->Play(Curve, Duration, Strength);
	return Modifier->IsPlaying() ? Modifier : nullptr;
}

// 시간으로 Curve를 구성해 효과를 재생
// 추가: 시간으로 커브를 구성해서 재생
UCameraModifier_FOVPunch* APlayerCameraManager::PlayFOVPunch(float Strength, float InTime, float HoldTime, float OutTime)
{
	// 우선 진입·복귀 시간은 양수, 유지 시간은 0 이상으로 제한한다.
	if (!std::isfinite(Strength) ||
		!std::isfinite(InTime) ||
		!std::isfinite(HoldTime) ||
		!std::isfinite(OutTime) ||
		InTime <= 0.0f || HoldTime < 0.0f || OutTime <= 0.0f)
	{
		return nullptr;
	}

	const float TotalTime = InTime + HoldTime + OutTime;
	if (!std::isfinite(TotalTime)) return nullptr;

	if (!FOVPunchCurve)
	{
		FOVPunchCurve = NewObject<UCurveFloat>(this);
		if (!FOVPunchCurve) return nullptr;
	}

	// 이전 재생을 끝내고 같은 커브를 다시 구성한다.
	StopFOVPunch(true);

	FOVPunchCurve->Reset();
	FOVPunchCurve->AddKey(0.0f, 0.0f);
	FOVPunchCurve->AddKey(InTime / TotalTime, 1.0f);
	FOVPunchCurve->AddKey(
		(InTime + HoldTime) / TotalTime, 1.0f);
	FOVPunchCurve->AddKey(1.0f, 0.0f);

	// 기존 함수를 호출해 Modifier 등록·재생 처리를 재사용한다.
	return PlayFOVPunch(FOVPunchCurve, TotalTime, Strength);
}
void APlayerCameraManager::StopFOVPunch(bool bImmediately)
{
	auto* Modifier = static_cast<UCameraModifier_FOVPunch*>(
		FindCameraModifierByClass(UCameraModifier_FOVPunch::StaticClass()));
	if (Modifier) Modifier->Stop(bImmediately);
}

bool APlayerCameraManager::IsActorAlive(AActor* Actor, uint32 UUID) const
{
	// 1. 레벨에 같은 주소가 있고 UUID가 같으면 살아 있음
	UWorld* World = GetWorld();
	if (!World || !Actor) return false;
	for (ULevel* Level : World->GetLevels())
		for (AActor* A : Level->GetActors())
			if (A == Actor) return A->GetUUID() == UUID;
	return false;
}

void APlayerCameraManager::CheckViewTarget(FViewTarget& VT)
{
	// 1. 살아 있으면 그대로
	if (IsActorAlive(VT.Target, VT.TargetUUID)) return;
	// 2. Pawn → PC 순서로 대체
	AActor* Fallback = nullptr;
	if (PlayerControllerOwner) Fallback = PlayerControllerOwner->GetPawn()
		? static_cast<AActor*>(PlayerControllerOwner->GetPawn()) : PlayerControllerOwner;
	VT.SetTarget(Fallback);
}

void APlayerCameraManager::UpdateViewTarget(FViewTarget& OutVT, float DeltaTime)
{
	if (OutVT.Target) OutVT.Target->CalcCamera(DeltaTime, OutVT.POV);
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	std::stable_sort(ModifierList.begin(), ModifierList.end(),
		[](const auto* A, const auto* B) { return A->Priority < B->Priority; });
	for (UCameraModifier* Modifier : ModifierList)
		if (!Modifier->IsDisabled() && Modifier->ModifyCamera(DeltaTime, InOutPOV))
			break;
	InOutPOV.FOV = std::isfinite(InOutPOV.FOV) ? std::clamp(InOutPOV.FOV, 1.0f, 179.0f) : 60.0f;
}

void APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float Duration,
	FLinearColor Color, bool bHoldWhenFinished)
{
	bEnableFading = true;
	FadeColor = Color;
	FadeAlpha = FVector2(FromAlpha, ToAlpha);
	FadeTime = Duration;
	FadeTimeRemaining = Duration;
	bAutoAnimateFade = true;
	bHoldFadeWhenFinished = bHoldWhenFinished;
}

// UpdateCamera에서 if (bEnableFading && bAutoAnimateFade) 일 때 호출
void APlayerCameraManager::UpdateFade(float DeltaTime)
{
	FadeTimeRemaining = std::max(FadeTimeRemaining - DeltaTime, 0.0f);   // 시간 먼저
	if (FadeTime > 0.0f)
		FadeAmount = FadeAlpha.X + (1.0f - FadeTimeRemaining / FadeTime) * (FadeAlpha.Y - FadeAlpha.X);
	if (!bHoldFadeWhenFinished && FadeTimeRemaining <= 0.0f)
		StopCameraFade();
}

void APlayerCameraManager::StopCameraFade()
{
	if (bEnableFading)
	{
		FadeAmount = FadeAlpha.Y;    // 목표값으로 끝내고
		bEnableFading = false;       // 렌더러로는 더 이상 안 보냄
	}
}

void APlayerCameraManager::SetLetterbox(float TargetAspect, float BlendTime)
{
	if (TargetAspect > 0.0f) LetterboxAspect = TargetAspect;
	LetterboxTarget = (TargetAspect > 0.0f) ? 1.0f : 0.0f;               // 0을 넘기면 해제
	LetterboxBlendSpeed = (BlendTime > 0.0f) ? 1.0f / BlendTime : 0.0f;
}

void APlayerCameraManager::UpdateLetterbox(float DeltaTime)
{
	if (LetterboxBlendSpeed <= 0.0f) { LetterboxAmount = LetterboxTarget; return; }
	const float Step = LetterboxBlendSpeed * DeltaTime;                  // 일정 속도 (FInterpConstantTo와 같은 식)
	LetterboxAmount += FMath::Clamp(LetterboxTarget - LetterboxAmount, -Step, Step);
}