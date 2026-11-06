#pragma once

#include "Camera/CameraModifier.h"
#include "CameraModifier_FOVPunch.generated.h"

class UCurveFloat;

UCLASS()
// 커브를 따라 FOV를 일시적으로 변화시키는 효과입니다.
// 자신의 재생 시간을 커브 입력으로 바꾸고 계산한 변화량만 이번 프레임의 FOV에 더합니다.
class UCameraModifier_FOVPunch : public UCameraModifier
{
    GENERATED_BODY()

public:
    // 멤버 선언의 기본값으로 객체의 초기 상태를 준비합니다.
    // 재생이나 외부 연결은 별도의 시작·등록 함수에서 수행합니다.
    UCameraModifier_FOVPunch();

    // 기존 재생을 정리하고 커브 수명·지속 시간·강도를 검증한 뒤 새 재생을 시작합니다.
    // 재호출 정책은 중첩이 아니라 재시작입니다.
    void Play(UCurveFloat* InCurve, float InDuration, float InStrength = 5.0f);
    // 즉시 종료하거나 현재 커브값을 고정하고 Modifier Alpha를 줄입니다.
    // 페이드 중 커브 모양까지 변하면 예상한 강도에서 매끄럽게 줄지 않을 수 있습니다.
    void Stop(bool bImmediately = false);
    // Alpha·커브 수명을 확인하고 정규화 시간으로 평가한 값을 FOV에 더합니다.
    // 종료 후 다음 프레임의 기본 FOV가 그대로 사용되므로 원본 컴포넌트 값을 복구할 필요가 없습니다.
    virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
    // FOV 효과가 현재 재생 중인지 반환합니다.
    // Modifier 체인을 중단하는 ModifyCamera의 반환값과는 다른 재생 상태입니다.
    bool IsPlaying() const { return bPlaying; }
    // 현재 참조하는 커브 객체를 반환합니다.
    // 참조만 제공하며 데이터를 소유하거나 수정하지 않습니다.
    UCurveFloat* GetCurve() const { return Curve; }
    // 현재 효과에서 진행한 재생 시간을 반환합니다.
    // 조회만 수행하며 시간 갱신은 재생 업데이트 함수가 담당합니다.
    float GetElapsedTime() const { return ElapsedTime; }

    UPROPERTY(EditAnywhere)
    float Duration = 0.3f;
    UPROPERTY(EditAnywhere)
    float Strength = 5.0f; // degrees. 음수면 FOV 감소.

protected:
    // 비소유 참조. Curve의 소유자는 재생 중 수명을 보장해야 한다.
    // 이 효과에서만 커브 입력 시간을 [0, 1]로 사용한다.
    UCurveFloat* Curve = nullptr;
    float ElapsedTime = 0.0f;
    bool bPlaying = false;
    bool bStopping = false;
    float CurrentCurveValue = 0.0f;
    uint32 CurveUUID = 0;
};
