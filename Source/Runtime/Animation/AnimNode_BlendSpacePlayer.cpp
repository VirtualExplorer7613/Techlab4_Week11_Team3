#include "EnginePCH.h"
#include "AnimNode_BlendSpacePlayer.h"

#include "AnimNodeContext.h"
#include "AnimSequence.h"
#include "BlendSpace1D.h"

#include <algorithm>
#include <cmath>

void FAnimNode_BlendSpacePlayer::SetPhase(double InPhase)
{
    Phase = std::isfinite(InPhase) ? std::clamp(InPhase, 0.0, 1.0) : 0.0;
}

void FAnimNode_BlendSpacePlayer::Update(const FAnimationUpdateContext& Context)
{
    ActiveSamples.Reset();
    Duration = 0.0;

    TArray<FBlendSampleWeight> SampleWeights;
    if (!BlendSpace || !BlendSpace->GetSampleWeights(FVector(Speed, 0.0f, 0.0f), SampleWeights))
    {
        return;
    }

    const TArray<FBlendSample>& Samples = BlendSpace->GetSamples();
    for (const FBlendSampleWeight& SampleWeight : SampleWeights)
    {
        const UAnimSequence* Animation = Samples[SampleWeight.SampleIndex].Animation;
        const double SampleDuration = Animation->GetSamplingDuration();
        ActiveSamples.Add(FActiveSample{ Animation, SampleDuration, SampleWeight.Weight });
        // Use a weighted cycle length while preserving the same phase for each clip.
        Duration += SampleDuration * SampleWeight.Weight;
    }

    if (Duration > 0.0 && std::isfinite(Context.DeltaTime) && Context.DeltaTime > 0.0f)
    {
        const double NextPhase = Phase + Context.DeltaTime / Duration;
        Phase = bLoop ? std::fmod(NextPhase, 1.0) : std::min(NextPhase, 1.0);
    }
}

void FAnimNode_BlendSpacePlayer::Evaluate(FPoseContext& Output)
{
    Output.Samples.Reset();
    if (ActiveSamples.IsEmpty())
    {
        return;
    }

    const FActiveSample& SampleA = ActiveSamples[0];
    if (ActiveSamples.Num() == 1)
    {
        SampleA.Animation->SampleLocalPoseAtTime(Phase * SampleA.Duration, Output.Bones, Output.Samples);
        return;
    }

    const FActiveSample& SampleB = ActiveSamples[1];
    FPoseContext PoseA{ Output.Bones };
    FPoseContext PoseB{ Output.Bones };
    SampleA.Animation->SampleLocalPoseAtTime(Phase * SampleA.Duration, Output.Bones, PoseA.Samples);
    SampleB.Animation->SampleLocalPoseAtTime(Phase * SampleB.Duration, Output.Bones, PoseB.Samples);
    Output.BlendTwoPoses(PoseA, PoseB, SampleB.Weight);
}
