#pragma once

#include "AnimSequenceBase.h"
#include "AnimSequence.generated.h"

struct FBoneInfo;
struct FMatrix;

UCLASS()
class UAnimSequence : public UAnimSequenceBase
{
	GENERATED_BODY()

public:
	UAnimSequence() = default;

	// 메시의 본 순서로 부모 기준 로컬 자세를 계산한다.
	bool EvaluatePoseAtTime(double TimeSeconds, const TArray<FBoneInfo>& Bones, TArray<FMatrix>& OutLocalPose) const;
};
