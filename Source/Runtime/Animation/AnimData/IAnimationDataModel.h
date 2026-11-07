#pragma once

#include "Animation/AnimTypes.h"
#include "Core/NameTypes.h"
#include "MovieScene/MovieSceneFrameTime.h"

struct FBoneAnimationTrack
{
    FName Name;
    FRawAnimSequenceTrack InternalTrack;
};

class IAnimationDataModel
{
public:
    virtual ~IAnimationDataModel() = default;

    virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const = 0;

    virtual float GetPlayLength() const = 0;
    virtual FFrameRate GetFrameRate() const = 0;
    virtual FFrameNumber GetNumberOfFrames() const = 0;
    virtual int32 GetNumberOfKeys() const = 0;
};