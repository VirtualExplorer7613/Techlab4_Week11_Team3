#include "EnginePCH.h"
#include "AnimNode_TwoWayBlend.h"

#include "AnimNodeContext.h"
#include "Math/EngineMath.h"

void FAnimNode_TwoWayBlend::Update(const FAnimationUpdateContext& Context)
{
    if (A.Node)
    {
        A.Node->Update(Context);
    }
    if (B.Node && B.Node != A.Node)
    {
        B.Node->Update(Context);
    }
}

void FAnimNode_TwoWayBlend::Evaluate(FPoseContext& Output)
{
    Output.Samples.Reset();
    if (!A.Node || !B.Node)
    {
        return;
    }

    const float BlendAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
    if (BlendAlpha == 0.0f)
    {
        A.Node->Evaluate(Output);
        return;
    }
    if (BlendAlpha == 1.0f)
    {
        B.Node->Evaluate(Output);
        return;
    }

    FPoseContext PoseA{ Output.Bones };
    FPoseContext PoseB{ Output.Bones };

    A.Node->Evaluate(PoseA);
    B.Node->Evaluate(PoseB);
    Output.BlendTwoPoses(PoseA, PoseB, BlendAlpha);
}
