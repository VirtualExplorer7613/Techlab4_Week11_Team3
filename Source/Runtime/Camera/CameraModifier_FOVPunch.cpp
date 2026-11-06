#include "EnginePCH.h"
#include "Camera/CameraModifier_FOVPunch.h"
#include "Camera/PlayerCameraManager.h"
#include "Curves/CurveFloat.h"

UCameraModifier_FOVPunch::UCameraModifier_FOVPunch() = default;

// 기존 재생을 정리하고 커브 수명·지속 시간·강도를 검증한 뒤 새 재생을 시작합니다.
void UCameraModifier_FOVPunch::Play(UCurveFloat* InCurve, float InDuration, float InStrength)
{
    Stop(true);
    if (!TryGetLiveObjectUUID(InCurve, nullptr, CurveUUID) ||
        !std::isfinite(InDuration) || InDuration <= 0.0f || !std::isfinite(InStrength)) return;
    Curve = InCurve;
    Duration = InDuration;
    Strength = InStrength;
    ElapsedTime = 0.0f;
    CurrentCurveValue = Curve->GetFloatValue(0.0f);
    bPlaying = true;
    bStopping = false;
    EnableModifier();
}

// 즉시 종료하거나 현재 커브값을 고정하고 Modifier Alpha를 줄입니다.
void UCameraModifier_FOVPunch::Stop(bool bImmediately)
{
    DisableModifier(bImmediately);
    if (IsDisabled())
    {
        bPlaying = bStopping = false;
        Curve = nullptr;
        CurveUUID = 0;
        CurrentCurveValue = 0.0f;
    }
    else if (bPlaying)
        bStopping = true; // 현재 커브값을 고정하고 Alpha만 줄여 자연스럽게 종료한다
}

// Alpha·커브 수명을 확인하고 정규화 시간으로 평가한 값을 FOV에 더합니다.
bool UCameraModifier_FOVPunch::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    Super::ModifyCamera(DeltaTime, InOutPOV);
    uint32 LiveUUID = 0;
    if (!bPlaying || IsDisabled() || !TryGetLiveObjectUUID(Curve, nullptr, LiveUUID) || LiveUUID != CurveUUID)
    {
        Stop(true);
        return false;
    }
    if (!bStopping)
    {
        if (!std::isfinite(Duration) || Duration <= 0.0f) { Stop(true); return false; }
        ElapsedTime += std::isfinite(DeltaTime) ? std::max(DeltaTime, 0.0f) : 0.0f;
        if (ElapsedTime >= Duration) { Stop(true); return false; }
        CurrentCurveValue = Curve->GetFloatValue(std::clamp(ElapsedTime / Duration, 0.0f, 1.0f));
    }
    const float Offset = Strength * CurrentCurveValue * GetAlpha();
    if (std::isfinite(Offset)) InOutPOV.FOV += Offset;
    return false;
}
