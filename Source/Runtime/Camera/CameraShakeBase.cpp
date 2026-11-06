#include "EnginePCH.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include <random>

namespace
{
	float NonNegative(float Value) { return std::isfinite(Value) ? std::max(Value, 0.0f) : 0.0f; }
	
	// 위상 0 또는 [0, 2π)에서 한 번 선택합니다. 재생 중에는 유지합니다.
    float InitialPhaseFor(const FWaveOscillator& Wave)
	{
		static std::mt19937 Generator(std::random_device{}());
		static std::uniform_real_distribution<float> Distribution(0.0f, 2.0f * PI);
		return Wave.InitialOffsetType == EInitialWaveOscillatorOffsetType::Random ? Distribution(Generator) : 0.0f;
	}
}

// 역할: Amplitude × sin(2π × Frequency × 시간 + 위상)을 계산합니다.
// 설명: double로 계산한 위상을 한 주기 안으로 줄입니다.
float FWaveOscillator::Evaluate(float ElapsedTime, float InitialPhase) const
{
	if (!std::isfinite(Amplitude) || !std::isfinite(Frequency) || !std::isfinite(ElapsedTime) || !std::isfinite(InitialPhase))
		return 0.0f;

	// double로 위상 계산 후 주기를 줄여 긴 재생/큰 주파수에 오차를 줄인다.
	const double Phase = std::remainder(2.0 * PI * static_cast<double>(Frequency) * static_cast<double>(ElapsedTime) + static_cast<double>(InitialPhase), 2.0 * PI);
	return Amplitude * static_cast<float>(std::sin(Phase));
}

// 역할: 멤버 선언의 기본값으로 객체의 초기 상태를 준비합니다.
// 설명: 재생이나 외부 연결은 별도의 시작·등록 함수에서 수행합니다.
UCameraShakeBase::UCameraShakeBase() = default;
// 역할: 객체가 종료될 때 기본 정리 경로를 수행합니다.
// 설명: 이 클래스가 직접 소유한 자원이 있다면 그 정리만 담당하고 관찰 참조는 삭제하지 않습니다.
UCameraShakeBase::~UCameraShakeBase() = default;

// CDO에서 시간 설정과 7개 채널의 진동 설정을 복사합니다.
void UCameraShakeBase::CopySettingsFrom(const UCameraShakeBase& Source)
{
	Duration = Source.Duration;
	BlendInTime = Source.BlendInTime;
	BlendOutTime = Source.BlendOutTime;
	LocationX = Source.LocationX, LocationY = Source.LocationY; LocationZ = Source.LocationZ;
	RotationPitch = Source.RotationPitch, RotationYaw = Source.RotationYaw; RotationRoll = Source.RotationRoll;
	FOV = Source.FOV;
}

//  시간,정지 상태, 배율을 초기화하고 초기 위상을 한 번 정합니다.
//  같은 인스턴스를 재시작해도 이전 시간이나 정지 상태가 남지 않습니다.
void UCameraShakeBase::StartShake(float Scale)
{
	ShakeScale = Scale;
	ElapsedTime = StopElapsedTime = StopStartBlendWeight = 0.0f;
	bIsStopping = false;
	bIsFinished = !std::isfinite(Duration) || Duration <= 0.0f || ShakeScale == 0.0f;
	InitializeOscillatorPhases();
	BlendWeight = IsFinished() ? 0.0f : CalculateBlendWeight();

}

// UpdateShake로 값을 구하고, 기준 POV 회전으로 위치를 월드 방향으로 변환합니다.
// ShakeScale * BlendWeight * Alpha를 각각 한 번만 곱해 POV에 적용.
void UCameraShakeBase::UpdateAndApplyCameraShake(float DeltaTime, float Alpha, FMinimalViewInfo& InOutPOV)
{
	FCameraShakeUpdateResult Result;
	UpdateShake(DeltaTime, Result);
	const float Weight = ShakeScale * BlendWeight * std::clamp(NonNegative(Alpha), 0.0f, 1.0f);
	if (Weight == 0.0f) return;
	const FVector WorldOffset = InOutPOV.Rotation.Quaternion().RotateVector(Result.LocationOffset);
	InOutPOV.Location += WorldOffset * Weight;
	InOutPOV.Rotation.Pitch += Result.RotationOffset.Pitch * Weight;
	InOutPOV.Rotation.Yaw += Result.RotationOffset.Yaw * Weight;
	InOutPOV.Rotation.Roll += Result.RotationOffset.Roll * Weight;
	InOutPOV.FOV += Result.FOVOffset * Weight;
}

