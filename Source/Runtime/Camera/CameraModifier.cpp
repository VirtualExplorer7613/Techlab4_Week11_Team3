#include "EnginePCH.h"
#include "Camera/CameraModifier.h"
#include "Camera/PlayerCameraManager.h"

namespace 
{
	float SafeTime(float Time ){ return std::isfinite(Time) ? std::max(Time, 0.0f) : 0.0f; }
}

UCameraModifier::UCameraModifier() = default;

UCameraModifier::~UCameraModifier() = default;

// 소유 PCM 연결. 다른 PCM에 중복 등록되지 않도록 호출 규약 확인.
void UCameraModifier::AddedToCamera(APlayerCameraManager* InCameraOwner)
{
	if (!CameraOwner || CameraOwner == InCameraOwner)
	{
		CameraOwner = InCameraOwner;
	}
}

// UpdateAlpha 호출. 기본 클래스는 POV를 바꾸지 않고 체인을 계속한다.
bool UCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	UpdateAlpha(DeltaTime);
	return false;
}

// 현재 Alpha를 유지하며 활성화한다. 별도 시작값/경과시간 변수 없이 상태 플래그만 갱신.
void UCameraModifier::EnableModifier()
{
	bDisabled = false;
	bPendingDisable = false;
	if (SafeTime(AlphaInTime) == 0.0f){ Alpha = 1.0f; }
}

//  즉시 종료 또는 현재 Alpha부터 0까지 전환. 페이드 완료 후 bDisabled 설정
void UCameraModifier::DisableModifier(bool bImmediate)
{
	if (bImmediate || SafeTime(AlphaOutTime) == 0.0f || Alpha <= 0.0f)
	{
		Alpha = 0.0f;
		bDisabled = true;
		bPendingDisable = false;
		return;
	}
	if (!bDisabled) bPendingDisable = true;
}

// 현재 Alpha에 DeltaTime/AlphaInTime 또는 DeltaTime/AlphaOutTime을 가감하고 0~1 범위로 제한.
void UCameraModifier::UpdateAlpha(float DeltaTime)
{
	if (bDisabled) return;
	const float Target = bPendingDisable ? 0.0f : 1.0f;
	const float Duration = SafeTime(bPendingDisable ? AlphaOutTime : AlphaInTime);
	const float Step = (Duration > 0.0f) ? (DeltaTime / Duration) : 1.0f;

	// 현재 ALpha를 목표 쪽으로 이동시키고 0~1 범위로 제한. 페이드 완료 시 bDisabled 설정.
	Alpha = Target > Alpha ? std::min(Alpha + Step, Target) : std::max(Alpha - Step, Target);
	Alpha = std::clamp(Alpha, 0.0f, 1.0f);
	if (bPendingDisable && Alpha <= 0.0f)
	{
		bDisabled = true;
		bPendingDisable = false;
	}
}
