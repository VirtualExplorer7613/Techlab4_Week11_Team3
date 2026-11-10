#pragma once

#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

// 위치, 회전, 크기 기록을 표현
struct FRawAnimSequenceTrack
{
    TArray<FVector> PosKeys;
    TArray<FQuat> RotKeys;
    TArray<FVector> ScaleKeys;
};

struct FLocalBoneTransform
{
    static bool TryFromMatrix(const FMatrix& Local, FLocalBoneTransform& OutTransform);

    FVector Position;
    FQuat Rotation;
    FVector Scale;
};
