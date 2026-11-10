#pragma once

#include "UObject/Object.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimDataModel.generated.h"

// 실제 배열과 시간 정보를 소유하고, 인터페이스의 조회 함수를 구현
UCLASS()
class UAnimDataModel : public UObject, public IAnimationDataModel
{
    GENERATED_BODY()

public:
    friend class UAnimDataController;

    const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const override
    {
        return BoneAnimationTracks;
    }

    float GetPlayLength() const { return PlayLength; }
    FFrameRate GetFrameRate() const { return FrameRate; }
    FFrameNumber GetNumberOfFrames() const { return NumberOfFrames; }
    int32 GetNumberOfKeys() const { return NumberOfKeys; }

private:
    TArray<FBoneAnimationTrack> BoneAnimationTracks;

    float PlayLength = 0.0f;
    FFrameRate FrameRate{ 30, 1 };
    FFrameNumber NumberOfFrames{0};
    int32 NumberOfKeys = 0;
};