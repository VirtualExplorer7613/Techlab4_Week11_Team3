#pragma once

#include "Core/Types.h"
#include "Core/NameTypes.h"
#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "MovieScene/MovieSceneFrameTime.h"
#include "AnimDataModel.h"

class IAnimationDataModel;

class IAnimationDataController
{
public:
	virtual ~IAnimationDataController() = default;

	// 어느 모델을 수정할 것인가?
	virtual void SetModel(UAnimDataModel* InModel) = 0;

	// 초당 몇 번의 자세를 저장할 것인가?
	virtual void SetFrameRate(FFrameRate FrameRate) = 0;

	// 애니메이션에 시간 구간이 몇 개인가?
	virtual bool SetNumberOfFrames(FFrameNumber InNumberOfFrames) = 0;

	// 어느 본의 트랙을 만들 것인가?(언리얼의 AddBoneCurve 역할)
	virtual bool AddBoneTrack(const FName& BoneName) = 0;

	// 해당 본의 위치·회전·크기 키를 입력한다.
	virtual bool SetBoneTrackKeys(const FName& BoneName, const TArray<FVector>& PosKeys, const TArray<FQuat>& RotKeys, const TArray<FVector>& ScaleKeys) = 0;

};
