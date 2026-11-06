#pragma once

#include "UObject/Class.h"
#include "CameraModifier.generated.h"

class APlayerCameraManager;
struct FMinimalViewInfo;

UCLASS()
// 카메라 효과가 공통으로 사용하는 실행 인터페이스와 활성화 상태를 관리합니다.
// 실제 위치·회전·FOV 변화는 파생 Modifier가 계산하고, 이 클래스는 효과 강도 Alpha를 갱신합니다.
class UCameraModifier : public UObject
{
    GENERATED_BODY()

public:

    UCameraModifier();
    virtual ~UCameraModifier() override;

    // 소유 PCM의 비소유 참조를 연결합니다.
    // 같은 Modifier를 두 PCM이 동시에 소유하지 않도록 합니다.
    virtual void AddedToCamera(APlayerCameraManager* InCameraOwner);
    
    // UpdateAlpha를 호출하고 false를 반환합니다. 
    virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV);

    // 현재 Alpha를 유지하고 비활성화와 종료 대기 상태를 해제합니다.
    virtual void EnableModifier();

    // 즉시 종료 및 일반 종료에 대한 처리를 수행합니다.
    virtual void DisableModifier(bool bImmediate = false);

    // 효과가 완전히 비활성화되었는지 반환합니다.
    // 페이드 아웃 중인 상태와 구분해 PCM이 실행 여부를 판단하는 데 사용합니다.
    bool IsDisabled() const { return bDisabled; }

    // 효과가 페이드 아웃을 마친 뒤 꺼질 예정인지 반환합니다.
    bool IsPendingDisable() const { return bPendingDisable; }

    // 현재 효과 강도 Alpha를 반환합니다.
    float GetAlpha() const { return Alpha; }

    // 이 Modifier를 소유한 PCM을 반환합니다.
    APlayerCameraManager* GetCameraOwner() const { return CameraOwner; }

    // 작은 값부터 실행. 같은 우선순위는 등록 순서를 유지한다.
    uint8 Priority = 127;
    UPROPERTY(EditAnywhere)
    float AlphaInTime = 0.0f;
    UPROPERTY(EditAnywhere)
    float AlphaOutTime = 0.0f;

protected:
    // DeltaTime과 In/Out 시간으로 변화량을 구해 현재 Alpha를 목표 쪽으로 이동시킵니다.
    virtual void UpdateAlpha(float DeltaTime);

    APlayerCameraManager* CameraOwner = nullptr; // PCM 소유, 비소유 역참조
    float Alpha = 0.0f;
    bool bDisabled = true;
    bool bPendingDisable = false;
};
