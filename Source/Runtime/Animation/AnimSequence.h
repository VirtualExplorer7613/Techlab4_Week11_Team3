#pragma once

#include "AnimSequenceBase.h"
#include "AnimTypes.h"
#include <optional>
#include "AnimSequence.generated.h"

struct FBoneInfo;
struct FMatrix;

UCLASS()
class UAnimSequence : public UAnimSequenceBase
{
	GENERATED_BODY()

public:
	UAnimSequence() = default;
	double GetSamplingDuration() const;

	// 메시의 본 순서로 부모 기준 로컬 자세를 계산한다.
	bool EvaluatePoseAtTime(double TimeSeconds, const TArray<FBoneInfo>& Bones, TArray<FMatrix>& OutLocalPose) const;
	bool SampleLocalPoseAtTime(double TimeSeconds, const TArray<FBoneInfo>& Bones, TArray<std::optional<FLocalBoneTransform>>& OutSamples) const;

	// 메시의 본 순서로 로컬 행렬을 조립한다. 샘플이 없는 본은 레퍼런스 자세를 사용한다.
	static bool BuildLocalPoseMatrices(const TArray<FBoneInfo>& Bones,
		const TArray<std::optional<FLocalBoneTransform>>& Samples, TArray<FMatrix>& OutLocalPose);
};
