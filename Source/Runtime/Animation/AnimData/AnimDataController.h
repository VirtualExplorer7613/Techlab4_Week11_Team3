#pragma once

#include "UObject/Object.h"
#include "Core/Types.h"
#include "IAnimationDataController.h"
#include "AnimDataController.generated.h"


UCLASS()
class UAnimDataController : public UObject, public IAnimationDataController
{
	GENERATED_BODY()

public:

	// 어느 모델을 수정할 것인가?
	void SetModel(UAnimDataModel* InModel) override;

	// 초당 몇 번의 자세를 저장할 것인가?
	void SetFrameRate(FFrameRate FrameRate) override;

	// 애니메이션에 시간 구간이 몇 개인가?
	bool SetNumberOfFrames(FFrameNumber InNumberOfFrames) override;

	bool ResizeNumberOfFrames(FFrameNumber NewLength, FFrameNumber T0, FFrameNumber T1);

	// 어느 본의 트랙을 만들 것인가?(언리얼의 AddBoneCurve 역할)
	bool AddBoneTrack(const FName& BoneName) override;

	// 해당 본의 위치·회전·크기 키를 입력한다.
	bool SetBoneTrackKeys(const FName& BoneName, const TArray<FVector>& PosKeys, const TArray<FQuat>& RotKeys, const TArray<FVector>& ScaleKeys) override;

private:
	UAnimDataModel* Model = nullptr;
};