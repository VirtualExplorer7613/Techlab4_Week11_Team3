#include "EnginePCH.h"
#include "AnimNode_SequencePlayer.h"

#include "AnimNodeContext.h"
#include "AnimSequence.h"

#include <algorithm>
#include <cmath>

double FAnimNode_SequencePlayer::GetDuration() const
{
    return Sequence ? Sequence->GetSamplingDuration() : 0.0;
}

void FAnimNode_SequencePlayer::Update(const FAnimationUpdateContext& Context)
{
    if (!std::isfinite(Context.DeltaTime) || Context.DeltaTime <= 0.0f)
    {
        return;
    }

    const double Duration = GetDuration();
    if (Duration <= 0.0)
    {
        CurrentTime = 0.0;
        return;
    }

    const double StartTime = std::isfinite(CurrentTime) ? std::max(0.0, CurrentTime) : 0.0;
    const double NextTime = StartTime + Context.DeltaTime;
    CurrentTime = bLoop && NextTime >= Duration ? std::fmod(NextTime, Duration) : std::min(NextTime, Duration);
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
{
    if (!Sequence)
    {
        Output.Samples.Reset();
        return;
    }

    Sequence->SampleLocalPoseAtTime(CurrentTime, Output.Bones, Output.Samples);
}
