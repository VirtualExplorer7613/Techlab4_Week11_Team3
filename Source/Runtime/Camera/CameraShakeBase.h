#pragma once

#include "Math/EngineMath.h"
#include "UObject/Class.h"
#include "CameraShakeBase.generated.h"

struct FMinimalViewInfo;

// 진동의 시작 위상을 0으로 둘지 무작위로 정할지 선택합니다.
// 선택한 위상은 재생을 시작할 때 한 번 정하고 이후 프레임에서는 유지합니다.
enum class EInitialWaveOscillatorOffsetType : uint8
{
    Random,
    Zero,
};

// 한 채널의 진폭,주파수,초기 위상 방식을 보관합니다.
// 입력한 시간에 해당하는 진동 값을 계산하며 재생 상태는 소유하지 않습니다.
struct FWaveOscillator
{
    float Amplitude = 0.0f; // 위치: 월드 단위, 회전: degrees, FOV: degrees
    float Frequency = 1.0f; // Hz
    EInitialWaveOscillatorOffsetType InitialOffsetType = EInitialWaveOscillatorOffsetType::Random;

    // Amplitude × sin(2π × Frequency × 시간 + 위상)을 계산합니다.
    // double로 계산한 위상을 한 주기 안으로 줄입니다.
    float Evaluate(float ElapsedTime, float InitialPhase) const;
};

// 한 프레임에 계산한 위치,회전,FOV 오프셋을 담습니다.
// 원본 카메라 값과 효과의 변화량을 분리해 적용과 합산에 사용합니다.
struct FCameraShakeUpdateResult
{
    FVector LocationOffset = FVector(0.0f);
    FRotator RotationOffset = FRotator(0.0f, 0.0f, 0.0f);
    float FOVOffset = 0.0f;
};

UCLASS()
// 한 번 재생되는 카메라 흔들림의 시간과 진동 값을 관리합니다.
// 별도 패턴 계층 없이 Wave를 직접 계산하고 결과를 전달받은 POV에 적용합니다.
class UCameraShakeBase : public UObject
{
    GENERATED_BODY()

public:
    // 멤버 선언의 기본값으로 객체의 초기 상태를 준비합니다.
    // 재생이나 외부 연결은 별도의 시작·등록 함수에서 수행합니다.
    UCameraShakeBase();
    // 객체가 종료될 때 기본 정리 경로를 수행합니다.
    // 이 클래스가 직접 소유한 자원이 있다면 그 정리만 담당하고 관찰 참조는 삭제하지 않습니다.
    virtual ~UCameraShakeBase() override;

    void CopySettingsFrom(const UCameraShakeBase& Soruce);

    // 시간·정지 상태·배율을 초기화하고 초기 위상을 한 번 정합니다.
    // 같은 인스턴스를 재시작해도 이전 시간이나 정지 상태가 남지 않습니다.
    virtual void StartShake(float Scale = 1.0f);
    // UpdateShake로 값을 구하고 로컬 위치를 기준 POV 회전으로 월드 방향으로 변환합니다.
    // Scale·BlendWeight·Alpha를 각각 한 번 적용합니다.
    virtual void UpdateAndApplyCameraShake(float DeltaTime, float Alpha, FMinimalViewInfo& InOutPOV);
    // 즉시 종료하거나 현재 BlendWeight를 캡처한 뒤 정지 페이드를 시작합니다.
    // 반복 Stop이 종료 시점을 계속 뒤로 미루지 않게 이미 정지 중이면 재시작하지 않습니다.
    virtual void StopShake(bool bImmediately = false);
    // 해당 Shake의 재생이 끝났는지 반환합니다.
    // Shake Modifier가 종료된 인스턴스를 목록에서 정리할 때 사용합니다.
    bool IsFinished() const { return bIsFinished; }
    // 현재 효과에서 진행한 재생 시간을 반환합니다.
    // 조회만 수행하며 시간 갱신은 재생 업데이트 함수가 담당합니다.
    float GetElapsedTime() const { return ElapsedTime; }
    // 현재 Shake의 시작·종료 블렌드 가중치를 반환합니다.
    // 진폭과 별도로 효과가 나타나고 사라지는 강도를 확인하는 데 사용합니다.
    float GetBlendWeight() const { return BlendWeight; }

    // Blend In/Out을 포함한 전체 시간(초). 0 이하는 즉시 종료한다.
    UPROPERTY(EditAnywhere)
    float Duration = 1.0f;
    UPROPERTY(EditAnywhere)
    float BlendInTime = 0.1f;
    UPROPERTY(EditAnywhere)
    float BlendOutTime = 0.2f;

    // 로컬 위치: X 전방, Y 오른쪽, Z 위쪽. 복합 구조체는 커스텀 UI에서 편집한다.
    FWaveOscillator LocationX;
    FWaveOscillator LocationY;
    FWaveOscillator LocationZ;
    FWaveOscillator RotationPitch;
    FWaveOscillator RotationYaw;
    FWaveOscillator RotationRoll;
    FWaveOscillator FOV;

protected:
    // 출력값을 초기화하고 시간을 진행한 뒤 블렌드·종료 상태를 갱신하고 채널별 값을 평가합니다.
    // 종료된 Shake는 오프셋 0으로 반환되어 마지막 값이 계속 적용되지 않습니다.
    virtual void UpdateShake(float DeltaTime, FCameraShakeUpdateResult& OutResult);
    // Zero는 0, Random은 [0, 2π)에서 채널마다 위상을 선택합니다.
    // 매 프레임 난수를 뽑으면 연속적인 진동 대신 불연속적인 떨림이 됩니다.
    void InitializeOscillatorPhases();
    // 자연 재생은 In/Out 가중치의 최솟값, 중도 정지는 저장 강도 × 남은 페이드 비율을 반환합니다.
    // 짧은 재생에서 Blend In/Out이 겹치는 경우에도 가중치 범위를 유지합니다.
    float CalculateBlendWeight() const;

    float ShakeScale = 1.0f;
    float ElapsedTime = 0.0f;
    float BlendWeight = 0.0f;
    float StopElapsedTime = 0.0f;
    float StopStartBlendWeight = 0.0f;
    bool bIsStopping = false;
    bool bIsFinished = true;

    // 시작할 때 한 번 선택하는 radians 위상. 회전 오프셋(degrees)과 구분한다.
    FVector InitialLocationPhase = FVector(0.0f);
    FVector InitialRotationPhase = FVector(0.0f); // X=Pitch, Y=Yaw, Z=Roll
    float InitialFOVPhase = 0.0f;
};
