#pragma once

struct FAnimationUpdateContext;
struct FPoseContext;

struct FAnimNode_Base
{
    virtual ~FAnimNode_Base() = default;

    virtual void Update(const FAnimationUpdateContext&) {}
    virtual void Evaluate(FPoseContext& Output) = 0;
};