// 즉시 종료 또는 현재 BlendWeight를 저장하고 그 강도부터 페이드 아웃.
void UCameraShakeBase::StopShake(bool bImmediately)
{
	if (bIsFinished) return;
	if (bImmediately || !std::isfinite(BlendOutTime) || BlendOutTime <= 0.0f)
	{
		bIsFinished = true;
		BlendWeight = 0.0f;
		return;
	}
	if (bIsStopping) return; // 반복 Stop이 페이드 종료 시점을 계속 미루지 않습니다.
	bIsStopping = true;
	StopElapsedTime = 0.0f;
	StopStartBlendWeight = BlendWeight;
}

// 시간/종료 상태/BlendWeight 갱신, 각 oscillator 평가. 매번 새 오프셋으로 덮어씁니다.
void UCameraShakeBase::UpdateShake(float DeltaTime, FCameraShakeUpdateResult& OutResult)
{
	OutResult = {};
	if (bIsFinished) return;
	const float step = NonNegative(DeltaTime);
	ElapsedTime += step;
	if (bIsStopping) StopElapsedTime += step;
	BlendWeight = CalculateBlendWeight();
	if ((!bIsStopping && ElapsedTime >= Duration) || (bIsStopping && StopElapsedTime >= BlendOutTime))
	{
		bIsFinished = true;
		BlendWeight = 0.0f;
		return;
	}
	OutResult.LocationOffset = FVector(
		LocationX.Evaluate(ElapsedTime, InitialLocationPhase.X),
		LocationY.Evaluate(ElapsedTime, InitialLocationPhase.Y),
		LocationZ.Evaluate(ElapsedTime, InitialLocationPhase.Z)
	);

	OutResult.RotationOffset = FRotator(
		RotationPitch.Evaluate(ElapsedTime, InitialRotationPhase.X),
		RotationYaw.Evaluate(ElapsedTime, InitialRotationPhase.Y),
		RotationRoll.Evaluate(ElapsedTime, InitialRotationPhase.Z)
	);
	OutResult.FOVOffset = FOV.Evaluate(ElapsedTime, InitialFOVPhase);
}

//  각 채널의 InitialOffsetType에 따라 0 또는 [0, 2*PI) 위상을 한 번 선택. 매 프레임 난수를 뽑음.
void UCameraShakeBase::InitializeOscillatorPhases()
{
	InitialLocationPhase = FVector(InitialPhaseFor(LocationX), InitialPhaseFor(LocationY), InitialPhaseFor(LocationZ));
	InitialRotationPhase = FVector(InitialPhaseFor(RotationPitch), InitialPhaseFor(RotationYaw), InitialPhaseFor(RotationRoll));
	InitialFOVPhase = InitialPhaseFor(FOV);	
}

// 자연 재생은 In/Out 가중치의 최솟값, 중도 정지는 저장 강도 × 남은 페이드 비율을 반환합니다.
float UCameraShakeBase::CalculateBlendWeight() const
{
	const float Out = NonNegative(BlendOutTime);
	if(bIsStopping)
		return Out > 0.0f ? StopStartBlendWeight * std::clamp(1.0f - StopElapsedTime / Out, 0.0f, 1.0f) : 0.0f;
	const float In = NonNegative(BlendInTime);
	const float InWeight = In > 0.0f ? std::clamp(ElapsedTime / In, 0.0f, 1.0f) : 1.0f;
	const float OutWeight = Out > 0.0f ? std::clamp((Duration - ElapsedTime) / Out, 0.0f, 1.0f) : 1.0f;
	return std::min(InWeight, OutWeight);
}
