#pragma once

#include "Camera/CameraModifier.h"
#include "Containers/Array.h"
#include "CameraModifier_CameraShake.generated.h"

class UCameraShakeBase;

UCLASS()
// 재생 중인 여러 Shake를 보관하고 결과를 하나의 POV에 합산합니다.
// 개별 Shake의 시간 계산은 각 인스턴스에 맡기고, 생성, 중지, 종료 정리를 담당합니다.
class UCameraModifier_CameraShake : public UCameraModifier
{
    GENERATED_BODY()

public:
    
    UCameraModifier_CameraShake();

    virtual ~UCameraModifier_CameraShake() override;

    // Super로 Alpha를 갱신하고 각 Shake를 BasePOV 사본에 적용한 뒤 그 차이만 최종 POV에 더합니다.
    virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

    // 클래스,상속,생성 가능 여부를 검사하고 NewObject, CDO 설정 복사, StartShake를 거쳐 목록에 넣습니다.
    UCameraShakeBase* AddCameraShake(UClass* ShakeClass, float Scale = 1.0f);
    
	// 완전 비활성화 시 Super::DisableModifier를 호출하고, ActiveShakes를 모두 정리합니다..
    void DisableModifier(bool bImmediate);
    
    // 이 목록에 속한 인스턴스인지 확인하고 StopShake를 요청한 뒤 종료 항목을 정리합니다.
    void RemoveCameraShake(UCameraShakeBase* ShakeInstance, bool bImmediately = false);
    
    // 살아 있는 모든 소유 인스턴스에 StopShake를 요청합니다.
    // 일반 중지는 페이드 완료까지 목록에 남고 즉시 중지는 바로 정리됩니다.
    void RemoveAllCameraShakes(bool bImmediately = false);

    // 현재 재생 중인 Shake 목록을 읽기 전용으로 반환합니다.
    const TArray<UCameraShakeBase*>& GetActiveShakes() const { return ActiveShakes; }

protected:
    // 역순으로 목록을 순회해 완료된 항목을 먼저 목록에서 빼고 delete합니다.
    // 정방향 삭제 시 다음 항목을 건너뛰는 문제를 피하고, 전역 종료 시 이미 삭제된 포인터도 건너뜁니다.
    void RemoveFinishedShakes();

    // 소유 목록. 종료/파괴 시 명시적으로 해제. Outer는 자동 삭제를 보장하지 않는다.
    TArray<UCameraShakeBase*> ActiveShakes;
};
