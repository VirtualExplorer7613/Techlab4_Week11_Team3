#pragma once

#include "AnimNode_Base.h"
#include "Containers/Array.h"

class UAnimSequence;
class UBlendSpace1D;

struct FAnimNode_BlendSpacePlayer : public FAnimNode_Base
{
    void Update(const FAnimationUpdateContext& Context) override;
    void Evaluate(FPoseContext& Output) override;

    double GetDuration() const { return Duration; }
    double GetPhase() const { return Phase; }
    void SetPhase(double InPhase);

    const UBlendSpace1D* BlendSpace = nullptr;
    float Speed = 0.0f;
    bool bLoop = true;

private:
    struct FActiveSample
    {
        const UAnimSequence* Animation;
        double Duration;
        float Weight;
    };

    TArray<FActiveSample> ActiveSamples;
    double Duration = 0.0;
    double Phase = 0.0;
};
