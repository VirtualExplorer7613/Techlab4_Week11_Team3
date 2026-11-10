#pragma once

#include "AnimTypes.h"

#include <optional>

struct FBoneInfo;

struct FAnimationUpdateContext
{
    float DeltaTime = 0.0f;
};

struct FPoseContext
{
    const TArray<FBoneInfo>& Bones;
    TArray<std::optional<FLocalBoneTransform>> Samples;

    void BlendTwoPoses(const FPoseContext& PoseA, const FPoseContext& PoseB, float Alpha);
};
